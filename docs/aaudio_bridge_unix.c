// aaudio_bridge_unix.c：PulseAudio simple-protocol(UNIX) -> AAudio
//
// 数据链路：
//   容器应用 -> PulseAudio null sink 混音 -> null sink monitor
//   -> module-simple-protocol-unix -> UNIX stream socket -> 本进程 -> AAudio
//
// 设计约束：
//  1. UNIX socket 是字节流，一次 read 不保证按 PCM 帧对齐，必须保留尾字节。
//  2. socket 非阻塞读取；AAudio 回调拿不到完整帧时补静音，不在音频线程等待。
//  3. PulseAudio simple-protocol 的录制队列上限为 5 秒，避免无限堆积旧音频。
//  4. socket 断开、AAudio 流错误都由主线程重建，回调里不做 close/connect。
//  5. 统计区分实际收到的 PCM、其中的非零 PCM、回调补的静音。
//
// 用法：aaudio_bridge_unix <宿主侧socket路径> [rate] [channels] [idle_ms]
// 例：aaudio_bridge_unix /data/linuxroot/tmp/runtime-root/pulse/aaudio-bridge.sock 48000 2 30000
// 编译：clang -O2 -std=c11 -o aaudio_bridge_unix aaudio_bridge_unix.c -ldl

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/un.h>

typedef struct AAudioStreamBuilder AAudioStreamBuilder;
typedef struct AAudioStream AAudioStream;
typedef int32_t aaudio_result_t;
typedef int32_t aaudio_data_callback_result_t;

#define AAUDIO_OK 0
#define AAUDIO_DIRECTION_OUTPUT 0
#define AAUDIO_FORMAT_PCM_I16 1
#define AAUDIO_PERFORMANCE_MODE_NONE 10
#define AAUDIO_CALLBACK_RESULT_CONTINUE 0
#define AAUDIO_CALLBACK_RESULT_STOP 2
#define DEFAULT_IDLE_CLOSE_MS 30000
#define MAX_STREAM_REOPEN_BACKOFF_MS 1000

static void *g_aaudio;

static aaudio_result_t (*p_createBuilder)(AAudioStreamBuilder **);
static void (*p_setDirection)(AAudioStreamBuilder *, int32_t);
static void (*p_setRate)(AAudioStreamBuilder *, int32_t);
static void (*p_setChannels)(AAudioStreamBuilder *, int32_t);
static void (*p_setFormat)(AAudioStreamBuilder *, int32_t);
static void (*p_setPerf)(AAudioStreamBuilder *, int32_t);
static void (*p_setDataCallback)(AAudioStreamBuilder *,
                                 aaudio_data_callback_result_t (*)(AAudioStream *, void *, void *, int32_t),
                                 void *);
static void (*p_setErrorCallback)(AAudioStreamBuilder *,
                                  void (*)(AAudioStream *, void *, aaudio_result_t),
                                  void *);
static aaudio_result_t (*p_openStream)(AAudioStreamBuilder *, AAudioStream **);
static void (*p_deleteBuilder)(AAudioStreamBuilder *);
static aaudio_result_t (*p_requestStart)(AAudioStream *);
static aaudio_result_t (*p_requestStop)(AAudioStream *);
static aaudio_result_t (*p_close)(AAudioStream *);
static int32_t (*p_getRate)(AAudioStream *);
static int32_t (*p_getChannels)(AAudioStream *);
static aaudio_result_t (*p_setBufCap)(AAudioStreamBuilder *, int32_t);
static aaudio_result_t (*p_setBufSize)(AAudioStreamBuilder *, int32_t);
static int32_t (*p_getXRun)(AAudioStream *);

static int g_rate = 48000;
static int g_ch = 2;
static int g_frame_bytes = 4;
static int g_idle_ms = DEFAULT_IDLE_CLOSE_MS;
static int g_fd = -1;
static const char *g_socket_path;

/* 仅 AAudio 数据回调访问；主线程只在流停止后清零。 */
static uint8_t *g_carry;
static size_t g_carry_len;

static atomic_int g_io_dead;       /* 0=正常，1=EOF，其他正值=errno */
static atomic_int g_stream_error;  /* AAudio error callback 返回的错误码 */
static atomic_llong g_last_data_ms;     /* 最近一次收到任何 PCM 的时间 */
static atomic_llong g_last_nonzero_ms;  /* 最近一次收到非零 PCM 的时间，idle 以此为准 */
static atomic_llong g_pcm_frames;
static atomic_llong g_nonzero_frames;
static atomic_llong g_pad_frames;

static long long g_dbg_last;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void logmsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[bridge-unix] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(ap);
}

static void fill_silence(void *audio_data, size_t bytes, int32_t num_frames) {
    memset(audio_data, 0, bytes);
    atomic_fetch_add_explicit(&g_pad_frames, num_frames, memory_order_relaxed);
}

static long long count_nonzero_frames(const int16_t *samples, size_t frames) {
    size_t i;
    int ch;
    long long nonzero = 0;
    for (i = 0; i < frames; ++i) {
        for (ch = 0; ch < g_ch; ++ch) {
            if (samples[i * (size_t)g_ch + (size_t)ch] != 0) {
                ++nonzero;
                break;
            }
        }
    }
    return nonzero;
}

/* AAudio 音频线程：从 UNIX 字节流取完整 PCM 帧，不足部分补静音。 */
static aaudio_data_callback_result_t data_cb(AAudioStream *stream, void *user,
                                             void *audio_data, int32_t num_frames) {
    (void)stream;
    (void)user;

    size_t wanted;
    size_t total;
    size_t complete;
    size_t remainder;
    ssize_t n;

    if (num_frames <= 0)
        return AAUDIO_CALLBACK_RESULT_CONTINUE;

    wanted = (size_t)num_frames * (size_t)g_frame_bytes;

    if (g_fd < 0 || atomic_load_explicit(&g_io_dead, memory_order_acquire) != 0) {
        fill_silence(audio_data, wanted, num_frames);
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    /* 上次遗留的不足一帧的字节必须放在本次输出最前面。 */
    memcpy(audio_data, g_carry, g_carry_len);
    n = read(g_fd, (uint8_t *)audio_data + g_carry_len, wanted - g_carry_len);

    if (n > 0) {
        total = g_carry_len + (size_t)n;
    } else {
        total = g_carry_len;
        if (n == 0) {
            atomic_store_explicit(&g_io_dead, 1, memory_order_release);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            atomic_store_explicit(&g_io_dead, errno, memory_order_release);
        }
    }

    complete = total - total % (size_t)g_frame_bytes;
    remainder = total - complete;

    if (remainder)
        memcpy(g_carry, (const uint8_t *)audio_data + complete, remainder);
    g_carry_len = remainder;

    if (complete < wanted)
        memset((uint8_t *)audio_data + complete, 0, wanted - complete);

    if (complete > 0) {
        size_t frames = complete / (size_t)g_frame_bytes;
        long long nonzero = count_nonzero_frames((const int16_t *)audio_data, frames);
        atomic_fetch_add_explicit(&g_pcm_frames, (long long)frames, memory_order_relaxed);
        if (nonzero > 0) {
            atomic_fetch_add_explicit(&g_nonzero_frames, nonzero, memory_order_relaxed);
            atomic_store_explicit(&g_last_nonzero_ms, now_ms(), memory_order_release);
        }
        atomic_store_explicit(&g_last_data_ms, now_ms(), memory_order_release);
    }

    if (complete < wanted) {
        size_t missing = wanted - complete;
        atomic_fetch_add_explicit(&g_pad_frames,
                                  (long long)(missing / (size_t)g_frame_bytes),
                                  memory_order_relaxed);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

/* AAudio 错误线程：只置标志，禁止在这里 close 或重建流。 */
static void error_cb(AAudioStream *stream, void *user, aaudio_result_t error) {
    (void)stream;
    (void)user;
    atomic_store_explicit(&g_stream_error, error, memory_order_release);
}

static int load_symbols(void) {
    g_aaudio = dlopen("libaaudio.so", RTLD_NOW);
    if (!g_aaudio) {
        logmsg("dlopen libaaudio.so: %s", dlerror());
        return -1;
    }
#define LOAD_REQUIRED(sym, name) do { \
    sym = dlsym(g_aaudio, name); \
    if (!sym) { logmsg("missing %s", name); return -1; } \
} while (0)
    LOAD_REQUIRED(p_createBuilder, "AAudio_createStreamBuilder");
    LOAD_REQUIRED(p_setDirection, "AAudioStreamBuilder_setDirection");
    LOAD_REQUIRED(p_setRate, "AAudioStreamBuilder_setSampleRate");
    LOAD_REQUIRED(p_setChannels, "AAudioStreamBuilder_setChannelCount");
    LOAD_REQUIRED(p_setFormat, "AAudioStreamBuilder_setFormat");
    LOAD_REQUIRED(p_setPerf, "AAudioStreamBuilder_setPerformanceMode");
    LOAD_REQUIRED(p_setDataCallback, "AAudioStreamBuilder_setDataCallback");
    LOAD_REQUIRED(p_setErrorCallback, "AAudioStreamBuilder_setErrorCallback");
    LOAD_REQUIRED(p_openStream, "AAudioStreamBuilder_openStream");
    LOAD_REQUIRED(p_deleteBuilder, "AAudioStreamBuilder_delete");
    LOAD_REQUIRED(p_requestStart, "AAudioStream_requestStart");
    LOAD_REQUIRED(p_requestStop, "AAudioStream_requestStop");
    LOAD_REQUIRED(p_close, "AAudioStream_close");
    LOAD_REQUIRED(p_getRate, "AAudioStream_getSampleRate");
    LOAD_REQUIRED(p_getChannels, "AAudioStream_getChannelCount");
#undef LOAD_REQUIRED
    p_setBufCap = dlsym(g_aaudio, "AAudioStreamBuilder_setBufferCapacityInFrames");
    p_setBufSize = dlsym(g_aaudio, "AAudioStreamBuilder_setBufferSizeInFrames");
    p_getXRun = dlsym(g_aaudio, "AAudioStream_getXRunCount");
    return 0;
}

static AAudioStream *open_stream(void) {
    AAudioStreamBuilder *builder = NULL;
    AAudioStream *stream = NULL;
    aaudio_result_t result;

    if (p_createBuilder(&builder) != AAUDIO_OK) {
        logmsg("createStreamBuilder failed");
        return NULL;
    }
    p_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    p_setRate(builder, g_rate);
    p_setChannels(builder, g_ch);
    p_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    p_setPerf(builder, AAUDIO_PERFORMANCE_MODE_NONE);
    p_setDataCallback(builder, data_cb, NULL);
    p_setErrorCallback(builder, error_cb, NULL);
    /* 缓冲给足：2048/4096 实测导致 AudioFlinger underruns=40（明显卡顿），
     * 而延迟并没有因此变短，说明 1 秒延迟来自链路本身而不是这里。 */
    if (p_setBufCap)
        p_setBufCap(builder, 16384);
    if (p_setBufSize)
        p_setBufSize(builder, 8192);

    result = p_openStream(builder, &stream);
    p_deleteBuilder(builder);
    if (result != AAUDIO_OK) {
        logmsg("openStream: %d", result);
        return NULL;
    }

    atomic_store_explicit(&g_stream_error, 0, memory_order_release);
    atomic_store_explicit(&g_pcm_frames, 0, memory_order_relaxed);
    atomic_store_explicit(&g_nonzero_frames, 0, memory_order_relaxed);
    atomic_store_explicit(&g_pad_frames, 0, memory_order_relaxed);
    result = p_requestStart(stream);
    if (result != AAUDIO_OK) {
        logmsg("requestStart: %d", result);
        p_close(stream);
        return NULL;
    }

    atomic_store_explicit(&g_last_data_ms, now_ms(), memory_order_release);
    atomic_store_explicit(&g_last_nonzero_ms, now_ms(), memory_order_release);
    logmsg("stream up: %d Hz %d ch", p_getRate(stream), p_getChannels(stream));
    return stream;
}

static void close_stream(AAudioStream **stream, const char *why) {
    if (!*stream)
        return;
    p_requestStop(*stream);
    p_close(*stream);
    *stream = NULL;
    logmsg("stream closed (%s)", why);
}

static int connect_unix_socket(const char *path) {
    struct sockaddr_un address;
    socklen_t address_len;
    int fd;
    int flags;
    int rcvbuf = 64 * 1024;

    if (strlen(path) >= sizeof(address.sun_path)) {
        logmsg("socket path too long");
        return -1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);
    address_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + strlen(path) + 1);

    if (connect(fd, (struct sockaddr *)&address, address_len) < 0) {
        close(fd);
        return -1;
    }

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }

    /* 限制内核侧额外积压。实测 16KB 会让上游持续缺空间，
     * 回滚到 64KB：延迟并没有因此变短，说明瓶颈不在这一层。 */
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    return fd;
}

static void close_socket(void) {
    if (g_fd >= 0)
        close(g_fd);
    g_fd = -1;
    g_carry_len = 0;
    atomic_store_explicit(&g_io_dead, 0, memory_order_release);
}

/* 流关闭期间排空 socket，避免旧数据堆积；顺带判断是否出现非零 PCM。
 * 返回 1 表示找到了非零样本（应当开流），0 表示全是静音，-1 表示连接已断。 */
static int drain_and_detect(void) {
    uint8_t buffer[8192];
    int found = 0;
    size_t total = 0;

    for (;;) {
        ssize_t n = read(g_fd, buffer, sizeof(buffer));
        size_t i;

        if (n > 0) {
            total += (size_t)n;
            for (i = 0; i + 1 < (size_t)n; i += 2) {
                if (buffer[i] != 0 || buffer[i + 1] != 0) {
                    found = 1;
                    break;
                }
            }
            /* 即使已经发现非零也继续读到 EAGAIN，保持队列为空。 */
            continue;
        }

        if (n == 0) {
            atomic_store_explicit(&g_io_dead, 1, memory_order_release);
            return -1;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            long long t = now_ms();
            if (found && t - g_dbg_last >= 2000) {
                g_dbg_last = t;
                logmsg("drain: bytes=%zu nonzero=1 -> opening stream", total);
            }
            return found;
        }
        atomic_store_explicit(&g_io_dead, errno, memory_order_release);
        return -1;
    }
}

static void print_stats(AAudioStream *stream, double seconds) {
    long long pcm = atomic_exchange_explicit(&g_pcm_frames, 0, memory_order_relaxed);
    long long nonzero = atomic_exchange_explicit(&g_nonzero_frames, 0, memory_order_relaxed);
    long long pad = atomic_exchange_explicit(&g_pad_frames, 0, memory_order_relaxed);
    double expected = seconds * (double)g_rate;
    logmsg("stat %.0fs: pcm=%lld nonzero=%lld pad=%lld (%.1f%%) xrun=%d",
           seconds, pcm, nonzero, pad,
           expected > 0 ? (double)pad * 100.0 / expected : 0.0,
           (stream && p_getXRun) ? p_getXRun(stream) : -1);
}

int main(int argc, char **argv) {
    AAudioStream *stream = NULL;
    long long stat_last;
    int stream_failures = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <unix-socket> [rate] [channels] [idle_ms]\n", argv[0]);
        return 1;
    }
    g_socket_path = argv[1];
    if (argc > 2)
        g_rate = atoi(argv[2]);
    if (argc > 3)
        g_ch = atoi(argv[3]);
    if (argc > 4)
        g_idle_ms = atoi(argv[4]);
    if (g_rate <= 0 || g_ch <= 0 || g_ch > 16 || g_idle_ms < 0) {
        fprintf(stderr, "invalid rate/channels/idle_ms\n");
        return 1;
    }
    g_frame_bytes = g_ch * 2;
    g_carry = calloc(1, (size_t)g_frame_bytes);
    if (!g_carry)
        return 1;

    signal(SIGPIPE, SIG_IGN);
    if (load_symbols() != 0)
        return 2;

    atomic_store(&g_last_data_ms, now_ms());
    stat_last = now_ms();
    logmsg("started: socket=%s rate=%d ch=%d idle_ms=%d",
           g_socket_path, g_rate, g_ch, g_idle_ms);

    for (;;) {
        long long now = now_ms();
        int io_dead = atomic_load_explicit(&g_io_dead, memory_order_acquire);
        int stream_error = atomic_exchange_explicit(&g_stream_error, 0, memory_order_acq_rel);

        if (stream_error != 0) {
            close_stream(&stream, "AAudio error callback");
            usleep(200 * 1000);
            continue;
        }

        if (io_dead != 0) {
            close_stream(&stream, io_dead == 1 ? "socket EOF" : "socket read error");
            close_socket();
            usleep(100 * 1000);
            continue;
        }

        if (g_fd < 0) {
            close_stream(&stream, "socket reconnect");
            g_fd = connect_unix_socket(g_socket_path);
            if (g_fd < 0) {
                usleep(500 * 1000);
                continue;
            }
            logmsg("socket connected");
            g_carry_len = 0;
            stat_last = now_ms();
        }

        if (stream && g_idle_ms > 0 && now - atomic_load(&g_last_nonzero_ms) > g_idle_ms)
            close_stream(&stream, "idle (all silence)");

        if (!stream) {
            int ready = g_idle_ms == 0 ? 1 : drain_and_detect();

            if (ready < 0) {
                close_socket();
                usleep(100 * 1000);
                continue;
            }

            if (ready > 0) {
                g_carry_len = 0;
                stream = open_stream();
                if (!stream) {
                    ++stream_failures;
                    int backoff = stream_failures * 100;
                    if (backoff > MAX_STREAM_REOPEN_BACKOFF_MS)
                        backoff = MAX_STREAM_REOPEN_BACKOFF_MS;
                    usleep((useconds_t)backoff * 1000);
                    continue;
                }
                stream_failures = 0;
                stat_last = now_ms();
            }
        }

        now = now_ms();
        if (stream && now - stat_last >= 5000) {
            print_stats(stream, (double)(now - stat_last) / 1000.0);
            stat_last = now;
        }

        usleep(100 * 1000);
    }

    return 0;
}
