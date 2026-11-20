# 安卓手机上的 Ubuntu 桌面（chroot + Termux:X11）

把 Ubuntu 24.04 arm64 的根文件系统解到 `/data/linuxroot`，用 `chroot` 进去跑 KDE Plasma。画面不走 VNC，也不走 RDP，而是用 Termux:X11 的 Xlorie 直接画到 Android 的 Surface 上。x86 程序靠 box64 加 Wine 跑。

全程只碰 `/data`，不写任何会被 BL 锁校验的系统分区。未解 BL 锁的内存态 root 也能做。

作者：Rxinns、[AI]Deepseek、[AI]Kimi

---

## 怎么用这份东西

主体是 `docs/SKILL.md`，约 4800 行，既是给人看的教程，也是给 AI agent 用的技能文件。

- **人**：照着 A 层走，能点亮桌面；再按 B 层一节一节补。每节先写现象，再给唯一有效的办法，最后附「试过、没用」的路子，省得重复踩。C 层是案例库，跳过不影响使用。
- **AI agent**：`docs/SKILL.md` 是标准技能格式，开头带 `name` / `description` 的 frontmatter，description 里塞了触发词（「装个 Ubuntu 桌面」「容器没声音」「exe 闪退」「AAudio 桥」之类）。装进 RikkaHub 这类支持技能的客户端后，聊天里提到这些词就会自动加载对应章节。

装成技能有两条路：

1. 客户端里选「从 URL 安装技能」，填
   `https://raw.githubusercontent.com/RxinnotRstar/skill-install-chroot-android-phone/main/docs/SKILL.md`
2. 手动下载 `docs/SKILL.md`，放到技能目录下 `howto-install-chroot-in-phone/SKILL.md`。目录名要和 frontmatter 里的 `name` 一致，否则不被识别。放完要去设置里启用，光有文件等于没装。

---

## 先说清楚它是什么

它不是虚拟机，也不是「和安卓并列的第二套系统」。同一个内核，只换根目录。安卓的 `system_server` 和容器里的 `plasmashell` 在同一张 PID 表里，`ps -ef` 能互相看见，进程、网络、IPC 全部共享。

所以：

- 没有「重启 Linux」这回事。内核是安卓的，重启就是重启手机。
- 它不独立于安卓。抢同一份 CPU 和内存，安卓卡它就卡。
- 唯一的好处是 root 身份、父进程挂在 init 底下的进程，能躲过后台清理，适合挂长时间跑的小工具。
- 重启后一切内存态的东西都没了（挂载、X server、桌面、音频），要重新点一次启动。

---

## 实测环境

| 项目 | 值 |
|---|---|
| 机型 | 红米 Note12 Turbo（marble） |
| 芯片 | 骁龙 7+ Gen2 / Adreno 725 |
| 系统 | MIUI 14.0.27 / Android 13 |
| 内核 | 5.10.149 |
| root | KernelSU（越狱模式，内存态，不写系统分区） |
| 磁盘 | 939 GB，装完约剩 21 GB |

换机器的移植成本分三类：

- **机型无关**：装 rootfs、挂载、chroot 模板、apt 多架构、KDE、KSU 综合管理器。任意 arm64 安卓照抄。
- **高通专属**：GPU 那节只对 Adreno + kgsl 成立（判据是 `/dev/kgsl-3d0` 能 open）。联发科 Mali 是另一套，跳过 GPU 也照样能点亮桌面。
- **ROM 相关**：MIUI 冻结白名单那两条命令在原生 Android 上会报错，跳过即可。

32 位 arm 设备不适用，镜像和包名整套都不一样。

---

## 前置条件

三个硬前提，缺了整套流程走不完：

1. **真 root**（uid=0）。uid=2000 的 shell 执行 `chroot` 直接 EPERM，没有替代路径。
2. **一个能直接画屏的 X server 方案**。本文用 Termux:X11 的 Xlorie；没有它就得换 VNC / Xvfb 那一套（本文不覆盖）。
3. **`/data` 至少留 12 GB**。解压 base rootfs 只占 101 MB，装完 KDE 和一堆工具涨到 4 到 6 GB，把 Mesa 和 box64 的源码算进去峰值 8 到 10 GB。

可选依赖（缺了不是必须补，是可以整段跳过）：

| 依赖 | 用来做什么 | 没有怎么办 |
|---|---|---|
| Termux | 提供 X11 socket 目录、模型文件，偶尔借它的 python3 / xz / tar | socket 目录换 `/data/local/tmp`；解压改在容器里做 |
| Termux:X11 | 唯一的画面出口 | 换 VNC 方案，显示层那节整节不适用 |
| KernelSU 3.x | 综合管理器的 WebUI（`ksu.exec` 注入对象） | 用文档里那份手工启动脚本，功能一样，只是没图形界面 |

---

## 文档导航

完整手册在 [`docs/SKILL.md`](docs/SKILL.md)，约 4800 行。按「做完能得到什么」分三层，每一层都能单独收工：

### A：从空盘到点亮桌面

做完的标准是：点开 Termux:X11 看到 KDE 桌面，能开终端、能拖窗口、能跑 Linux 程序。

| 节 | 内容 |
|---|---|
| a0 | 执行环境与路径（先读，不然每条命令都会敲进错的窗口） |
| a1 | 前置检查 |
| a2 | 装 rootfs |
| a3 | 挂载与 chroot 的统一姿势 |
| a4 | apt 源与 amd64 多架构 |
| a5 | KDE Plasma |
| a6 | 显示层 Termux:X11（Xlorie） |
| a7 | chroot 综合管理器（KSU 模块 + WebUI） |
| a8 | A 部分验收 |

### B：让它勉强能日常用

| 节 | 内容 |
|---|---|
| b1 | GPU（Vulkan Turnip + OpenGL zink） |
| b2 | box64 |
| b3 | Wine 跑 exe |
| b4 | 中文（locale、字体、界面语言） |
| b5 | exe 双击运行（文件关联） |
| b6 | exe 图标与缩略图 |
| b7 | 窗口尺寸与显示适配 |
| b8 | 性能调优 |
| b9 | 跑现代 .NET 应用 |
| b10 | 常驻服务与守护 |
| b11 | 音频（让容器出声） |
| b12 | 排查工具箱（含 minidump 解析） |
| b13 | 收尾清理 |
| b14 | 常用软件（看图、看视频、听歌、上网） |
| b15 | Windows 程序的文件关联自动同步 |

### C：作者自己的安装记录（可选）

改第三方应用的 SQLite 配置、给 .NET 服务配 Azure 权限并部署、国内网盘抠文件、给网页版服务做桌面图标、X11 在后台时怎么截屏。不参与「能不能用」的判定，跳过完全没问题。

---

## 快速开始（A 层最短路径）

下面只是骨架，每步的坑和判断条件都在手册对应小节里。

### 0. 体检

```sh
{
  echo "root      : $(id -u) $(cat /proc/self/attr/current)"
  echo "机型      : $(getprop ro.product.model) / $(getprop ro.board.platform) / Android $(getprop ro.build.version.release)"
  echo "架构      : $(uname -m)"
  echo "kgsl      : $([ -e /dev/kgsl-3d0 ] && echo 有 || echo 无)   # 无就跳过 GPU"
  echo "data      : $(df -h /data 2>/dev/null | tail -1 | awk '{print $4" 可用 / "$5" 已用"}')"
  echo "termux-x11: $(pm list packages 2>/dev/null | grep -c 'com.termux.x11')"
  echo "ksu       : $([ -x /data/adb/ksu/bin/ksud ] && echo 有 || echo 无)"
}
```

结果决定范围：`kgsl` 无就整节跳过 GPU；`termux-x11` 为 0 就得换显示方案；`ksu` 无就只用手工脚本。

### 1. 装 rootfs

用官方 ubuntu-base（最小根文件系统，不带安装器），不要 docker 镜像。

```sh
cd /sdcard/Download
curl -L -o ubuntu-base-arm64.tar.gz \
  "https://mirrors.tuna.tsinghua.edu.cn/ubuntu-cdimage/ubuntu-base/releases/24.04/release/ubuntu-base-24.04.5-base-arm64.tar.gz"

mkdir -p /data/linuxroot
tar -xzf /sdcard/Download/ubuntu-base-arm64.tar.gz -C /data/linuxroot
```

放 `/data/linuxroot`，不要放 `/data/local/tmp`（后者 OTA 偶尔会清）。解压前先看一眼目录是不是空的，覆盖同名文件会把之前装的东西吃掉。

### 2. 挂载

```sh
M=/data/linuxroot
mkdir -p $M/proc $M/sys $M/dev/pts $M/dev/shm $M/sdcard $M/tmp
mount -t proc proc $M/proc
mount --bind /sys $M/sys
mount --bind /dev $M/dev
mount --bind /dev/pts $M/dev/pts
mount -t tmpfs -o size=128M tmpfs $M/dev/shm
mount --bind /storage/emulated/0 $M/sdcard
echo "nameserver 223.5.5.5" > $M/etc/resolv.conf
```

判重不要用 `mountpoint -q`：源和目标在同一个 `st_dev` 时它认为「没挂载」，于是每执行一次就多叠一层。实测把 `.X11-unix` 叠到过 16 层。用 `awk '$2 == d' /proc/mounts` 比对。

### 3. 进容器

```sh
chroot $M /usr/bin/env -i \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  HOME=/root TERM=xterm TMPDIR=/tmp \
  /bin/bash -c 'echo ok; uname -a'
```

`env -i` 不能省。shizuku 会往 chroot 里传一个容器里不存在的 `TMPDIR`，症状是 `ls: command not found`、apt 的 postinst 报 `mktemp: failed to create file`，看着完全不像环境变量问题。

### 4. apt 源

24.04 默认的 deb822 源必须删掉，然后 arm64 走 `ubuntu-ports`，amd64 走 `ubuntu`，每行都要带 `[arch=xxx]`：

```sh
rm -f $M/etc/apt/sources.list.d/ubuntu.sources
cat > $M/etc/apt/sources.list << 'EOF'
deb [arch=arm64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ noble main restricted universe multiverse
deb [arch=arm64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ noble-updates main restricted universe multiverse
deb [arch=amd64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble main restricted universe multiverse
deb [arch=amd64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble-updates main restricted universe multiverse
EOF

chroot $M /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root TMPDIR=/tmp \
  DEBIAN_FRONTEND=noninteractive /bin/bash -c 'dpkg --add-architecture amd64; apt-get update'
```

### 5. KDE

```sh
chroot $M /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin \
  DEBIAN_FRONTEND=noninteractive HOME=/root TMPDIR=/tmp /bin/bash -c '
  apt-get install -y --no-install-recommends \
    plasma-desktop plasma-workspace kwin-x11 konsole dolphin systemsettings \
    breeze breeze-icon-theme fonts-noto-cjk fonts-wqy-zenhei fonts-wqy-microhei fonts-droid-fallback \
    plasma-workspace-wallpapers xdg-utils shared-mime-info \
    x11-utils scrot xdotool dbus-x11 ca-certificates'
```

约 549 个包，清华源 4 分钟左右。装完写三个配置：

- `/root/.config/kwinrc`：`Enabled=false`（KWin 合成必须关，软件 GLX 不支持 `glSwapInterval`，开了会 SIGILL 循环崩溃）
- `/root/.config/kscreenlockerrc`：`Autolock=false`
- `/root/.config/kdeglobals`：`ForceFontDPI=160`（按 1080 宽调的，换机器自己改）

### 6. 显示层

核心认知只有一句：**X server 不需要开 App。**

`termux-x11` 不是「在 App 进程里跑 X server」，而是用 `/system/bin/app_process` 加载 `loader.apk` 里的 Loader 类，反射调进 `libXlorie.so`。用 root 跑这条命令，X server 就是一个独立的 root 进程，`am force-stop com.termux.x11` 都杀不掉它。App 只是显示器，X server 是显卡。

```sh
# 从 termux-x11-nightly 的 deb 里抠出 loader.apk（3347 字节，权限必须 644）
CLASSPATH=$XAPK \
XKB_CONFIG_ROOT=$M/usr/share/X11/xkb \
TMPDIR=$XTMP \
setsid /system/bin/app_process -Xnoimage-dex2oat / \
    --nice-name="termux-x11 :0" com.termux.x11.Loader :0 \
    > $M/tmp/tx11.log 2>&1 < /dev/null &

# socket 接进容器（bind 整个目录，不能 bind 单个 X0 文件）
mount --bind $XTMP/.X11-unix $M/tmp/.X11-unix
```

然后在容器里拉 `kwin_x11` 和 `plasmashell`（不要用 `dbus-launch --exit-with-session startplasma-x11`，那个组合在实测机上会静默失败）：

```sh
chroot $M /usr/bin/env -i \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  HOME=/root DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
  LANG=zh_CN.UTF-8 LANGUAGE=zh_CN:en TMPDIR=/tmp SHELL=/bin/bash \
  QT_QUICK_BACKEND=software KWIN_COMPOSE=0 \
  /bin/bash -c '
    mkdir -p /tmp/runtime-root && chmod 700 /tmp/runtime-root
    setsid kwin_x11 > /tmp/kwin.log 2>&1 < /dev/null &
    sleep 3
    setsid plasmashell > /tmp/pshell.log 2>&1 < /dev/null &
  '
```

验证连的是 Xlorie 而不是 Xvfb：`xdpyinfo` 报 `X.Org version: 21.1.99`（Xvfb 是 21.1.11）。

### 7. 日常入口

A 层做完之后，建议装一个 KSU 模块当控制台：`/data/adb/modules/chroot_manager/`，里面 `webroot/index.html` 被 KSU 管理器识别成 WebUI，点模块卡片上的 `<> 打开` 就能用。每个按钮对应 `bin/tasks/` 下一个独立脚本，能脱离网页单独跑：

```
bin/tasks/start.sh     启动桌面（后台 worker）
bin/tasks/stop.sh      停止桌面
bin/tasks/restart.sh   重启
bin/tasks/suspend.sh   挂起 / 解冻（kill -STOP / -CONT）
bin/tasks/status.sh    状态，2 秒轮询，必须毫秒级
bin/tasks/gpu.sh       GPU 检测
bin/tasks/log.sh       读日志
bin/tasks/audio.sh     音频单独启停
```

模块里没有 `action.sh`、没有 `service.sh`：内存态 root 重启就没了，开机自启是伪需求，写上也执行不了。

---

## 关键路径速查

```
Ubuntu rootfs           /data/linuxroot
X server loader         /data/adb/modules/chroot_manager/bin/loader.apk
X11 socket（宿主）       /data/data/com.termux/files/usr/tmp/.X11-unix/
X11 socket（容器）       /data/linuxroot/tmp/.X11-unix（bind mount）
真机内部存储（容器内）     /sdcard
宿主临时目录（容器内）     /mnt/hosttmp
KSU 模块                /data/adb/modules/chroot_manager/
模块运行时状态           /data/adb/chroot_manager/state/
手工启动脚本（无模块时）   /data/local/tmp/start-x11-desktop.sh
```

两套视角别混：

```
宿主 /data/linuxroot/root  =  容器 /root
宿主 /data/linuxroot/tmp   =  容器 /tmp
宿主 /storage/emulated/0   =  容器 /sdcard
宿主 /data/local/tmp       =  容器 /mnt/hosttmp
```

---

## B 层能到什么程度

- **GPU**：自编 Mesa 25.2.8，开 `-Dfreedreno-kmds=kgsl` 走 `/dev/kgsl-3d0`（Ubuntu 自带的 freedreno 只编了 msm 后端，chroot 里枚举不到设备）。注册 ICD 之后 `vulkaninfo` 能看到 `Turnip Adreno (TM) 725`。OpenGL 走 zink，但**必须用系统自带的 `zink_dri.so`**，自编那份会 `failed to choose pdev`。实测 glxgears 675 FPS，llvmpipe 312 FPS；llama.cpp Vulkan 全 offload Qwen2.5-3B，pp128 约 26 t/s。
- **Wine**：只有 box64 加 Kron4ek 的 Wine 10.0 amd64-wow64 这个组合跑通了。Wine 里的 OpenGL 程序会稳定段错误（退出码 139），包装脚本默认 `LIBGL_ALWAYS_SOFTWARE=1` 退回 llvmpipe。
- **中文**：base rootfs 没有 i18n 数据，要自己 `localedef`；KDE 的 `.mo` 被 `/etc/dpkg/dpkg.cfg.d/excludes` 排除了，要删掉那条规则再重装 61 个包；`LANG` 最关键的第三处是启动脚本里那行 `env -i`，不写前两处全部白做。
- **音频**：别碰 `/dev/snd`（那是 QCOM AudioReach 的虚拟节点）。链路是容器内 PulseAudio null sink → monitor → simple-protocol-unix → 宿主 AAudio 桥（C 写的 `aaudio_bridge_unix`，约 18 KB）。蓝牙耳机下整体约 1 秒延迟，A2DP 自己就占 300 到 600 ms。

---

## 已知边界

这些是文档里明确没验证、或者验证过不通的东西，看到空着别以为默认能用：

- **容器里的中文输入法**：没装 fcitx5，也没配输入法框架。要在容器里打中文得自己加。
- **进容器的录音**：播放方向通了，麦克风没做。
- **剪贴板互通**：X11 剪贴板和安卓剪贴板是两套东西，没验证。
- **外接键盘鼠标、多显示器**：没测。
- **EGL 加速**：Xlorie 的 DRI3 只覆盖 GLX。走 EGL 的程序（火狐、Electron 系）自动退到 llvmpipe。火狐在 llvmpipe 上会因为 LLVM JIT 生成非法指令而崩（SIGILL），修法是在 `user.js` 里把所有 GL 路径关掉，代价是纯 CPU 渲染。
- **Wine 里的 .NET**：单文件 bundle 起不来 CLR（三层模拟叠一起，box64 给不了 CoreCLR 要的页对齐和可执行内存）；.NET 8 WPF self-contained 能跑到 92% CPU 但没有主窗口（milcore 硬依赖 D3D + DirectWrite）。
- **非高通 GPU**：Mali 那一套（panfrost / panvk + `/dev/mali0`）本文一条都没验证。

---

## 安全与风险

- **BL 锁未解时不要写任何会被 BL 校验的系统分区。** 一旦触发校验就是变砖加数据全没，代价不对等。所有操作只碰 `/data`。
- 内存态 root 重启即失效。重启后先跑提权程序，再进管理器点「启动桌面」，整条链一次拉起。
- 这套东西跑在真机上，不是沙箱。下面三类操作动手前先说一句：
  - 会顶掉当前界面的：重启桌面、改分辨率、模拟点击、拉起全屏程序
  - 会吃满 CPU / GPU 的：编 Mesa、编 box64、解大压缩包、跑 llama.cpp 的 GPU 压测（屏幕会定格一两秒，看着像死机）
  - 会删文件、改开机行为的：清目录、装 KSU 模块
- 容器里的 `reboot` 只是 systemctl 的软链，不会重启手机。`/proc/sysrq-trigger` 那条路也被堵着（本机 `kernel.sysrq=0`），别去碰。

---

## 踩坑精选

这些都是真撞过的：

1. **`mountpoint -q` 判 bind 挂载不可靠**：同 `st_dev` 会认为没挂载，每跑一次叠一层。要读 `/proc/mounts` 比对第二列。
2. **`pkill -f 关键词` 会打死自己**：执行命令的 shell 自己的 cmdline 里就含那串关键词。表现是 `exit_code=143`、stdout 全空、后面的步骤一条没跑，而且是静默的。一律 `pidof` 拿 PID 再 kill，或者写成脚本文件再执行。
3. **`pgrep -f` 会匹配到它自己**：`pgrep -f App.dll` 永远返回非空，等于「永远认为它在跑」。判断语句要写 `pgrep -f 'App[.]dll'`。
4. **`kill` 和 `rm` 永远分两条命令**：自杀发生在第一条 kill 上时，后面的 rm 会被整条吞掉，返回码只剩 143，很容易误判成已经清完了。
5. **mksh 里 `r` 是内置别名**（`fc -e -`）：定义 `r()` 函数永远不会执行，还不报错，只是日志里那行凭空消失。shell 函数一律用 `emit` / `logit` 这类有意义的词。
6. **`grep -F "-> $M"` 会被当选项**：toybox 的 grep 把以 `-` 开头的模式当参数，必须写 `grep -F -e "-> $M"`。
7. **`umount` 必须按路径长度倒序**，最后用 `-l` 兜底：容器 `/dev` 下挂着 udev 的 debugfs，普通 umount 卸不掉。
8. **X11 socket 必须 bind 整个目录**：chroot 只认文件系统那种 socket，bind 单个 `X0` 文件不行。
9. **`loader.apk` 权限必须 644**：留 600 时 `app_process` 读不到，表现是静默不启动，日志里什么都没有。
10. **Termux 的 tmp 目录必须 `chmod 777`**：默认 `drwx------ u0_aXXX`，root 能写但 Xlorie 建 socket 会失败。
11. **KDE 的 `LANG` 要写三处**，第三处在启动脚本的 `env -i` 里：桌面进程树是从那里长出来的，`/etc/default/locale` 对这种方式完全无效。
12. **WebUI 的 `status.sh` 不许 chroot**：它每 2 秒跑一次，三次 chroot 加扫全表要 640 ms，请求堆积之后点按钮就像没反应。优化到 122 ms 的做法是一次 `pgrep -x` 拿多个进程。
13. **慢任务必须后台跑**：GPU 检测同步跑十几秒会把 `ksu.exec` 通道堵死，前端转圈然后报错。
14. **前端不能等回调才解锁**：回调丢了界面就永久假死。点按钮先乐观改 UI，1.8 秒后无论回调来没来都解锁。

---

## 仓库结构

```
.
├── README.md                   本文件
├── LICENSE
└── docs/
    ├── SKILL.md                完整手册（4800 行，含所有脚本源码、C 源码、Python 脚本）
    └── aaudio_bridge_unix.c    容器音频桥（手册 b11.1 里也有全文，单独放一份方便直接编译）
```

手册里的「别试」「跑不起来」都是实测样本，不是禁令。版本一变、机型一变结论就可能翻，想试就去试，只是先知道大概要花多少时间、会看到什么现象。

---

## 致谢

- Termux:X11 / Xlorie：整套方案的画面输出来自这里
- Mesa：Turnip 的 freedreno kgsl 后端
- box64、Kron4ek 的 Wine 构建
- KernelSU：模块 WebUI

## 许可

文档内容随仓库许可证发布，自行取用。文中提到的第三方项目各自遵循其原许可证。
