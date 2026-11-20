---
name: howto-install-chroot-in-phone
description: 在已 root 的安卓手机上从零装一套 chroot Ubuntu 24.04 桌面（KDE + Termux:X11 的 Xlorie 直接画屏），再补 GPU/Vulkan、box64、Wine、中文 locale 与字体、exe 双击与图标、性能调优。只写验证过的正确路径，走过的死路压成「别试」清单；与机型解耦，换任意一台 arm64 有 root 的安卓都能照做，未解 BL 锁的内存态 root，办家仍然可以照做（笔者的手机就没有解锁）。机器相关的东西全部做成可跳过或可覆盖的形式，开工先跑 a0 的环境体检。当用户说「手机上装 chroot Linux」「装个 Ubuntu 桌面」「chroot 桌面环境」「Termux:X11 怎么起」「chroot 里跑 exe」「box64 配 wine」「exe 图标/缩略图出不来」「KDE 汉化」「系统是英文的」「界面没中文」「wine 乱码」「程序界面是乱码」「exe 闪退」「点开就没了」「段错误 139」「chroot 怎么手动启动」「chroot 综合管理器」「chroot WebUI」「chroot GPU 用不了」「换台手机重装 chroot」「给别人装 chroot Linux」「chroot 里火狐闪退」「浏览器开一会儿就崩」「chroot 没声音」「容器里放不了声音」「怎么让 chroot 出声」「chroot 音频」「AAudio 桥」「容器里播放器没声音」「chroot 音乐播放器」「容器声音卡顿」「容器音频延迟」「GStreamer 没声音」时使用。
---
# 介绍

在已经拿到 root 的安卓手机上，把一套 Ubuntu 24.04 arm64 的根文件系统解到 `/data/linuxroot`，用 `chroot` 进去跑 KDE Plasma，画面不走 VNC 也不走 RDP，而是用 Termux:X11 的 Xlorie 直接画到 Android 的 Surface 上；x86 程序靠 box64 加 Wine 跑。

作者：Rxinns、[AI]Deepseek、[AI]Kimi
链接：https://github.com/RxinnotRstar/skill-install-chroot-android-phone

## 几个必须先接受的事实

- **它不是虚拟机**，也不是「和安卓并列的系统」。同一个内核，只换根目录。安卓的 system_server 和容器里的 plasmashell 在同一张 PID 表里，`ps -ef` 直接能互相看见。进程、网络、IPC 全部共享。
- **没有「重启 Linux」这回事**。内核是安卓的，重启就是重启手机。所谓重启桌面，只是把桌面进程杀掉重拉。
- **它不那么独立于安卓**。抢同一份 CPU 和内存，安卓卡它就卡。唯一的好处是 root 身份、父进程挂到 init 的进程，能躲过 MIUI 的后台清理，适合挂一些长时间运行的工具。
- **这篇skill不完善**。这份 skill 是笔者和 AI 一边研究探索路线、一边让 AI 将正确的路线记录下来的产物，可能存在不少的只能在笔者的手机上实现的内容，距离成熟的、可照抄的教程还有很远的距离。因此在照做的时候，很可能会遇到大量的这篇文档里没提及的问题，甚至部分内容本身就无法运行和前后矛盾，需要操作者有变通并解决问题的能力。

## 实测环境

红米 Note12 Turbo（marble / 骁龙 7+ Gen2 / Adreno 725）、MIUI 14.0.27 / Android 13、内核 5.10.149、KernelSU root 权限（支持“越狱模式”操作，本项目全程不写硬盘上受 BL 锁校验的分区）。

换机器时下面这几类的移植成本不一样，先看清楚自己属于哪一类：

- **机型无关**：装 rootfs、挂载、chroot 模板、apt 与多架构、KDE、chroot 综合管理器，全部逻辑在任意 arm64 安卓上照抄即可。32 位的 arm 设备不适用，镜像和包名整套都不一样。
- **高通专属**：b1 的 GPU 只对 Adreno + kgsl 成立（判据是 `/dev/kgsl-3d0` 存在、且 SELinux 对 appdomain 放行）。联发科走 Mali 那一套（`/dev/mali0` + panfrost/panvk），本文没有验证过，照抄必失败，直接在 B 层跳过 GPU 也能用。
- **ROM 相关**：a6 末尾的 MIUI 冻结白名单是小米系命令，原生 Android 上这两条会报错，跳过即可，不影响点亮。
- **要按自己设备改的参数**：a5 的 `ForceFontDPI=160`（按 1080 宽调的）、b7 的 `xrandr --fb`、a2 的磁盘余量阈值。

## 三个版块的定义

全文按「做完能得到什么」分三层，每一层都能单独收工：

- **A 做完，容器能点亮。** 从一张空盘装到屏幕上有 KDE 桌面，能开终端、能拖窗口、能跑 Linux 程序。例如：执行环境与路径（a0，不读就会把命令敲进错的窗口）、KSU 综合管理器（a7，日常唯一的入口，模块加 WebUI 整套骨架）、Termux:X11 显示层（a6，画面就靠它）、KDE 桌面（a5）、挂载与 chroot 的统一姿势（a3）、rootfs 安装（a2），等等。这一层不追性能、不碰兼容性，只保证「有画面」。
- **B 做完，容器勉强能日常用。** GPU 动得起来、exe 能双击跑、中文不乱码、服务能常驻、磁盘不会撑爆。例如：GPU 加速（b1，Turnip 走 Vulkan、zink 走 OpenGL）、中文与界面语言（b4，不弄就是乱码加英文）、容器出声（b11）、Wine 跑 exe（b3）、常用软件（b14，看图看视频听歌上网）、常驻服务与守护（b10），等等。到这一步它才算能替代一台电脑干活。
- **C 不是流程，是案例库。** 作者自己装某些非系统软件时的过程经验，也许对操作者会有启发，所以在这里保留。几个案例：网页版服务做成桌面图标（c4）、国内网盘把文件抠下来（c3）、改第三方应用的 SQLite 配置（c1）、给 .NET 服务配 Azure 权限并部署（c2）。它不参与前两层能不能用的判定，跳过完全没问题。

如果只想尝鲜，体验一下在手机上不关机跑 Linux，做到 A 就停。B 板块随便一节出了问题都得花几个小时排障。

## 其它注意事项

**a0 不属于任何一层，是全局前提**：它讲清楚每条命令该在哪跑、路径怎么对应、机器上缺哪些 App 时要跳过什么。不管做到哪一层，都先把 a0 读完。

**关于文中所有的「不行」**：这里的「别试」「跑不起来」「会崩」，都是笔者在这台机器、这个版本组合上亲眼看到的，不是严谨数学证明。版本一变、机型一变，结论就可能翻。看到这类地方，把它当成「笔者亲测的、已知的失败样本」而不是禁令，想试就去试，只是先知道大概要花多少时间、浪费多少Token、会看到什么现象，别白费力气。

**关于文中没覆盖的东西**：下面这几块本文一个字都没验证过，看到空着别以为默认就能用。

- **容器里的中文输入法**。b4 解决的是「显示中文」和「界面语言」，不是「输入中文」。KDE 里没装 fcitx5，也没有配输入法框架。要在容器里打中文，得自己装 fcitx5 加前端，在起桌面的 `env -i` 那行补 `GTK_IM_MODULE` / `QT_IM_MODULE`，再调 Termux:X11 的键盘映射。这套没实测，不写步骤。
- **进容器的录音**。播放方向 b11 已经打通（容器内 PulseAudio 混音 → UNIX socket → 宿主 AAudio 桥），但反过来没有：容器里麦克风仍然不可用，没做（见 b11.7）。
- **剪贴板互通**。X11 剪贴板和安卓剪贴板是两套东西，能不能通取决于 Termux:X11 的版本和设置，没验证。
- **外接键盘鼠标、多显示器**。没测。

别人在容器里能打中文、能录音，那是他们额外做了这些，不是本文漏了一步。

**这套东西跑在别人的手机上，不是沙箱。** 下面三类操作，动手前先说一句，别闷头做完再汇报：

- 会顶掉用户当前界面的：重启桌面、改分辨率、模拟点击、拉起全屏程序
- 会吃满 CPU 或 GPU 的：编 Mesa、编 box64、解大压缩包、装几百个包，以及**跑 llama.cpp 的 GPU 压测**（这条会让屏幕定格一两秒，用户会以为死机）
- 会删文件、改开机行为的：清目录、装 KSU 模块

用户如果正在用手机，这些操作会让他觉得手机被抢占了。

关键路径速查（宿主视角）：

```
Ubuntu rootfs          /data/linuxroot
宿主启动脚本            /data/local/tmp/start-x11-desktop.sh（a6 手工版）
旧入口（一行转发）      /data/local/tmp/start-linux.sh
日常入口                走 a7 的 KSU 模块，上面两个脚本只在没装模块时手敲
X server loader        /data/local/tmp/x11/loader.apk（3347 字节，644）
X11 socket（宿主）      /data/data/com.termux/files/usr/tmp/.X11-unix/
X11 socket（容器）      /data/linuxroot/tmp/.X11-unix（bind mount）
工作区在容器内          /mnt/ws
真机内部存储在容器内     /sdcard
KSU 模块（综合管理器）   /data/adb/modules/chroot_manager/（WebUI：webroot/index.html）
```

---

# A、底层：从空盘到点亮桌面

A 的完成标准就一句话：手机点开 Termux:X11，看到 KDE 桌面，能开终端、能拖动窗口。

## a0：执行环境与路径（先读这节，不然每条命令都会敲进错的窗口）

这份文档里的代码块来自四种完全不同的执行位置，长得像，跑错地方只会得到 `Permission denied` 或 `command not found`，而且提示往往指向错误的方向。每一步动手前先确认自己在哪一号位置。

| # | 位置 | 怎么进去 | 能做什么 | 做不到什么 |
|---|---|---|---|---|
| ① | 宿主 root shell | 提权后的 `adb shell` / Shizuku 授权 root 后的执行通道 / 手机终端里的 `su -c` | 读写 `/data` 任意位置、`mount`、`chroot`、按 `/proc` 杀进程 | 没有 `/workspace`，也没有容器内的任何命令 |
| ② | chroot 容器内 | 在 ① 里执行 `chroot $M ...` | 完整的 Ubuntu 用户态、apt、编译 | 看不见宿主 `/data`，宿主目录只有 bind 进 `/mnt/*` 的那几个 |
| ③ | Agent 工作区（proot 沙盒） | agent 自己的 shell 工具 | 有 `/workspace`、自带 python3/bash/gcc、能联网 | 看不见宿主 `/data/data`，`/sdcard` 往往是假的 |
| ④ | App 自带工具 | 截图、读图、通知、分享这类 | 能读相册可访问的路径 | 有些工具读不了 `/sdcard`，也读不了容器内路径 |

本文的约定，按这个读代码块就不会错：

- 讲宿主路径（`/data/...`、`/sdcard/...`）的代码块一律在 ① 跑，**必须 uid=0**。
- 标「容器」的代码块是 ②，统一走 `chroot` 模板。
- 出现 `/workspace` 的一律是 ③（agent 沙盒里的路径），别拿去 ① 跑，那里没有这个路径。
- 要「看一眼」的截图，从 ① 或 ② 产出后必须落到工作区，④ 才读得到；④ 读 `/sdcard` 和容器路径都会失败。

开工第一件事，确认身份：

```sh
id; cat /proc/self/attr/current
# 期望 uid=0(root) groups=0(root)，context 形如 u:r:ksu:s0 / u:r:magisk:s0 / u:r:supersu:s0
command -v chroot mount setsid pgrep
```

三个硬前提，缺了整套流程走不完：

1. **真 root**。uid=2000 的 shell 执行 `chroot` 直接 EPERM，没有替代路径。
2. **有一个能直接画到屏幕上的 X server 方案**。本文用 Termux:X11 的 Xlorie；没装 Termux:X11 的话 a6 整节不适用，得换 VNC / Xvfb 那一套（本文不覆盖）。
3. **`/data` 至少留 12GB**，理由在 a2。

拿到一台新机器，先跑这个体检，一条命令就能看出哪些节要做、哪些要跳过：

```sh
{
  echo "root      : $(id -u) $(cat /proc/self/attr/current)"
  echo "机型      : $(getprop ro.product.model) / $(getprop ro.board.platform) / Android $(getprop ro.build.version.release)"
  echo "架构      : $(uname -m)"
  echo "kgsl      : $([ -e /dev/kgsl-3d0 ] && echo 有 || echo 无)   # 无就跳过 b1"
  echo "data      : $(df -h /data 2>/dev/null | tail -1 | awk '{print $4" 可用 / "$5" 已用"}')"
  echo "termux    : $([ -d /data/data/com.termux/files/usr ] && echo 有 || echo 无)"
  echo "termux-x11: $(pm list packages 2>/dev/null | grep -c 'com.termux.x11')   # 0 说明 a6 走不通"
  echo "ksu       : $([ -x /data/adb/ksu/bin/ksud ] && echo 有 || echo 无)   # 无则 a7 的 WebUI 不可用"
  echo "busybox   : $(ls /data/adb/ksu/bin/busybox /data/adb/magisk/busybox 2>/dev/null | head -1)"
  echo "工作区    : $(ls -dt /data/data/*/files/workspaces/*/files 2>/dev/null | head -1)"
}
```

对照结果决定范围：`kgsl` 无就整节跳过 b1；`termux-x11` 为 0 就得换显示方案；`ksu` 无就跳过 a7、只用 a6 的脚本；`busybox` 有就用它当 ar/wget/xz 的兜底。

**命令存在不等于语义一样。** 这几条决定 a7 的脚本能不能原样跑（不同 ROM 的 toybox / busybox 参数面不一样），逐条对一下：

```sh
pgrep -x kwin_x11 plasmashell dotnet      # 多 pattern 必须是 OR 语义，一次能出多个 pid
ps -o comm= -p 1                          # 要支持 -o 和 comm=，输出进程名
ls -l /proc/1/root                        # 要能看到 -> 符号链接（a7.5 的 cpid 靠它认容器进程）
grep -F -e "-> /data/linuxroot" /dev/null; echo "rc=$?"   # 期望 rc=1；报 unknown option 就是不支持 -e
df -h /data | tail -1 | awk '{print $4}'  # 期望一个容量值（status.sh 取第 4 列当剩余空间）
```

- 哪条不符，就不要硬套 a7.5：`pgrep` 不支持多 pattern 就把循环拆成三条；`ps -o comm=` 不行就把 cpid 改成读 `/proc/pid/comm`（现在的写法本来就只读 comm，`ps` 只是排查用）；`df` 列不对就调 status.sh 的字段号。
- `grep` 那条尤其要注意：toybox 的 grep 会把以 `-` 开头的模式当选项，`-e` 是唯一稳妥写法，a3 的 `umount_tree` 和 a7.5 的 `cpid` 都依赖它。

### 本文的变量约定

所有脚本开头都用这几行，换机器只改这里，后面不用逐条改：

```sh
M=/data/linuxroot                          # rootfs 落点
MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
XAPK=$MODDIR/bin/loader.apk                # Xlorie 的 loader，见 a6
[ -f "$XAPK" ] || XAPK=/data/local/tmp/x11/loader.apk
XTMP=/data/data/com.termux/files/usr/tmp   # X11 socket 落地目录
WS_SRC=                                    # agent 工作区真路径；留空自动探测，用不着就留空
```

`XTMP` 默认取 Termux 的私有 tmp，因为 Xlorie 要求对这个目录可写，而 Termux 的 tmp 天然是 `777`。**机器上没有 Termux 就把它换成 `/data/local/tmp`**，自己 `chmod 777`，原理一样：只要是个 root 可写、Xlorie 能建目录进去的普通目录。这条没有在无 Termux 的机器上实测，换之前先 `ls -ld` 确认权限。

### 本文依赖哪些「别的 App」

有三处用到了本机其它 App，缺了不是必须补，是可以整段跳过：

| 依赖 | 用来做什么 | 没有怎么办 |
|---|---|---|
| Termux | 提供 X11 socket 目录、模型文件，偶尔借它的 python3/xz/tar | `XTMP` 改 `/data/local/tmp`；解压改在容器里做；读模型的段落跳过 |
| Termux:X11 | 本文唯一的画面出口 | 换 VNC 方案，a6 整节不适用 |
| 某个 agent | 把工作区 bind 成 `/mnt/ws`，方便和宿主来回递文件 | `WS_SRC` 留空，这层 bind 跳过，容器照常能点亮 |

### 要宿主上某个 App 的文件，怎么拿

B 层经常有这种需求（把模型、字体、exe、配置文件搬进容器）。三条路按顺序试：

1. **公共目录直读**。`/sdcard/Download`、`/sdcard/Pictures` 这类，root 和容器都能直接读写，容器里就是 `/sdcard`。
2. **私有目录用 root 读**。`/data/data/<包名>/` 下的大部分文件，root 能直接 `ls` / `cat` / `cp`；切成 shell 身份（uid 2000）就进不去，报 `Permission denied`，这**不是路径写错**。
   `/sdcard/Android/data/<包名>/` 是另一种情况：Android 11 起被 FUSE 限制，root 能读，shell 读不到。
3. **让 App 自己导出**。前两条都失败时（SELinux 更严的 ROM、文件加密、数据库被锁），让用户在 App 里点「分享 / 导出 / 备份」，落到公共目录再拿，比硬啃靠谱。

搬进容器的落点：要么直接写 `$M/...`（宿主视角），要么扔进已经 bind 好的 `/mnt/ws`、`/mnt/hosttmp`。别把文件放 `/data/local/tmp` 之后就忘了它没 bind 进去。

## a1：前置检查

```sh
# 在 ① 号位置（root shell）里跑
id; cat /proc/self/attr/current
df -h /data | tail -1
ls -ld /data/linuxroot
mount | grep linuxroot
```

- `chroot` 必须真 root。`id` 要是 uid=0，`/proc/self/attr/current` 要是 `u:r:ksu:s0` 这种。uid=2000（shell）执行 chroot 直接 EPERM，没法绕。
- 越狱模式的 root 是内存态，**重启就没了**，每次开机都要重新提权。所以这套东西**不做开机自启**：重启后先跑提权程序拿到 root，再打开 chroot 综合管理器的 WebUI，点「启动桌面」，整条链一次性拉起来（见 a7）。**这条只对内存态 root 成立**：Magisk / KernelSU 正式安装这种持久 root 开机就有 root，想让桌面跟着开机起也能做（加 `service.sh`，见 a7.2 末尾），本文没验证。
- BL 锁未解时不要写任何会被 BL 校验的系统分区。这条笔者不打算「碰碰运气」：一旦触发校验就是变砖加数据全没，代价不对等。所有操作只碰 `/data`。
- 别用 `du -sh /data/linuxroot` 看体积，几万个文件，几分钟不返回。看余量用 `df -h /data`，只读 Size / Used / Avail 三列。**Mounted on 那一列会骗人**：容器起来过之后 `/data` 下挂着 bind（比如 `/data/linuxroot/tmp/.X11-unix`），`df -h /data` 会把那一列显示成那个挂载点，看着像查错了地方，数值本身是对的。

## a2：装 rootfs

用官方 ubuntu-base（最小根文件系统，不带安装器），别用 docker 镜像或第三方打包。

```sh
# 宿主上不一定有 curl。先确认一下：
command -v curl || ls -l /data/adb/ksu/bin/busybox /data/adb/magisk/busybox 2>/dev/null

# 看当前版本号（有 curl 就用 curl，没有就 busybox wget 下来 grep）
curl -s "https://mirrors.tuna.tsinghua.edu.cn/ubuntu-cdimage/ubuntu-base/releases/24.04/release/" \
  | grep -oE 'ubuntu-base-[0-9.]+-base-arm64\.tar\.gz' | sort -u

cd /sdcard/Download
curl -L -o ubuntu-base-arm64.tar.gz \
  "https://mirrors.tuna.tsinghua.edu.cn/ubuntu-cdimage/ubuntu-base/releases/24.04/release/ubuntu-base-24.04.5-base-arm64.tar.gz"
# 没有 curl 时：
# /data/adb/ksu/bin/busybox wget -O ubuntu-base-arm64.tar.gz "<同一个 URL>"

mkdir -p /data/linuxroot
tar -xzf /sdcard/Download/ubuntu-base-arm64.tar.gz -C /data/linuxroot
df -h /data | tail -1
```

- 放在 `/data/linuxroot`，不要放 `/data/local/tmp`。后者系统 OTA 偶尔会清。
- **解压前先看一眼 `/data/linuxroot` 是不是空的**。对已有目录再解一遍会覆盖同名文件，用户之前装的东西可能就没了。目录里有东西的话，先问一句要不要换路径或者先备份。
- 解压完约 101M。装完 KDE 加一堆工具会涨到 4 到 6 GB，把 B 层的 Mesa 源码和 box64 源码算进去峰值能到 8 到 10 GB，编译产物删掉之后回落到 6 GB 左右。所以**开工前 `/data` 至少留 12GB**，不够就先清盘或者换落点，别装到一半卡死。
- 实测一台装完的机器：总量 939G、已用 919G、剩 21G、98%。这个余量能点亮、能装 KDE，但再编一次 Mesa 就悬了，属于「刚好够但没退路」的状态。
- 解压后 `bin`、`lib` 是指向 `usr/bin`、`usr/lib` 的符号链接，这是正常的，别当成包下坏了。

## a3：挂载与 chroot 的统一姿势

先给判重包装。**不要用 `mountpoint -q` 写这个判断**：

```sh
# mountpoint -q 判断 bind 挂载不可靠：源和目标在同一个 st_dev 上时它认为"没挂载"，
# 于是每执行一次就多叠一层 bind。实测把 /tmp/.X11-unix 叠到过 16 层。
is_mount() { mount | grep -F -q " on $1 type "; }

# mnt <挂载点> <mount 及其参数...>
mnt() {
  d=$1; shift
  mkdir -p "$d" 2>/dev/null
  is_mount "$d" && return 0
  "$@" >/dev/null 2>&1 || { echo "  挂载失败: $d"; return 1; }
}
```

挂载本体：

```sh
M=/data/linuxroot
mkdir -p $M/proc $M/sys $M/dev/pts $M/dev/shm $M/sdcard $M/tmp
mnt $M/proc    mount -t proc proc $M/proc
mnt $M/sys     mount --bind /sys $M/sys
mnt $M/dev     mount --bind /dev $M/dev
mnt $M/dev/pts mount --bind /dev/pts $M/dev/pts
mnt $M/dev/shm mount -t tmpfs -o size=128M tmpfs $M/dev/shm
mnt $M/sdcard  mount --bind /storage/emulated/0 $M/sdcard
echo "nameserver 223.5.5.5" > $M/etc/resolv.conf
```

还有三个可选的 bind。前两个是「和本机其它 App 交换文件」用的，没有对应 App 就整段跳过，容器照样能点亮：

```sh
# 工作区：给 agent 递文件用。WS_SRC 留空时自动找最近改过的那个
[ -z "$WS_SRC" ] && WS_SRC=$(ls -dt /data/data/*/files/workspaces/*/files 2>/dev/null | head -1)
if [ -n "$WS_SRC" ] && [ -d "$WS_SRC" ]; then
  echo "工作区: $WS_SRC"          # ★ 这行一定要看一眼，别挂错目录
  mnt $M/mnt/ws mount --bind "$WS_SRC" $M/mnt/ws
else
  echo "没找到 agent 工作区，跳过 /mnt/ws"
fi

# 模型目录：没装 Termux 就跳过
if [ -d /data/data/com.termux/files/home/models ]; then
  mnt $M/mnt/termux mount --bind /data/data/com.termux/files/home/models $M/mnt/termux
fi

# 宿主临时目录，基本总是有用
mnt $M/mnt/hosttmp mount --bind /data/local/tmp $M/mnt/hosttmp
```

- `ls -dt` 是按修改时间倒序，同时存在多个工作区时取最近动过的那个。实测机上只有一个工作区（`229d43d1-...`），所以「随便取一个」一直没出错，但换到多工作区的机器上就会挂错，症状是「文件放进去了，agent 那边看不到」。看到 `echo` 出来的路径和预期不一致，就把 `WS_SRC` 手动指过去。
- `/data/data/` 在 shell 身份（uid 2000）下**进不去**，这条 bind 只有在 ① 号位置（root）才成立。
- `mnt` 失败只打一行「挂载失败」不中断脚本，所以这一段必须看完整输出。
- `/mnt/ws` 就是 agent 的工作区，`/mnt/termux` 放 Termux 下的模型文件（跑 llama.cpp 从这儿读），`/mnt/hosttmp` 是宿主 `/data/local/tmp`，倒腾临时文件用。

之后所有容器内命令统一用这个模板：

```sh
chroot $M /usr/bin/env -i \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  HOME=/root TERM=xterm \
  /bin/bash -c '<你的命令>'
```

- 这些 mount 全是**内存态**，重启就没了。所以 a7 的启动脚本必须把它们全写进去，一次点按钮全部重建，不能指望手动。
- 不挂 `/dev/shm`，apt 和图形程序会报 `shmget failed`。挂 tmpfs 128M 足够。
- **必须 `env -i`**。shizuku 会往 chroot 里传 `TMPDIR=/data/user/0/moe.shizuku.privileged.api/cache`，这个路径在容器里不存在。症状是 `ls: command not found`、apt 的 postinst 报 `mktemp: failed to create file`、dpkg 配置失败，看起来完全不像环境变量问题。
- **尽量别用 `pkill -f 关键词`**。执行命令的 shell 自己的 cmdline 里就含这串关键词，会把自己打死。这不是玄学，是 cmdline 匹配的机制使然，笔者踩过三次。症状是整条命令 `exit_code=143`（或 137）、stdout 全空、后面的步骤一条都没跑，而且是静默的。一律 `pidof 名字` 拿 PID 再 `kill`，或者把逻辑写成脚本文件再 `bash 脚本`。方括号 `[x]xxx` 那种写法在**单条命令里只有它一个关键词**时是有效的（`pgrep -f 'termux-x11 [:]0'`），但同一行里还有裸关键词就照样被打中。
- **`pgrep -f` 会匹配到它自己**：`pgrep -f Microsoft365_E5_Renew_X.dll` 这条命令的 cmdline 里就含那个 dll 名，于是永远返回非空，等于"永远认为它在跑"，该启动的服务一个都不会启动。判断语句一定要写成 `pgrep -f 'Microsoft365_E5_Renew_X[.]dll'` 这种带方括号的形式。同理 `pgrep -f 'termux-x11 :0'` 要写成 `pgrep -f 'termux-x11 [:]0'`。
- 路径有两套视角，别混：

```
宿主 /data/linuxroot/root   =  容器 /root
宿主 /data/linuxroot/tmp    =  容器 /tmp
宿主 /storage/emulated/0    =  容器 /sdcard
宿主 /data/local/tmp        =  容器 /mnt/hosttmp
宿主 $M/mnt/ws 或 $WS_SRC   =  容器 /mnt/ws
```

- `cd` 不能写在 chroot 前面（cwd 会被重置成 `/`），要写进 `bash -c` 里。宿主侧没有 `/tmp`，重定向日志写到 `/tmp/xxx.log` 会让整条命令直接失败（`bash: can't create`），用 `/data/local/tmp/xxx.log`。

## a4：apt 源与 amd64 多架构

```sh
M=/data/linuxroot
rm -f $M/etc/apt/sources.list.d/ubuntu.sources

cat > $M/etc/apt/sources.list << 'EOF'
deb [arch=arm64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ noble main restricted universe multiverse
deb [arch=arm64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ noble-updates main restricted universe multiverse
deb [arch=arm64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ noble-backports main restricted universe multiverse
deb [arch=arm64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports/ noble-security main restricted universe multiverse
deb [arch=amd64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble main restricted universe multiverse
deb [arch=amd64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble-updates main restricted universe multiverse
deb [arch=amd64] http://mirrors.tuna.tsinghua.edu.cn/ubuntu/ noble-security main restricted universe multiverse
EOF

chroot $M /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root TMPDIR=/tmp \
  DEBIAN_FRONTEND=noninteractive /bin/bash -c 'dpkg --add-architecture amd64; apt-get update'
```

- 24.04 默认的 `/etc/apt/sources.list.d/ubuntu.sources`（deb822 格式）必须删掉，否则它会继续按 ports 去请求 amd64，刷一屏 404。
- `ports.ubuntu.com` 和 `ubuntu-ports` **只有 arm64**。amd64 那几行必须写不带 `-ports` 的 `.../ubuntu/`。
- 每一行都要带 `[arch=xxx]`，否则一个源会被两种架构同时请求。
- 所有 dpkg/apt 命令都要 `TMPDIR=/tmp`。忘了就会出现「装完表面成功，其实 ca-certificates 没配置好」这种隐形故障，修法是 `dpkg --configure -a` 再来一遍。

## a5：桌面环境 KDE Plasma

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

`/root/.config/kwinrc`

```ini
[Compositing]
Enabled=false
Backend=QPainter
OpenGLIsUnsafe=true
[org.kde.kdecoration2]
library=org.kde.breeze
theme=Breeze
```

`/root/.config/kscreenlockerrc`

```ini
[Daemon]
Autolock=false
LockOnResume=false
```

`/root/.config/kdeglobals`（手机屏小，字太小，加大 DPI）

```ini
[General]
ForceFontDPI=160
```

- `160` 是按 1080 宽、6.7 寸屏看着舒服的值。换分辨率或换机器要自己调，嫌字大就往下压（144、136）。

- **KWin 合成必须关**。软件的 GLX 不支持 `glSwapInterval`，KWin 会 SIGILL 循环重启，日志里是 `glSwapInterval is unsupported` 加 `KCrash: crashing... crashRecursionCounter = 2`。试过 `KWIN_COMPOSE=O2`、`LIBGL_ALWAYS_SOFTWARE=1`、`Backend=QPainter` 但 `Enabled=true`，全都没用，唯一解是 `Enabled=false`。代价是没阴影、没圆角。
- 不写 `kscreenlockerrc`，连上去看到的就是锁屏。
- 改完配置**必须重启整个 Plasma 会话**。只杀 `kscreenlocker_greet` 没用，kded 会立刻重拉。正确做法是杀掉 `plasmashell` / `kwin_x11` / `ksmserver` / `plasma_session` 再重新起（起法见 a6）。
- 只装 `breeze` 包不够，`kdeglobals` 和 `kwinrc` 里的 decoration 也要指过去，否则窗口还是 95 风的灰框。而且光重启 kwin 有时不生效，要重启会话。

## a6：显示层 Termux:X11（Xlorie）

这一节是整套方案的地基。核心认知只有一句：**X server 不需要开 App**。

`termux-x11` 这个命令不是「在 App 进程里跑 X server」，而是用 `/system/bin/app_process`（ART 虚拟机）加载 `loader.apk` 里的 `Loader` 类，反射调用 `com.termux.x11.CmdEntryPoint`，最终把 `libXlorie.so` 拉起来。用 root 跑这条命令，X server 就是一个独立的 root 进程，`am force-stop com.termux.x11` 都杀不掉它。App 只是「显示器」，X server 是「显卡」。

**第一步：拿 loader.apk**

宿主上不一定有 curl，也不一定有 ar。先看自己有什么：

```sh
for x in /system/bin/curl /data/data/com.termux/files/usr/bin/curl \
         /data/adb/ksu/bin/busybox /data/adb/magisk/busybox; do
  [ -e "$x" ] && echo "有: $x"
done
```

下载 deb（两条路，挑能跑的那条）：

```sh
URL="https://mirrors.tuna.tsinghua.edu.cn/termux/apt/termux-x11/pool/main/t/termux-x11-nightly/termux-x11-nightly_1.03.01-6_all.deb"
curl -sL -o /data/local/tmp/t.deb "$URL"                          # 有 curl
/data/adb/ksu/bin/busybox wget -O /data/local/tmp/t.deb "$URL"    # 只有 busybox
```

拆包（三条路，从上往下试，第一条最省事）：

```sh
cd /data/local/tmp
/data/adb/ksu/bin/busybox ar x t.deb        # ① KSU 自带 busybox，通常有这个
/data/data/com.termux/files/usr/bin/ar x t.deb   # ② Termux 装了 ar
```

①② 都不行，才用 python 手切（纯净 Android 的 toybox 里确实没有 ar，但 root 方案一般带 busybox）：

```sh
python3 -c "
d=open('/data/local/tmp/t.deb','rb').read()
assert d[:8]==b'!<arch>\n'
off=8
while off+60<=len(d):
    hdr=d[off:off+60]
    name=hdr[0:16].decode('utf8','replace').strip()
    size=int(hdr[48:58].decode('utf8','replace').strip())
    if name.startswith('data.tar'):
        open('/data/local/tmp/data.tar','wb').write(d[off+60:off+60+size])
    off += 60+size+(size%2)
"
mkdir -p /data/local/tmp/x11 /data/local/tmp/x11deb
tar xf /data/local/tmp/data.tar -C /data/local/tmp/x11deb
cp /data/local/tmp/x11deb/data/data/com.termux/files/usr/libexec/termux-x11/loader.apk \
   /data/local/tmp/x11/loader.apk
chmod 644 /data/local/tmp/x11/loader.apk
```

- 顺序是 busybox ar → Termux ar → python 手切。**别看到「手机没有 ar」就去 `apt install ar`**，那个包在 Termux 里也不叫这个名字。
- busybox 的位置按 root 方案不同：KernelSU 是 `/data/adb/ksu/bin/busybox`，Magisk 是 `/data/adb/magisk/busybox`。busybox 里还顺带有 `wget`、`xz`、`tar`、`gzip`，宿主缺工具时优先找它。
- `loader.apk` 只有 3347 字节，是正常的，里面就三个文件（Manifest、resources.arsc、3820 字节的 classes.dex）。
- **权限必须 644**。留 600 时 `app_process` 读不到，表现是静默不启动，日志里没东西。

**第二步：起 X server**

```sh
# $M / $XTMP 见 a0；没有 Termux 时 $XTMP=/data/local/tmp
mkdir -p "$XTMP"; chmod 777 "$XTMP"
CLASSPATH=$XAPK \
XKB_CONFIG_ROOT=$M/usr/share/X11/xkb \
TMPDIR=$XTMP \
setsid /system/bin/app_process -Xnoimage-dex2oat / \
    --nice-name="termux-x11 :0" com.termux.x11.Loader :0 \
    > $M/tmp/tx11.log 2>&1 < /dev/null &
```

| 变量 | 作用 |
|---|---|
| `CLASSPATH` | 告诉 app_process 从哪个包加载类 |
| `XKB_CONFIG_ROOT` | 键盘映射根目录，不能省，省了报 `$XKB_CONFIG_ROOT is not set`。指到 chroot 里现成的那份最省事（Termux 那边默认没装 xkeyboard-config） |
| `TMPDIR` | Xlorie 用它拼 socket 路径，不能省，省了报 `$TMPDIR is not set` |
| `--nice-name` | 进程名，方便 `pgrep -f "termux-x11 :0"` |

**第三步：把 socket 接进 chroot**

```sh
mkdir -p $M/tmp/.X11-unix
# 判重别用 mountpoint -q（理由见 a3），读 /proc/mounts
if ! awk -v d="$M/tmp/.X11-unix" '$2 == d {f=1; exit} END {exit !f}' /proc/mounts; then
  mount --bind $XTMP/.X11-unix $M/tmp/.X11-unix
fi
ls -l $M/tmp/.X11-unix/    # 应该看到 srwxrwxrwx X0
```

- 这一层特别容易叠。`mountpoint -q` 在源和目标同一个 `st_dev` 时会认为「没挂载」，每跑一次启动脚本就多一层，`/proc/mounts` 里同一个目标会重复出现。正常状态应该只有一行。
- socket 目录由 Xlorie 自己建，权限 `1777`。宿主侧看到 `srwxrwxrwx X0` 就对了。

**第四步：在容器里起桌面**

```sh
chroot /data/linuxroot /usr/bin/env -i \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  HOME=/root DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
  TMPDIR=/tmp SHELL=/bin/bash QT_QUICK_BACKEND=software KWIN_COMPOSE=0 \
  /bin/bash -c '
    mkdir -p /tmp/runtime-root && chmod 700 /tmp/runtime-root
    setsid kwin_x11 > /tmp/kwin.log 2>&1 < /dev/null &
    sleep 3
    setsid plasmashell > /tmp/pshell.log 2>&1 < /dev/null &
  '
```

（这里故意没有 `VK_ICD_FILENAMES`。A 阶段不需要任何 GPU，桌面靠软件渲染就能亮；b1 把 Turnip 装好之后再把那一行加回来。）

**第五步：验证**

```sh
pgrep -f "termux-x11 :0"
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin /bin/bash -c 'DISPLAY=:0 xdpyinfo | head -5'
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin /bin/bash -c 'DISPLAY=:0 xrandr | head -3'
```

- `xdpyinfo` 报 `X.Org version: 21.1.99` 才连对（Xlorie 是 21.1.99，Xvfb 是 21.1.11，用它区分连的是谁）。
- `xdpyinfo` 的扩展列表里应该有 `DRI3`，这说明 Xlorie 自带真 DRI3（走 AHardwareBuffer 传 fd），B 里 OpenGL 能白捡加速就靠它。

**第六步：把上面全部写成一个脚本**

这一步开始涉及杀进程和重起桌面。如果用户正开着 Termux:X11 看画面，先跟他说一句「桌面要重启一下」，别让他的画面突然没了。

`/data/local/tmp/start-x11-desktop.sh` 就是把 a3 的挂载、第二步、第三步、第四步串起来，每步都加判重：

```sh
#!/system/bin/sh
# 一键拉起 chroot KDE 桌面，直接画到 Termux:X11 的 Surface 上（需要 root）
M=/data/linuxroot
XAPK=/data/local/tmp/x11/loader.apk
# 没有 Termux 的机器把 XTMP 换成 /data/local/tmp
XTMP=/data/data/com.termux/files/usr/tmp
LOGDIR=$M/tmp

[ -d "$M" ] || { echo "rootfs 不存在"; exit 1; }
[ -f "$XAPK" ] || { echo "缺 loader.apk: $XAPK"; exit 1; }

# ---------- 1. 挂载 ----------
# 判重读 /proc/mounts，不要用 mountpoint -q（同 st_dev 会误判成没挂载，见 a3）
ism() { awk -v d="$1" '$2 == d {f=1; exit} END {exit !f}' /proc/mounts 2>/dev/null; }
mkdir -p $M/proc $M/sys $M/dev/pts $M/dev/shm $M/sdcard $M/tmp
ism $M/proc    || mount -t proc proc $M/proc
ism $M/sys     || mount --bind /sys $M/sys
ism $M/dev     || mount --bind /dev $M/dev
ism $M/dev/pts || mount --bind /dev/pts $M/dev/pts
ism $M/dev/shm || mount -t tmpfs -o size=128M tmpfs $M/dev/shm
ism $M/sdcard  || mount --bind /storage/emulated/0 $M/sdcard

# ---------- 2. X server（root 进程，独立于 App）----------
chmod 777 $XTMP 2>/dev/null
mkdir -p $LOGDIR
if [ -z "$(pgrep -f 'termux-x11 :0')" ]; then
  CLASSPATH=$XAPK \
  XKB_CONFIG_ROOT=$M/usr/share/X11/xkb \
  TMPDIR=$XTMP \
  setsid /system/bin/app_process -Xnoimage-dex2oat / \
      --nice-name="termux-x11 :0" com.termux.x11.Loader :0 \
      > $LOGDIR/tx11.log 2>&1 < /dev/null &
  sleep 5
fi

# ---------- 3. socket 接进容器 ----------
mkdir -p $M/tmp/.X11-unix
ism $M/tmp/.X11-unix || mount --bind $XTMP/.X11-unix $M/tmp/.X11-unix
[ -S $M/tmp/.X11-unix/X0 ] || { echo "X0 socket 没建起来"; exit 1; }

# ---------- 4. 桌面 ----------
if [ -z "$(chroot $M /usr/bin/pgrep -x kwin_x11)" ]; then
  chroot $M /usr/bin/env -i \
      PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
      HOME=/root DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
      LANG=zh_CN.UTF-8 LANGUAGE=zh_CN:en \
      TMPDIR=/tmp SHELL=/bin/bash QT_QUICK_BACKEND=software KWIN_COMPOSE=0 \
      /bin/bash -c '
        mkdir -p /tmp/runtime-root && chmod 700 /tmp/runtime-root
        setsid kwin_x11 > /tmp/kwin.log 2>&1 < /dev/null &
        sleep 3
        setsid plasmashell > /tmp/pshell.log 2>&1 < /dev/null &
      '
fi

sleep 6
echo "X server: $(pgrep -f 'termux-x11 :0' | head -1)"
echo "kwin:     $(chroot $M /usr/bin/pgrep -x kwin_x11)"
echo "plasma:   $(chroot $M /usr/bin/pgrep -x plasmashell)"
echo "现在点开 Termux:X11 就是桌面"
```

- 早先所有尝试失败的原因，逐条记一遍，省得重走：`:5` 打不开是因为 socket 表里只有 X0/X11，加上当时 Xvfb 占着 `:0`；起来 1.5 秒就被 `SmartPower ... move to hibernation` 冻掉，是因为 X server 跑在 App 进程里，现在提到 root 进程就解耦了；chroot 里连不上是因为 `/tmp/.X11-unix` 是独立的空目录，要 bind；「App 必须开着才有 X」是被 MainActivity 那个 "Not connected" 界面误导的。
- `/data/data/com.termux/files/usr/tmp` 默认是 `drwx------ u0_aXXX`，root 能写但 Xlorie 建 socket 会失败，**必须 chmod 777**（只改这个目录，别动父目录）。实测跑起来之后它是 `drwxrwxrwx u0_a393`，里头的 `.X11-unix` 是 `1777`，由 Xlorie 自己建。
- X11 socket 有文件系统和 abstract 两种。chroot 只认文件系统那种，所以必须 bind mount **整个目录**，bind 单个 `X0` 文件不行。
- 笔者建议别用 `dbus-launch --exit-with-session startplasma-x11`。这个组合在这台机器上会静默失败：`plasma.log` 是 0 字节，进程表里只剩一个 `dbus-launch` 挂着，kwin 和 plasmashell 全没起来，重跑一次时好时坏。直接拉 `kwin_x11` 和 `plasmashell` 两个进程，100% 成功。代价是没有完整会话管理（注销/关机按钮不管用）。
- `QT_QUICK_BACKEND=software` 必须给，不给 plasmashell 的 Qt Quick 会去试 GL 然后崩。`XDG_RUNTIME_DIR` 必须建出目录并 `chmod 700`，否则 Qt 抱怨 invalid。
- plasmashell 启动时刷的 `kactivitymanagerd is not running`、`Failed to find a Kirigami platform plugin`、`Entry is not valid "preferred://browser"` 都是无害的。kwin 只刷一行 tiles 配置 parse error，也无害。
- 分辨率跟着手机走（`xrandr` 报 `current 720 x 1600, maximum 32767 x 32767`），横屏自动变 `2400x874`。这一点比老的 Xvfb 强：`maximum` 不是锁死的，能动态改。
- App 侧要防 MIUI 冻结（App 是显示器，冻了就看不了）：

```sh
dumpsys deviceidle whitelist +com.termux.x11
am set-standby-bucket com.termux.x11 active
dumpsys deviceidle whitelist | grep -i termux   # 期望 user,com.termux.x11,10416
```

**Termux:X11 的偏好得自己在 App 里调。** 没有现成配置可以抄：按键栏、强制横屏、防息屏这些开关都在 App 的设置界面里，key 名还跟着版本变。要调的几项是防息屏、强制横屏、按键栏透明度、指针捕获、按键栏布局（决定右上角有没有退出键、右下角有没有键盘键）。

- 别去别的 App 的私有目录里拷偏好文件。那些值是别人对着自己的屏幕尺寸、输入法和使用习惯调出来的，搬过来大概率不合适，而且属于人家的私有配置。

## a7：chroot 综合管理器（KSU 模块 + WebUI）

**这一节是可选增强，不是必经之路。** 它有两个前提：root 方案是 KernelSU（3.x 以上，带模块 WebUI），并且你愿意用网页当控制台。

- 用 Magisk 的话没有 `ksu.exec` 这套注入对象，页面能打开但按钮点了没反应。这种情况整节跳过，用 a6 第六步那个 `start-x11-desktop.sh`，再自己补一个 stop 和 status 的脚本，功能一样，只是没有图形界面。
- **a6 的手工脚本和 a7 的模块二选一。** 两边做的是同一件事（挂载 + 起 X + 起桌面），挂载有判重不会叠，但同时跑会出现「一边在停、另一边以为还在跑」的错位，排查起来很费劲。装了 a7 就把 `/data/local/tmp/start-x11-desktop.sh` 当参考留着，日常只用 WebUI。
- 先确认 `ksud` 在不在：KernelSU 的标准位置是 `/data/adb/ksu/bin/ksud`（软链到 `/data/adb/ksud`），PATH 里通常已经带了；不确定就一律写绝对路径 `/data/adb/ksud`。

到这一步你已经有：rootfs 装好、挂载姿势对了、Termux:X11 能画屏、KDE 能点亮、GPU 通了。剩下的问题是**怎么日常操作它**。

早期做法是拆成两个模块：一个负责启动（`action.sh` 转发到启动脚本），一个负责挂起解冻（`action.sh` 做开关）。模块一多就难受——每次要重启桌面得先去点 A 模块的按钮，想腾 CPU 得去点 B 模块，日志散在四五个地方，改个参数要开终端。**最终形态是合并成一个模块 + 一个 WebUI**，点一下调一个脚本，见下面。

这里是这一版的设计，也是踩完坑之后的样子，别自己重新发明。

### a7.0 模块目录与权限（WebUI 出不来，九成是这节没做对）

```sh
# 位置 ①，root 身份
MOD=/data/adb/modules/chroot_manager
mkdir -p $MOD/bin/tasks $MOD/webroot
# 文件写完再统一刷权限，最后一步必须做
chmod 755 $MOD $MOD/bin $MOD/bin/tasks $MOD/webroot
chmod 755 $MOD/uninstall.sh $MOD/bin/lib.sh $MOD/bin/tasks/*.sh
chmod 644 $MOD/module.prop $MOD/webroot/index.html $MOD/bin/loader.apk
```

- 实测权限值就是这套：目录 755、`*.sh` 755、`module.prop` 与 `index.html` 644、`loader.apk` 644。实测机上 `bin/` 和 `webroot/` 实际是 700 也能用（管理器以 root 读），但给别人装的时候用 755，少一个变量。
- `module.prop` 里的 `id` 必须和目录名一致，不一致的话列表里显示会错乱。
- `loader.apk` 要手动 `cp` 一份进 `bin/`（a6 抠出来的那份）。a7.4 的 lib.sh 有 `/data/local/tmp/x11/loader.apk` 兜底，但自带一份更稳，也不怕 `/data/local/tmp` 被 OTA 清。
- **不用重启手机**。KSU 是现场读 `/data/adb/modules` 的，`/data/adb/ksud module list` 立刻就能看到；管理器界面里重进一次模块列表即可。
- 列表里没有就按顺序查：目录名 → `module.prop` 的 `id` → 所有者是不是 `root:root` → 文件权限。
- 有 `webroot/index.html` 的模块，`ksud module list` 输出里会多一行 `"web": "true"`，这是判断「WebUI 认没认到」的唯一硬指标。

### a7.1 为什么用 WebUI 而不是只用「操作」按钮

KernelsU 3.x 支持模块自带 WebUI：模块目录下放 `webroot/index.html`，管理器里那张模块卡片会出现一个 `<> 打开` 按钮，点进去是用系统 WebView 加载的网页。网页里能拿到注入的 `ksu` 对象，用 `ksu.exec(cmd, opts, cbName)` 以 root 执行命令并把 stdout 拿回来。

这比「操作」按钮好在：

- 一个页面放十几个控件，不用每次点按钮看一眼终端弹窗
- 能实时显示状态（挂载数、PID、磁盘、内存），2 秒刷一次
- 能读日志、能看历史
- 按钮可以按状态禁用/变灰，不会误触

代价是：**WebUI 里跑的每一次 exec 都是 root**，写错就是事故。所以后面那几条性能坑必须当真。

`ksud module list` 里能看到这个模块多了 `"web": "true"` —— 这就是「有 webroot/index.html」的标志。没有这个字段就是没认到（常见原因：`webroot` 目录名拼错、index.html 权限不对、模块没重新扫描）。

### a7.2 目录结构

```
/data/adb/modules/chroot_manager/
├── module.prop
├── uninstall.sh              卸载时收尾（解冻 + 停桌面 + 卸挂载）
├── bin/
│   ├── lib.sh                公共变量和函数，所有任务都 source 它
│   ├── loader.apk            Xlorie 的 loader（3347 字节）
│   └── tasks/                ★ 每个按钮对应这里一个脚本
│       ├── start.sh          启动桌面（后台 worker）
│       ├── stop.sh           停止桌面（后台 worker）
│       ├── restart.sh        重启 = stop + start
│       ├── suspend.sh        挂起/解冻（同步，快）
│       ├── status.sh         状态（毫秒级，2 秒一次）
│       ├── gpu.sh            GPU 检测（后台 worker）
│       ├── log.sh            读各种日志
│       ├── open.sh           打开外部 App / 网页
│       └── audio.sh          音频单独启停 + 跟随开关
└── webroot/
    └── index.html            控制台
```

**没有 `action.sh`，没有 `service.sh`。** 理由跟上一条一样：越狱模式的 root 是内存态，重启后要手动提权，开机自启是伪需求；「操作」按钮在 WebUI 面前也没必要存在。真想要一个按钮兜底的话加 `action.sh` 也只该做一件事——打印 WebUI 入口提示，别让它去动 chroot。

（补一句边界：上面这条只对内存态 root 成立。Magisk / KernelSU 正式安装这种持久 root，开机就有 root，想要开机自启可以加 `service.sh` 起桌面，代价是开机就抢 CPU，本文没验证过，自己权衡。）

`uninstall.sh` 是目录里唯一没在正文出现的脚本，但它必须存在（a7.8 会 `sh -n` 检查它），全文如下：

```sh
#!/system/bin/sh
# uninstall.sh —— 模块被卸载时由 KSU / Magisk 调用（删目录之前执行）
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh" 2>/dev/null

# 1) 先解冻。挂起状态下的进程是 STOPPED，直接卸挂载会卡在 busy
if [ -f "$STATE/.frozen" ] && [ -f "$STATE/.pidlist" ]; then
  while read -r p; do
    [ -n "$p" ] && kill -CONT "$p" 2>/dev/null
  done < "$STATE/.pidlist"
  rm -f "$STATE/.frozen" "$STATE/.pidlist" "$STATE/.pidlist.tmp" 2>/dev/null
fi

# 2) 停音频桥和容器内的桌面。一律按 pid 点名，不用 pkill -f（见 a3）
for p in $(cpid pulseaudio) $(cpid kwin_x11) $(cpid plasmashell) $(xs_pid); do
  [ -n "$p" ] && kill -9 "$p" 2>/dev/null
done
for p in $(pgrep -x aaudio_bridge_u aaudio_bridge 2>/dev/null); do
  kill -9 "$p" 2>/dev/null
done

# 3) 从深到浅卸挂载，lazy 兜底
sleep 1
umount_tree

# 4) 只清模块自己的运行时状态。★ 绝对不要动 /data/linuxroot 里的任何东西，
#    那是用户的整个容器，卸载模块不等于删掉系统
rm -rf "$STATE" "$DATA/task.log" "$DATA/gpu.log" "$DATA/history.log" 2>/dev/null

exit 0
```

### a7.3 module.prop

```ini
id=chroot_manager
name=chroot 综合管理器
version=v1.0
versionCode=1
author=<你的名字>
description=chroot 桌面的一站式管理器。点 WebUI 进入控制台，启动/停止/重启/挂起/解冻/GPU检测全部手动点按钮，点一下调用 bin/tasks 里对应的一个脚本。模块装上去什么都不跑，没有守护进程，不碰 chroot 一根手指头。
```

description 里那句「什么都不跑」是有意义的：用户装完模块会担心它偷跑东西，写清楚省得解释。

### a7.4 bin/lib.sh：所有任务共用的底座

**每个 tasks/*.sh 开头都 source 这一份**，别各自复制粘贴。核心就这几块：

```sh
#!/system/bin/sh
MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
DATA=/data/adb/chroot_manager
STATE=$DATA/state

M=/data/linuxroot
XAPK=$MODDIR/bin/loader.apk
[ -f "$XAPK" ] || XAPK=/data/local/tmp/x11/loader.apk
XTMP=/data/data/com.termux/files/usr/tmp

mkdir -p "$DATA" "$STATE" 2>/dev/null

# agent 工作区真路径。别写死包名，也别用 ls -d ... | head -1：多工作区时会挑错
wsdir() { ls -dt /data/data/*/files/workspaces/*/files 2>/dev/null | head -1; }
```

`wsdir` 取的是最近修改过的那个工作区。要在 UI 显示它、或者把它 bind 进 `/mnt/ws`，一律调这个函数，别在内联命令里重复写通配。

挂载判重和批量卸载：

```sh
# 读 /proc/mounts 判重。绝对不要用 mountpoint -q（见 a3）
is_mount() {
  awk -v d="$1" '$2 == d {found=1; exit} END {exit !found}' /proc/mounts 2>/dev/null
}

mnt() {
  d=$1; shift
  mkdir -p "$d" 2>/dev/null
  is_mount "$d" && return 0
  "$@" >/dev/null 2>&1 || { echo "  挂载失败: $d"; return 1; }
}

# 递归卸掉 $M 下所有挂载。必须从深到浅，否则父挂载因子挂载存在而 busy。
# 最后用 -l（lazy）兜底：从命名空间摘掉，引用释放后内核自己真正卸。
umount_tree() {
  list_mounts() { awk -v p="$M/" '$2 ~ ("^" p) {print $2}' /proc/mounts 2>/dev/null; }
  list_mounts | awk '{print length" "$0}' | sort -rn | cut -d' ' -f2- \
    | while read -r d; do
        [ -z "$d" ] && continue
        umount "$d" 2>/dev/null || umount -l "$d" 2>/dev/null
      done
  sleep 1
  list_mounts | awk '{print length" "$0}' | sort -rn | cut -d' ' -f2- \
    | while read -r d; do
        [ -z "$d" ] && continue
        umount -l "$d" 2>/dev/null
      done
}
```

关于 `umount_tree` 的写法，有两个真踩过的点：

- **必须按路径长度倒序**。先卸 `/dev` 再卸 `/dev/pts` 会 `Device or resource busy`。
- **`umount` 失败要 `umount -l` 兜底**。容器 `dev` 里挂着 udev 的 `debugfs`（路径形如 `/dev/hbpxkeim/ubhielot`），普通 umount 卸不掉，lazy 一下就干净了。实测普通 umount 后剩 2 个挂载，加 lazy 后 0 个。

冻结标记的自愈逻辑（**这条让你可以完全不写 service.sh**）：

```sh
# 标记还在但名单里的进程全没了 → 是上次重启留下的垃圾，自己清掉
is_frozen() {
  [ -f "$STATE/.frozen" ] || return 1
  if [ -f "$STATE/.pidlist" ]; then
    alive=0
    while read p; do
      [ -n "$p" ] && [ -d "/proc/$p" ] && alive=$((alive + 1))
    done < "$STATE/.pidlist"
    if [ "$alive" -eq 0 ]; then
      rm -f "$STATE/.frozen" "$STATE/.pidlist" "$STATE/.pidlist.tmp" 2>/dev/null
      return 1
    fi
  fi
  return 0
}
```

找 pid 的函数**不要走 chroot**（原因见 a7.7）：

```sh
xs_pid() { pgrep -f 'termux-x11 [:]0' 2>/dev/null | head -1; }

# 按进程名在容器里找 pid，扫 /proc 就行，不用 chroot
cpid() {
  _n=$1
  for _p in $(ls -l /proc/[0-9]*/root 2>/dev/null \
              | grep -F -e "-> $M" | grep -o -e "/proc/[0-9]*" | cut -d/ -f3); do
    read -r _c < "/proc/$_p/comm" 2>/dev/null || continue
    [ "$_c" = "$_n" ] && { echo "$_p"; return 0; }
  done
}
kwin_pid() { cpid kwin_x11; }
plasma_pid() { cpid plasmashell; }
svc_pid() { cpid dotnet; }   # 示例：常驻服务的进程名，按自己情况改
desktop_alive() { [ -n "$(kwin_pid)" ]; }
```

日志三件套：`task.log` 放最近一次动作的完整输出（UI 读这个），`history.log` 一次动作一行，`gpu.log` 单独放检测结果。下面这几个函数**必须一起放进 lib.sh**，a7.5 的脚本全靠它们，缺一个就是 `command not found` 静默失败：

```sh
# 回显用。别在这里再写一遍 task.log：worker 的 stdout 已经被重定向进 $DATA/task.log 了，
# 函数里再追加一次会让每行重复。脱离 WebUI 手跑时输出直接落在终端，也是想要的行为。
log()  { printf '%s\n' "$*"; }
emit() { printf '%s\n' "$*"; }   # 就是 log 的别名；gpu.sh 会再覆盖它，改成追加进 gpu.log

# 一次动作一行，带时间戳，给 UI 的「操作记录」用
hist() { printf '%s %s\n' "$(date '+%F %T')" "$*" >> "$DATA/history.log" 2>/dev/null; }

# 在容器里跑一条命令：stdout 原样返回，退出码原样透出（调用方常拿它判空、判成功）
inbox() {
  chroot "$M" /usr/bin/env -i \
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    HOME=/root DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root TMPDIR=/tmp \
    /bin/bash -c "$*"
}

# 容器里的 pulseaudio。走 cpid 扫 /proc，不 chroot（见 a7.7 第①条）
cpulse_pid() { cpid pulseaudio; }

# 「跟随容器启动」开关，文件不存在按开处理
audio_follow() { [ "$(cat "$STATE/audio.follow" 2>/dev/null)" = "0" ] && echo 0 || echo 1; }
```

- `inbox` 是唯一进容器的入口，别在任务脚本里直接写 `chroot`（a7.7 第①条：status.sh 里连 chroot 都不许有）。
- 函数定义顺序无所谓（sh 是运行时解析），但 `cpulse_pid` 依赖 `cpid`，两个都得在 lib.sh 里。

### a7.5 任务脚本：一个按钮 = 一个脚本

**这是整套设计的关键。** WebUI 不写任何业务逻辑，它只会 `sh bin/tasks/xxx.sh`。想加功能就加一个脚本 + 网页加一个按钮，两者互不影响。好处是能脱离网页单独测试：

```sh
sh /data/adb/modules/chroot_manager/bin/tasks/start.sh    # 直接跑，看输出
```

慢任务（start / stop / restart / gpu）统一用**前台秒回 + 后台 worker** 的模式：

```sh
# 前台：打标记、起 worker、立刻返回
if [ "$1" != "--worker" ]; then
  if [ -f "$STATE/.task_running" ]; then
    echo "err 有任务正在跑（$(cat "$STATE/.task_running")），等它完成"
    exit 0
  fi
  echo start > "$STATE/.task_running"
  hist "开始启动桌面"
  setsid sh "$0" --worker > "$DATA/task.log" 2>&1 < /dev/null &
  echo "ok 正在启动，约 20-40 秒"
  exit 0
fi

# ---------- worker ----------
trap 'rm -f "$STATE/.task_running"' EXIT
# ... 真正的活 ...
```

三个要点：

1. `trap ... EXIT` 保证 worker 无论正常结束还是被杀都会清标记，否则界面永远显示"正在执行"。
2. 同一个脚本带 `--worker` 递归调用自己，避免复制两份逻辑。
3. `restart.sh` 自己不要管 `task_running`，起完 worker 立刻 `rm -f` 再依次调 stop/start 的 `--worker`，否则子任务会被自己的标记挡住。

最快的 `suspend.sh` 不需要后台（几十毫秒），前台同步返回结果就行，这样点一下立刻能看到"冻结 18 个"。

下面四组是实测机上跑着的版本，逐条改成了「换机器只改变量」的写法（工作区探测换成 `wsdir`、常驻服务字段换成通用的 `svc`），其余逻辑原样。照抄能跑。

#### status.sh（每 2 秒调一次，必须毫秒级）

```sh
#!/system/bin/sh
# 一行一个 key=value，给 WebUI 解析
# 铁律：不许 chroot，不许扫 /proc/*/root 全表，能 pgrep -x 就用 pgrep -x
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"

# 一次 pgrep 拿多个（多 pattern 是 OR 关系），约 36ms
kwin=; plasma=; svc=
for p in $(pgrep -x kwin_x11 plasmashell dotnet 2>/dev/null); do
  read -r c < "/proc/$p/comm" 2>/dev/null || continue
  case "$c" in
    kwin_x11)    [ -z "$kwin" ]   && kwin=$p ;;
    plasmashell) [ -z "$plasma" ] && plasma=$p ;;
    dotnet)      [ -z "$svc" ]    && svc=$p ;;   # 示例：常驻服务，没跑就删这行
  esac
done

# X server 的 comm 是 main（app_process 主线程名），只能按 cmdline 匹配
xs=$(pgrep -f 'termux-x11 [:]0' 2>/dev/null | head -1)

if is_frozen; then state=frozen
elif [ -n "$kwin" ]; then state=running
elif [ -n "$xs" ]; then state=half
else state=stopped
fi

mounts=$(awk -v p="$M/" '$2 ~ ("^" p)' /proc/mounts 2>/dev/null | wc -l)
[ -z "$mounts" ] && mounts=0

set -- $(df -h /data 2>/dev/null | tail -1)
disk=$4; diskpct=$5

set -- $(awk '/MemAvailable/{a=$2} /SwapTotal/{t=$2} /SwapFree/{f=$2}
  END{printf "%.1f %.1f", a/1048576, (t-f)/1048576}' /proc/meminfo)
mem=$1; swap=$2

echo "state=$state"
echo "xs=$xs"
echo "kwin=$kwin"
echo "plasma=$plasma"
echo "svc=$svc"
echo "mounts=$mounts"
echo "task=$(cat "$STATE/.task_running" 2>/dev/null)"
echo "gpu=$([ -f "$STATE/.gpu_running" ] && echo 1 || echo 0)"
echo "disk=$disk"
echo "diskpct=$diskpct"
echo "mem=$mem"
echo "swap=$swap"
echo "debug=1"
```

- `state` 四态：`running` / `frozen` / `half`（X 起着、桌面没拉起）/ `stopped`。
- `svc` 只是示例字段，没挂常驻服务就删掉对应那两行，UI 里也别显示。
- `disk`/`diskpct` 取自 `df -h /data` 的第 4、5 列，注意 a1 提到的「Mounted on 会骗人」，这里只取前五列所以不受影响。

#### start.sh（完整一条链，幂等）

```sh
#!/system/bin/sh
# 前台立刻返回，真活在 --worker 里跑，UI 轮询 status.sh 看进度
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"

if [ "$1" != "--worker" ]; then
  if [ -f "$STATE/.task_running" ]; then
    echo "err 有任务正在跑（$(cat "$STATE/.task_running")），等它完成"
    exit 0
  fi
  echo start > "$STATE/.task_running"
  hist "开始启动桌面"
  setsid sh "$0" --worker > "$DATA/task.log" 2>&1 < /dev/null &
  echo "ok 正在启动，约 20-40 秒"
  exit 0
fi

# ---------- worker ----------
trap 'rm -f "$STATE/.task_running"' EXIT

if is_frozen; then
  log "错误：现在是挂起状态，先点「解冻」"
  exit 2
fi
[ -d "$M" ] || { log "错误：rootfs 不存在 $M"; exit 1; }
[ -f "$XAPK" ] || { log "错误：缺 loader.apk $XAPK"; exit 1; }

log "== 1/5 基础挂载"
mnt $M/proc    mount -t proc proc $M/proc
mnt $M/sys     mount --bind /sys $M/sys
mnt $M/dev     mount --bind /dev $M/dev
mnt $M/dev/pts mount --bind /dev/pts $M/dev/pts
mnt $M/dev/shm mount -t tmpfs -o size=128M tmpfs $M/dev/shm
mnt $M/sdcard  mount --bind /storage/emulated/0 $M/sdcard

log "== 2/5 目录共享"
WSDIR=$(wsdir)
if [ -n "$WSDIR" ]; then
  mnt $M/mnt/ws mount --bind "$WSDIR" $M/mnt/ws
  log "  工作区: $WSDIR"
else
  log "  没找到工作区目录，/mnt/ws 跳过"
fi
if [ -d /data/data/com.termux/files/home/models ]; then
  mnt $M/mnt/termux mount --bind /data/data/com.termux/files/home/models $M/mnt/termux
fi
mnt $M/mnt/hosttmp mount --bind /data/local/tmp $M/mnt/hosttmp

log "== 3/5 X server"
chmod 777 "$XTMP" 2>/dev/null
mkdir -p "$M/tmp" "$XTMP/.X11-unix"
if [ -n "$(xs_pid)" ]; then
  log "  已在运行 (pid $(xs_pid))"
else
  # 残留的死 socket / lock 会让新 server bind 失败，卡在等 X0 然后退出
  rm -f "$XTMP/.X11-unix/X0" "$XTMP/.X0-lock" 2>/dev/null
  CLASSPATH=$XAPK \
  XKB_CONFIG_ROOT=$M/usr/share/X11/xkb \
  TMPDIR=$XTMP XDG_RUNTIME_DIR=$XTMP \
  setsid /system/bin/app_process -Xnoimage-dex2oat / \
      --nice-name="termux-x11 :0" com.termux.x11.Loader :0 \
      > "$M/tmp/tx11.log" 2>&1 < /dev/null &
  i=0
  while [ $i -lt 20 ]; do
    [ -S "$XTMP/.X11-unix/X0" ] && break
    sleep 1; i=$((i + 1))
  done
  [ -S "$XTMP/.X11-unix/X0" ] || {
    log "错误：X server 没起来，看 $M/tmp/tx11.log"
    tail -5 "$M/tmp/tx11.log" 2>/dev/null
    exit 1
  }
  log "  已启动 (pid $(xs_pid))"
fi

mnt $M/tmp/.X11-unix mount --bind $XTMP/.X11-unix $M/tmp/.X11-unix
[ -S "$M/tmp/.X11-unix/X0" ] || { log "错误：容器内看不到 X0 socket"; exit 1; }

log "== 4/5 KDE 桌面"
if desktop_alive; then
  log "  已在运行"
else
  rm -f "$M/tmp/.X0-lock" 2>/dev/null
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
  sleep 4
  log "  已拉起"
fi

log "== 5/5 之后这里挂你的常驻服务"
# 做了 b1（GPU）的机器，把这两行加回上面那个 env -i：
#   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/freedreno_kgsl.json \
#   VK_DRIVER_FILES=/usr/share/vulkan/icd.d/freedreno_kgsl.json \
# 没做 b1 就保持现在这样，别设成不存在的 ICD 路径。
#
# 常驻服务（b9/b10 那种）在这里拉，必须判重，模式带方括号：
# if [ -z "$(inbox "/usr/bin/pgrep -f 'App[.]dll'" 2>/dev/null)" ]; then
#   chroot $M /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root \
#     /bin/bash -c 'setsid /bin/bash /root/app/start.sh >> /tmp/app.log 2>&1 < /dev/null &'
# fi

log ""
log "== 完成  X=$(xs_pid) kwin=$(kwin_pid) plasma=$(plasma_pid)"
log "现在点开 Termux:X11 就是桌面"
hist "桌面启动完成"
```

- 5/5 那段在实测机上是 E5 Renew X 的启动 + 守护，这里抽成注释骨架，有常驻服务就照格式填。
- `VK_ICD_FILENAMES` 在实测机的版本里是开着的，本文改成按需启用：它指的是 b1 那个 ICD 文件，**没编 Turnip 就别设**，设了一个不存在的路径，容器里的 Vulkan 程序会找不到设备。

#### stop.sh（收工顺序，别打乱）

```sh
#!/system/bin/sh
# 停后台服务 → 停桌面 → 停 X server → 卸挂载
# 只卸本模块挂的那几个，不碰 /data/linuxroot 里的文件
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"

if [ "$1" != "--worker" ]; then
  if [ -f "$STATE/.task_running" ]; then
    echo "err 有任务正在跑（$(cat "$STATE/.task_running")），等它完成"
    exit 0
  fi
  echo stop > "$STATE/.task_running"
  hist "开始停止桌面"
  setsid sh "$0" --worker > "$DATA/task.log" 2>&1 < /dev/null &
  echo "ok 正在停止"
  exit 0
fi

trap 'rm -f "$STATE/.task_running"' EXIT

if is_frozen; then
  log "错误：现在是挂起状态，先点「解冻」再停止"
  exit 2
fi

log "== 1/4 停后台服务"
# 有守护进程的必须先停 guard，否则它立刻把服务拉回来。示例：
# for p in $(inbox "/usr/bin/pgrep -f 'app/guard[.]sh'" 2>/dev/null); do
#   inbox "/bin/kill -9 $p" 2>/dev/null && log "  guard 已停 (pid $p)"
# done
# for p in $(cpid dotnet); do
#   inbox "/bin/kill -9 $p" 2>/dev/null && log "  svc 已停 (pid $p)"
# done

log "== 2/4 停桌面"
# 容器里绝不能 pkill -f（会匹配到自己那条命令），一律按 pid 点名
for n in plasmashell kwin_x11; do
  for p in $(inbox "/usr/bin/pgrep -x $n" 2>/dev/null); do
    inbox "/bin/kill -9 $p" 2>/dev/null
  done
  [ -n "$p" ] && log "  $n 已停"
done

log "== 3/4 停 X server"
for p in $(pgrep -f 'termux-x11 [:]0' 2>/dev/null); do
  kill -9 "$p" 2>/dev/null && log "  X server 已停 (pid $p)"
done
rm -f "$XTMP/.X11-unix/X0" "$XTMP/.X0-lock" 2>/dev/null

log "== 4/4 卸挂载"
sleep 1
umount_tree
log "  已清理，剩余挂载: $(mount | grep -c " on $M/")"

log ""
log "== 已停止"
hist "桌面已停止"
```

- 顺序不能换：先停守护，再停桌面，最后才卸挂载。反过来的话进程还占着文件，`umount` 一律 busy。
- 停完必须 `rm` 掉 socket 和 lock，否则下次启动会卡在「等 X0」。

#### suspend.sh（挂起 / 解冻，同一个按钮切换）

```sh
#!/system/bin/sh
# 只冻 CPU，不释放内存。X server 必须一起冻：它是宿主进程，不冻会卡画面还烧 GPU。
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"

PIDLIST=$STATE/.pidlist
FREEZE=$STATE/.frozen

collect() {
  {
    ls -l /proc/[0-9]*/root 2>/dev/null \
      | grep -F -e "-> $M" \
      | grep -o -e "/proc/[0-9]*" \
      | cut -d/ -f3
    ps -A -o PID,NAME 2>/dev/null | awk '/termux-x11 :0/ {print $1}'
  } | sort -u | grep -v -x -e "$$"
}

is_stopped() {
  st=$(awk '{print $3}' /proc/$1/stat 2>/dev/null)
  [ "$st" = "T" ] || [ "$st" = "t" ]
}

# ---------- 解冻 ----------
if is_frozen; then
  {
    collect
    [ -f "$PIDLIST" ] && cat "$PIDLIST"
  } | sort -u > "$PIDLIST.tmp"

  ok=0; gone=0
  while read p; do
    [ -z "$p" ] && continue
    if [ -d "/proc/$p" ]; then
      kill -CONT "$p" 2>/dev/null && ok=$((ok + 1))
    else
      gone=$((gone + 1))
    fi
  done < "$PIDLIST.tmp"

  rm -f "$FREEZE" "$PIDLIST" "$PIDLIST.tmp"
  hist "解冻：恢复 $ok 个"
  echo "ok 已解冻，恢复运行 $ok 个进程$([ "$gone" -gt 0 ] && echo "（$gone 个已不存在）")"
  exit 0
fi

# ---------- 冻结 ----------
collect > "$PIDLIST.tmp"
total=$(grep -c . "$PIDLIST.tmp" 2>/dev/null)
if [ -z "$total" ] || [ "$total" -eq 0 ]; then
  rm -f "$PIDLIST.tmp"
  echo "err 没找到 chroot 进程（桌面可能还没启动）"
  exit 1
fi
mv "$PIDLIST.tmp" "$PIDLIST"

ok=0
while read p; do
  [ -z "$p" ] && continue
  kill -STOP "$p" 2>/dev/null && ok=$((ok + 1))
done < "$PIDLIST"

date +%s > "$FREEZE"

sleep 1
running=0
while read p; do
  [ -z "$p" ] && continue
  [ -d "/proc/$p" ] || continue
  is_stopped "$p" || running=$((running + 1))
done < "$PIDLIST"

hist "挂起：冻结 $ok/$total 个"
echo "ok 已挂起，冻结 $ok 个进程，CPU 占用归零$([ "$running" -gt 0 ] && echo "（$running 个还在等 IO）")"
```

- 这是唯一一个**前台同步跑**的任务，几十毫秒，不需要 `--worker`。
- 「还在等 IO」是正常的：卡在不可中断睡眠里的进程杀不进 T 态，解冻时一起放开就行。
- 冻结前先 `is_frozen` 判断，所以这个脚本既当挂起又当解冻，按钮只需要一个。

#### restart.sh（stop + start，共用一个标记）

```sh
#!/system/bin/sh
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"

if [ "$1" != "--worker" ]; then
  if [ -f "$STATE/.task_running" ]; then
    echo "err 有任务正在跑（$(cat "$STATE/.task_running")），等它完成"
    exit 0
  fi
  echo restart > "$STATE/.task_running"
  hist "开始重启桌面"
  setsid sh "$0" --worker > "$DATA/task.log" 2>&1 < /dev/null &
  echo "ok 正在重启，约 30-50 秒"
  exit 0
fi

trap 'rm -f "$STATE/.task_running"' EXIT

# 子任务会自己管 task_running，这里先摘掉，免得它们互相挡住
rm -f "$STATE/.task_running"
MODDIR=$MODDIR sh "$MODDIR/bin/tasks/stop.sh" --worker
sleep 2
MODDIR=$MODDIR sh "$MODDIR/bin/tasks/start.sh" --worker

hist "桌面重启完成"
```

#### gpu.sh / log.sh / open.sh（都短，一起给）

```sh
#!/system/bin/sh
# gpu.sh —— 必须后台跑，三条命令十几秒，同步跑会把 KSU 的 exec 通道堵死
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"
GPUFLAG=$STATE/.gpu_running
GPULOG=$DATA/gpu.log

if [ "$1" != "--worker" ]; then
  [ -f "$GPUFLAG" ] && { echo "err GPU 检测正在进行"; exit 0; }
  desktop_alive || { echo "err 桌面没运行，先启动桌面"; exit 1; }
  echo gpu > "$GPUFLAG"
  : > "$GPULOG"
  setsid sh "$0" --worker > /dev/null 2>&1 < /dev/null &
  echo "ok 正在检测 GPU，约 15 秒"
  exit 0
fi

trap 'rm -f "$GPUFLAG"' EXIT
# 函数名别用单字母：Android 的 mksh 把 r 定义成 history 别名（fc -e -），
# 叫 r 的函数永远不会执行，且不报错，只是日志里那行凭空消失
emit() { echo "$*" >> "$GPULOG"; }

emit "===== Vulkan ====="
inbox 'vulkaninfo --summary 2>/dev/null | grep -E "deviceName|driverName|apiVersion" | head -6' >> "$GPULOG" 2>&1
emit ""
emit "===== OpenGL ====="
inbox 'DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
  glxinfo -B 2>/dev/null | grep -E "Accelerated|Device:|Vendor:|OpenGL version" | head -6' >> "$GPULOG" 2>&1
emit ""
emit "===== glxgears 5 秒 ====="
inbox 'DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
  timeout 8 glxgears 2>/dev/null | tail -1' >> "$GPULOG" 2>&1
emit ""
emit "== 检测完成"
hist "GPU 检测完成"
```

```sh
#!/system/bin/sh
# log.sh —— 只吐最后 40 行，别把大文件整个塞进 WebView
export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"
tail_n() { [ -f "$1" ] && tail -40 "$1" 2>/dev/null || echo "（无日志）"; }
case "$1" in
  history) tail -40 "$DATA/history.log" 2>/dev/null || echo "（无记录）" ;;
  gpu)     tail_n "$DATA/gpu.log" ;;
  kwin)    tail_n "$M/tmp/kwin.log" ;;
  plasma)  tail_n "$M/tmp/pshell.log" ;;
  tx11)    tail_n "$M/tmp/tx11.log" ;;
  *)       [ -f "$DATA/task.log" ] && cat "$DATA/task.log" || echo "（还没有动作）" ;;
esac
```

```sh
#!/system/bin/sh
# open.sh <目标> —— 打开外部 App / 网页
case "$1" in
  x11)
    am start -n com.termux.x11/.MainActivity > /dev/null 2>&1 \
      && echo "ok 已打开 Termux:X11" || echo "err 打开失败，手动点一下 App" ;;
  *)
    echo "err 用法: open.sh x11" ;;
esac
```

- `open.sh` 的包名 `com.termux.x11` 是固定的，不用改。要加别的目标就照 `x11)` 那条抄一段。



#### audio.sh（音频单独启停 + 跟随开关）

桌面和音频不是绑死的。把音频拆成独立任务，WebUI 上给一组「启动音频 / 停止音频」按钮，再加一个「跟随容器启动」复选框。勾上时点「启动桌面」会自动把音频带起来；取消勾选，启动桌面就只管桌面，音频要手动点。

状态存一个文件，不存在就当开。`audio_follow()` 已经在 lib.sh 里定义（见 a7.4），**别在 audio.sh 里再定义一遍**，同名函数两处定义，改一处忘一处最难查：

文件内容 `0` 为关、没有这个文件按开处理，判断逻辑就是 `[ "$(cat "$STATE/audio.follow" 2>/dev/null)" = "0" ]`（完整函数在 lib.sh，见 a7.4）。

`start.sh` 的第 5 步用这个函数包一层，关闭时直接跳过：

```sh
log "== 5/6 音频桥"
if [ "$(audio_follow)" != "1" ]; then
  log "  已关闭「跟随容器启动」，跳过（WebUI 里可点「启动音频」）"
else
  : # 这里换回原来的 pulse + socket 等待 + bridge 三步；光放注释不行，dash 的 else 分支至少要一条命令
fi
```

`status.sh` 只加**一次** `pgrep` 和两个字段，别重启一个进程扫描，否则 2 秒一次的轮询会变慢：

```sh
kwin=; plasma=; svc=; abridge=
for p in $(pgrep -x kwin_x11 plasmashell dotnet aaudio_bridge_u 2>/dev/null); do
  read -r c < "/proc/$p/comm" 2>/dev/null || continue
  case "$c" in
    kwin_x11)        [ -z "$kwin" ]    && kwin=$p ;;
    plasmashell)     [ -z "$plasma" ]  && plasma=$p ;;
    dotnet)          [ -z "$svc" ]     && svc=$p ;;
    aaudio_bridge_u) [ -z "$abridge" ] && abridge=$p ;;
  esac
done
...
echo "audio=$([ -n "$abridge" ] && echo 1 || echo 0)"
echo "audioFollow=$(audio_follow)"
```

完整脚本：

```sh
#!/system/bin/sh
# 音频服务：容器 PulseAudio + 宿主 AAudio 桥，可单独启停
# 用法: audio.sh start | stop | follow on|off
# 前台立刻返回，真正的活在 --worker-* 里跑，UI 靠 status.sh 的 audio 字段轮询

export MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
. "$MODDIR/bin/lib.sh"

SOCK_HOST=/data/linuxroot/tmp/runtime-root/pulse/aaudio-bridge.sock

# 宿主侧桥进程。comm 只有 15 字节，aaudio_bridge_unix 被截成 aaudio_bridge_u
abridge_pid() {
  for p in $(pgrep -x aaudio_bridge_u aaudio_bridge 2>/dev/null); do
    echo "$p"; return 0
  done
}

start_pulse() {
  if [ -n "$(cpulse_pid)" ]; then
    log "  PulseAudio 已在跑 (pid $(cpulse_pid))"
    return 0
  fi
  chroot $M /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root \
    XDG_RUNTIME_DIR=/tmp/runtime-root \
    /bin/bash -c 'mkdir -p /tmp/runtime-root/pulse && chmod 700 /tmp/runtime-root /tmp/runtime-root/pulse; /usr/bin/pulseaudio --daemonize --exit-idle-time=-1 --log-target=file:/tmp/pulse.log >/dev/null 2>&1'
  sleep 2
  if [ -n "$(cpulse_pid)" ]; then
    log "  PulseAudio 已启动 (pid $(cpulse_pid))"
  else
    log "  PulseAudio 没起来，看 /tmp/pulse.log"
    return 1
  fi
}

start_bridge() {
  if [ -n "$(abridge_pid)" ]; then
    log "  音频桥已在跑 (pid $(abridge_pid))"
    return 0
  fi
  if [ ! -x "$MODDIR/bin/aaudio_bridge_unix" ]; then
    log "  缺 aaudio_bridge_unix，跳过"
    return 1
  fi
  setsid "$MODDIR/bin/aaudio_bridge_unix" "$SOCK_HOST" 48000 2 30000 \
    >> /data/local/tmp/bridge.log < /dev/null &
  sleep 1
  if [ -n "$(abridge_pid)" ]; then
    log "  音频桥已启动 (pid $(abridge_pid))"
  else
    log "  音频桥没起来，看 /data/local/tmp/bridge.log"
    return 1
  fi
}

case "$1" in
  follow)
    case "$2" in
      on|1)  echo 1 > "$STATE/audio.follow"; echo "ok 已开启跟随容器启动" ;;
      off|0) echo 0 > "$STATE/audio.follow"; echo "ok 已关闭跟随容器启动" ;;
      *)     echo "err 用法: audio.sh follow on|off" ;;
    esac
    ;;

  start)
    if [ -n "$(abridge_pid)" ] && [ -n "$(cpulse_pid)" ]; then
      echo "ok 音频服务已在运行"
      exit 0
    fi
    if [ -f "$STATE/.audio_task" ]; then
      echo "err 音频任务正在跑，稍等"
      exit 0
    fi
    echo start > "$STATE/.audio_task"
    setsid sh "$0" --worker-start >> "$DATA/task.log" 2>&1 < /dev/null &
    echo "ok 正在启动音频"
    ;;

  stop)
    if [ -f "$STATE/.audio_task" ]; then
      echo "err 音频任务正在跑，稍等"
      exit 0
    fi
    echo stop > "$STATE/.audio_task"
    setsid sh "$0" --worker-stop >> "$DATA/task.log" 2>&1 < /dev/null &
    echo "ok 正在停止音频"
    ;;

  --worker-start)
    trap 'rm -f "$STATE/.audio_task"' EXIT
    hist "启动音频服务"
    log "== 音频服务：启动"
    start_pulse
    _n=0
    while [ $_n -lt 16 ] && [ ! -S "$SOCK_HOST" ]; do
      sleep 0.5
      _n=$((_n + 1))
    done
    [ -S "$SOCK_HOST" ] || log "  警告：$SOCK_HOST 未出现，桥会持续重连"
    start_bridge
    log "== 音频服务：完成"
    ;;

  --worker-stop)
    trap 'rm -f "$STATE/.audio_task"' EXIT
    hist "停止音频服务"
    log "== 音频服务：停止"
    for p in $(abridge_pid); do
      kill -9 "$p" 2>/dev/null && log "  音频桥已停 (pid $p)"
    done
    for p in $(cpulse_pid); do
      kill -9 "$p" 2>/dev/null && log "  容器 PulseAudio 已停 (pid $p)"
    done
    ;;

  *)
    echo "err 用法: audio.sh start|stop|follow on|off"
    ;;
esac
```

三个必须注意的点：

1. **前台立刻返回**。启动要 3 到 4 秒（pulse 2 秒 + socket 等待 + bridge 1 秒），同步跑会撞 KSU exec 的超时，表现是按钮转圈然后报错。所以和 start.sh 一样走 `--worker-*` 后台模式。
2. **已在运行就直说**，不要重建。用户连点两次不能出现两个 bridge 抢同一个 socket。
3. **停止顺序反过来**：先桥后 pulse。反过来会让桥在 socket 消失后不断重连，日志刷屏。

WebUI 侧对应改四处：按钮、`CMD` 映射、`LABEL` 映射、`paint()` 里同步复选框。复选框提交期间要有个 `followBusy` 标志，否则 2 秒一次的轮询会把用户刚点的状态覆盖回去。

### a7.6 webroot/index.html

完整文件如下，直接写到 `$MOD/webroot/index.html`，权限 644（见 a7.0）。`M`、`SH`、`CMD`、`LOGARG` 这些常量全在里面，按钮和脚本的对应关系只在这一处，加功能就是往 `CMD` 和 `LABEL` 两张表里各加一行。

```html
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>chroot</title>
<style>
:root{
  --bg:#0b0e11; --card:#161d25; --line:#2c3641; --fg:#e9eef3; --dim:#78838f;
  --acc:#3ad1ff; --ok:#4ade80; --warn:#ffb43a; --bad:#ff5f5f;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;padding:0}
body{
  background:var(--bg);color:var(--fg);
  font-family:-apple-system,system-ui,"Noto Sans SC",sans-serif;
  padding:20px 16px 30px;overflow-x:hidden;
}
.mono{font-family:ui-monospace,"SF Mono",Menlo,monospace;font-variant-numeric:tabular-nums}

.top{display:flex;align-items:center;justify-content:space-between;margin-bottom:24px}
.tag{font-size:11px;letter-spacing:.22em;color:var(--dim)}
.state{display:flex;align-items:center;gap:7px;font-size:12px;color:var(--dim)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--dim);transition:background .2s,box-shadow .2s}
.dot.running{background:var(--ok);box-shadow:0 0 10px rgba(74,222,128,.75)}
.dot.frozen{background:var(--acc);box-shadow:0 0 10px rgba(58,209,255,.75)}
.dot.half{background:var(--warn);box-shadow:0 0 10px rgba(255,180,58,.6)}
.dot.stopped{background:#48525c}
.dot.lost{background:var(--bad);box-shadow:0 0 10px rgba(255,95,95,.6)}

.hero{display:flex;align-items:flex-end;gap:9px;margin-bottom:5px}
.num{font-size:48px;line-height:.9;font-weight:600;letter-spacing:-.03em}
.unit{font-size:13px;color:var(--dim);padding-bottom:6px}
.sub{font-size:12px;color:var(--dim);height:34px;line-height:1.45;margin-bottom:14px}

.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}

/* ---------- 按钮基础：可点 = 有立体感 ---------- */
button{
  font-family:inherit;font-size:14.5px;font-weight:600;letter-spacing:.01em;
  color:#f0f4f8;
  background:linear-gradient(180deg,#212b35 0%,#171f27 100%);
  border:1px solid #3a4753;
  border-radius:13px;
  padding:17px 8px;
  cursor:pointer;
  box-shadow:
    inset 0 1px 0 rgba(255,255,255,.09),
    0 2px 5px rgba(0,0,0,.45);
  transition:background .1s,border-color .1s,transform .06s,box-shadow .1s;
  user-select:none;
}
button:active{
  background:linear-gradient(180deg,#111820 0%,#1a2430 100%);
  border-color:#48586a;
  transform:translateY(1px) scale(.985);
  box-shadow:inset 0 2px 7px rgba(0,0,0,.55);
}

/* 启动：绿 */
button.p{
  border-color:#2e6b4c;
  background:linear-gradient(180deg,#1c3a2b 0%,#132a20 100%);
  color:#a8f0c0;
}
button.p:active{background:linear-gradient(180deg,#10251b 0%,#1a3629 100%)}

/* 停止：红 */
button.d{
  border-color:#77393a;
  background:linear-gradient(180deg,#3a1f21 0%,#2a1618 100%);
  color:#ffb3b3;
}
button.d:active{background:linear-gradient(180deg,#2a1416 0%,#3a1f21 100%)}

/* 次一级操作：蓝 */
button.s{
  border-color:#2f5670;
  background:linear-gradient(180deg,#1b3140 0%,#131f2b 100%);
  color:#a8d8f0;
}
button.s:active{background:linear-gradient(180deg,#0f1e2a 0%,#1b3140 100%)}

/* 小按钮（日志类）：稍微素一点但依然明确可点 */
button.small{
  font-size:13px;font-weight:500;padding:13px 8px;
  color:#c8d3dd;
  background:linear-gradient(180deg,#1c242d 0%,#151c24 100%);
  border-color:#333f4c;
}

/* ---------- 不可点：明显塌下去、虚线、灰字 ---------- */
button:disabled,
button.p:disabled,
button.d:disabled,
button.s:disabled{
  background:#0d1218;
  background-image:none;
  border:1px dashed #232c35;
  color:#4b555f;
  box-shadow:none;
  transform:none;
  cursor:default;
  opacity:1;
}

/* 正在执行的那个：呼吸边框 */
@keyframes pulse{
  0%,100%{border-color:#3ad1ff;box-shadow:inset 0 1px 0 rgba(255,255,255,.09),0 0 0 0 rgba(58,209,255,.35)}
  50%{border-color:#6ee0ff;box-shadow:inset 0 1px 0 rgba(255,255,255,.09),0 0 0 3px rgba(58,209,255,.22)}
}
button.busy{
  border-style:solid !important;
  color:#bfe9ff !important;
  background:linear-gradient(180deg,#132430 0%,#0e1a24 100%) !important;
  animation:pulse 1.1s ease-in-out infinite;
}

.cap{margin:24px 0 9px;font-size:11px;letter-spacing:.18em;color:var(--dim)}
.chk{display:flex;align-items:center;gap:9px;margin-top:11px;font-size:12px;color:var(--dim);user-select:none}
.chk input{width:16px;height:16px;margin:0;flex:none}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:1px;background:var(--line);border:1px solid var(--line);border-radius:13px;overflow:hidden}
.cell{background:var(--card);padding:13px 10px}
.k{font-size:11px;color:var(--dim);margin-bottom:5px}
.v{font-size:18px;font-weight:600;letter-spacing:-.02em}
.v i{font-size:11px;color:var(--dim);font-weight:400;font-style:normal;margin-left:2px}

.logbox{
  border:1px solid var(--line);border-radius:13px;background:#080b0e;
  padding:12px;font-family:ui-monospace,monospace;font-size:11.5px;
  line-height:1.6;color:#9aa6b2;white-space:pre-wrap;word-break:break-all;
  max-height:290px;overflow:auto;margin-top:9px;
}
.logbox.blank{color:#3d4650}
.foot{position:fixed;left:0;right:0;bottom:0;padding:5px 16px 7px;font-size:10px;color:#3d4650;background:var(--bg);word-break:break-all}
</style>
</head>
<body>

<div class="top">
  <div class="tag">CHROOT · v3</div>
  <div class="state"><div class="dot" id="dot"></div><span id="stateText">连接中</span></div>
</div>

<div class="hero">
  <div class="num mono" id="num">--</div>
  <div class="unit">挂载</div>
</div>
<div class="sub mono" id="sub"></div>

<div class="grid">
  <button class="p" data-act="start">启动桌面</button>
  <button class="d" data-act="stop">停止桌面</button>
  <button class="s" data-act="restart">重启桌面</button>
  <button class="s" id="btnSuspend" data-act="suspend">挂起</button>
  <button data-act="gpu">GPU 检测</button>
  <button data-act="x11">打开 X11</button>
</div>

<div class="cap">AUDIO</div>
<div class="grid">
  <button class="s" data-act="audStart">启动音频</button>
  <button class="d" data-act="audStop">停止音频</button>
</div>
<label class="chk"><input type="checkbox" id="ckFollow"><span>跟随容器启动</span></label>

<div class="cap">SYSTEM</div>
<div class="stats">
  <div class="cell"><div class="k">剩余空间</div><div class="v mono" id="sDisk">-</div></div>
  <div class="cell"><div class="k">可用内存</div><div class="v mono" id="sMem">-<i>G</i></div></div>
  <div class="cell"><div class="k">Swap 已用</div><div class="v mono" id="sSwap">-<i>G</i></div></div>
</div>

<div class="cap">OUTPUT</div>
<div class="grid">
  <button class="small" data-log="task">本次输出</button>
  <button class="small" data-log="history">操作记录</button>
  <button class="small" data-log="kwin">kwin 日志</button>
  <button class="small" data-log="tx11">X server 日志</button>
</div>
<div class="logbox blank" id="log">（还没有动作）</div>

<div class="foot mono" id="foot"></div>

<script>
var M  = '/data/adb/modules/chroot_manager';
var SH = '/system/bin/sh';

function $(id){ return document.getElementById(id); }

var seq = 0;
var lastRaw = '';
var busyAct = null;      // 正在执行的动作名，非空时锁住所有动作按钮
var followBusy = 0;      // 复选框正在提交时，别让轮询把它抢回去
var polling = false;     // 有没有 status 请求在飞
var consecFail = 0;      // 连续失败次数
var timer = null;

/* ---------- KernelSU exec 封装 ----------
   ksu.exec(cmd, JSON选项, 回调名)，回调挂在 window 上，
   native 侧按名字回调 window[name](errno, stdout, stderr)。
   关键：回调有可能永远不来（WebView 切后台时 JS 被挂起），
   所以每个调用都自带超时，绝不让 Promise 永远挂着。 */
function ksuObj(){
  if (typeof ksu !== 'undefined' && ksu && typeof ksu.exec === 'function') return ksu;
  return null;
}

function exec(cmd, timeoutMs){
  var k = ksuObj();
  if (!k) return Promise.resolve(null);
  var name = 'ksu_cb_' + Date.now() + '_' + (seq++);
  var t = timeoutMs || 4000;
  return new Promise(function(resolve){
    var done = false;
    function finish(r){
      if (done) return;
      done = true;
      clearTimeout(tm);
      try { delete window[name]; } catch(e){}
      resolve(r);
    }
    var tm = setTimeout(function(){ lastRaw = 'timeout'; finish(null); }, t);
    window[name] = function(errno, stdout, stderr){
      lastRaw = 'ok';
      finish({
        code: errno | 0,
        out: stdout == null ? '' : String(stdout),
        err: stderr == null ? '' : String(stderr)
      });
    };
    try {
      k.exec(cmd, JSON.stringify({cwd:'/'}), name);
    } catch(e){
      lastRaw = 'throw:' + e;
      finish(null);
    }
  });
}

/* ---------- 解析 ---------- */
function parse(out){
  var d = {}, lines = (out || '').split('\n');
  for (var i = 0; i < lines.length; i++){
    var j = lines[i].indexOf('=');
    if (j > 0) d[lines[i].slice(0, j).trim()] = lines[i].slice(j + 1).trim();
  }
  return d;
}

var ST = {
  running:['运行中','桌面已就绪，点开 Termux:X11 就是桌面'],
  frozen :['已挂起','CPU 已归零，点「解冻」恢复'],
  half   :['半启动','X 起着但桌面没拉起来，点「启动桌面」补上'],
  stopped:['未运行','全部停止，点「启动桌面」开始']
};

function paint(d){
  var t = ST[d.state] || ['未知',''];
  $('dot').className = 'dot ' + (d.state || 'stopped');
  $('stateText').textContent = t[0];
  if (!busyAct) {
    $('sub').textContent = d.task ? ('正在执行：' + d.task + '，稍等') : t[1];
  }
  $('num').textContent = d.mounts || '0';
  $('sDisk').textContent = d.disk || '-';
  $('sMem').innerHTML = (d.mem || '-') + '<i>G</i>';
  $('sSwap').innerHTML = (d.swap || '-') + '<i>G</i>';

  // 复选框跟随后端状态；用户正在提交时不覆盖他的操作
  if (!followBusy) $('ckFollow').checked = (d.audioFollow === '1');

  var b = $('btnSuspend');
  var froz = (d.state === 'frozen');
  if (busyAct !== 'suspend'){
    b.textContent = froz ? '解冻' : '挂起';
    b.className = froz ? 'p' : 's';
  }
  connected(true);
  return d;
}

function connected(ok){
  if (ok) { consecFail = 0; $('foot').textContent = ''; return; }
  consecFail++;
  if (consecFail >= 3){
    $('dot').className = 'dot lost';
    $('stateText').textContent = '无响应';
    $('sub').textContent = 'exec 通道没回应。切出去再进来，或者退出重进 WebUI';
  }
}

/* ---------- 刷新 ---------- */
function refresh(){
  if (polling) return Promise.resolve(null);
  polling = true;
  return exec(SH + ' ' + M + '/bin/tasks/status.sh', 4000).then(function(r){
    polling = false;
    if (!r || !r.out) { connected(false); return null; }
    var d = parse(r.out);
    if (!d.debug) { connected(false); return null; }
    return paint(d);
  }).catch(function(e){
    polling = false;
    connected(false);
    return null;
  });
}

/* GPU 检测跑在后台，这里盯着 status 里的 gpu 字段，
   等它从 1 变 0 就说明跑完了，再去读 gpu.log。不阻塞任何按钮。 */
function watchGpu(){
  clearTimeout(gpuWatch);
  gpuActive = true;
  var btn = document.querySelector('button[data-act="gpu"]');
  if (btn) { btn.classList.add('busy'); btn.textContent = '检测中…'; btn.disabled = true; }

  function done(){
    gpuActive = false;
    clearTimeout(gpuWatch);
    gpuWatch = null;
    if (btn) { btn.classList.remove('busy'); btn.textContent = 'GPU 检测'; btn.disabled = false; }
    exec(SH + ' ' + M + '/bin/tasks/log.sh gpu', 5000).then(function(lr){
      showLog(lr ? lr.out : '（读取结果失败）');
    });
    refresh();
  }

  function tick(){
    exec(SH + ' ' + M + '/bin/tasks/status.sh', 4000).then(function(r){
      var d = r && r.out ? parse(r.out) : null;
      if (d && d.gpu === '1'){
        $('sub').textContent = '正在检测 GPU…';
        gpuWatch = setTimeout(tick, 1500);
        return;
      }
      // 标记没了就是跑完了。拿不到状态也算结束，别让按钮一直转
      $('sub').textContent = 'GPU 检测完成';
      done();
    });
  }
  // 给 worker 一点时间建标记，再开始盯
  gpuWatch = setTimeout(tick, 900);
}

function loop(delay){
  clearTimeout(timer);
  timer = setTimeout(function(){
    refresh().then(function(){ loop(2000); });
  }, delay === undefined ? 2000 : delay);
}

/* ---------- 动作 ---------- */
var CMD = {
  start:   SH + ' ' + M + '/bin/tasks/start.sh',
  stop:    SH + ' ' + M + '/bin/tasks/stop.sh',
  restart: SH + ' ' + M + '/bin/tasks/restart.sh',
  suspend: SH + ' ' + M + '/bin/tasks/suspend.sh',
  gpu:     SH + ' ' + M + '/bin/tasks/gpu.sh',
  x11:     SH + ' ' + M + '/bin/tasks/open.sh x11',
  audStart: SH + ' ' + M + '/bin/tasks/audio.sh start',
  audStop:  SH + ' ' + M + '/bin/tasks/audio.sh stop'
};
var LABEL = {
  start:'启动桌面', stop:'停止桌面', restart:'重启桌面',
  suspend:'挂起/解冻', gpu:'检测 GPU', x11:'打开 X11',
  audStart:'启动音频', audStop:'停止音频'
};

var gpuWatch = null;   // GPU 检测的轮询定时器
var gpuActive = false; // GPU 检测是否进行中

function lockBtns(v){
  var bs = document.querySelectorAll('button[data-act]');
  for (var i = 0; i < bs.length; i++){
    // GPU 检测进行中时，它自己单独管，别被别的动作抢回去
    if (v === false && bs[i].dataset.act === 'gpu' && gpuActive) continue;
    bs[i].disabled = v;
  }
}

function run(a){
  if (busyAct) return;
  busyAct = a;

  // 乐观反馈：先改 UI，不等回调
  $('sub').textContent = '正在' + LABEL[a] + '…';
  var btn = document.querySelector('button[data-act="' + a + '"]');
  var oldTxt = btn ? btn.textContent : '';
  lockBtns(true);
  if (btn) { btn.textContent = oldTxt + '…'; btn.classList.add('busy'); }

  function unlock(){
    busyAct = null;
    lockBtns(false);
    if (btn) {
      btn.classList.remove('busy');
      if (btn.dataset.act !== 'suspend') btn.textContent = oldTxt;
    }
  }

  // 不管回调来不来，1.8 秒后都解锁，绝不把界面卡死
  var unlockTimer = setTimeout(function(){
    unlock();
    $('sub').textContent = LABEL[a] + ' 已提交，等待状态更新…';
    loop(600);
  }, 1800);

  exec(CMD[a], 6000).then(function(r){
    clearTimeout(unlockTimer);
    var first = '';
    if (r && r.out) first = (r.out.trim().split('\n')[0] || '');
    var isErr = first.indexOf('err') === 0;
    if (first){
      $('sub').textContent = first.replace(/^(ok|err)\s*/, '') || (LABEL[a] + ' 已提交');
    } else {
      $('sub').textContent = LABEL[a] + ' 已提交';
    }
    unlock();
    if (a === 'gpu'){
      if (isErr || !r) {
        loop(2000);
      } else {
        watchGpu();
        loop(2000);
      }
    } else if (a === 'x11'){
      // 打开 App 立即出结果
      if (r && r.out) showLog(r.out);
      refresh();
      loop(2000);
    } else {
      loop(700);
    }
  });
}

/* ---------- 日志 ---------- */
var LOGARG = { task:'', history:'history', gpu:'gpu', kwin:'kwin', tx11:'tx11' };

function showLog(txt){
  var box = $('log');
  txt = (txt || '').trim();
  box.textContent = txt || '（空）';
  box.className = 'logbox' + (txt ? '' : ' blank');
  box.scrollTop = box.scrollHeight;
}

function loadLog(kind){
  var arg = LOGARG[kind] === undefined ? '' : LOGARG[kind];
  $('log').textContent = '读取中…';
  exec(SH + ' ' + M + '/bin/tasks/log.sh ' + arg, 5000).then(function(r){
    showLog(r ? r.out : '（读取失败，切出去再进来试试）');
  });
}

/* ---------- 事件 ---------- */
document.addEventListener('click', function(e){
  var b = e.target.closest('button');
  if (!b || b.disabled) return;
  if (b.dataset.log !== undefined) { loadLog(b.dataset.log); return; }
  if (b.dataset.act) run(b.dataset.act);
});

window.onerror = function(msg){
  $('foot').textContent = 'js err: ' + msg;
};

/* ---------- 启动 ----------
   KSU 的 ksu 对象是页面加载后注入的，可能晚几十毫秒。
   先轮询等它出现，出现之前不要狂发请求。 */
var bootTries = 0;
function boot(){
  bootTries++;
  if (!ksuObj()){
    if (bootTries < 20){
      $('sub').textContent = '等待注入接口…';
      setTimeout(boot, 200);
    } else {
      $('dot').className = 'dot lost';
      $('stateText').textContent = '接口不可用';
      $('sub').textContent = '没拿到 ksu.exec。确认是在 KSU 管理器里打开的 WebUI';
      $('foot').textContent = 'ksu=' + (typeof ksu);
    }
    return;
  }
  $('sub').textContent = '读取状态…';
  refresh().then(function(d){
    if (d) showLog('（点上面的按钮操作）');
    loadLog('task');
    loop(2000);
  });
}
/* ---------- 音频跟随开关 ---------- */
$('ckFollow').addEventListener('change', function(){
  var on = this.checked;
  followBusy = 1;
  exec(SH + ' ' + M + '/bin/tasks/audio.sh follow ' + (on ? 'on' : 'off'), 4000).then(function(r){
    followBusy = 0;
    var first = (r && r.out) ? (r.out.trim().split('\n')[0] || '') : '';
    $('sub').textContent = first.replace(/^(ok|err)\s*/, '') || '已保存';
    refresh();
  }).catch(function(){
    followBusy = 0;
  });
});

boot();
</script>
</body>
</html>
```

- 三处保命逻辑都在文件里，别删：每个 `exec` 自带超时（回调可能永远不来）、点按钮先乐观改 UI 并在 1.8 秒后无条件解锁、轮询用 `polling` 标志防堆积。理由见 a7.7。
- `paint()` 读的字段名（`state` / `task` / `mounts` / `disk` / `mem` / `swap` / `audioFollow` / `gpu`）必须和 a7.5 的 `status.sh` 输出一致；`refresh()` 拿不到 `debug=1` 就判成无响应。改一边必须改另一边。
- 复选框提交期间靠 `followBusy` 挡住 2 秒一次的轮询，否则用户刚点的状态会被覆盖回去。状态文件是 `$STATE/audio.follow`（见 b11.4）。

### a7.7 WebUI 四个必须当真的坑

这四个每一个都真实咬过一次，症状都很像「界面卡死/按钮没反应」，但根因完全不同。

**① status.sh 必须毫秒级，绝对不能 chroot。**

它是每 2 秒被调一次的。第一版里它调了三次 chroot（`chroot ... pgrep -x kwin_x11` 之类），每次 80ms，加上扫 `/proc/*/root` 全表 255ms，单次 640ms。看着还好，但 WebView 那边回调一慢，请求就堆积，你点按钮时排在一堆 status 后面 → **表现就是「点了没反应，切出去再切回来才动」**。

优化路线（实测）：

```
640ms  三次 chroot + ls|grep|grep|cut 扫全表 + 两次 df + 两次 awk meminfo
345ms  去掉 chroot，改扫 /proc；awk 一步顶掉三个 fork
122ms  进程识别改用一次 pgrep -x（多 pattern 是 OR 关系）+ 读 /proc/pid/comm
```

最终 `status.sh` 里不许出现 `chroot`，也不许全表扫。`pgrep -x kwin_x11 plasmashell dotnet` 一次拿三个只要 36ms。X server 因为 comm 是 `main`（app_process 主线程名），只能 `pgrep -f 'termux-x11 [:]0'`。

顺带：`df -h /data` 和 `/proc/meminfo` 各调一次就够，`set -- $(...)` 拆字段比重复调用快。

**② 慢任务必须后台跑，否则会把 exec 通道堵死。**

GPU 检测第一版是同步的，`vulkaninfo` + `glxinfo` + `glxgears` 十几秒，点完之后整个界面卡到出结果为止。改成前台秒回（实测 469ms）+ 后台 worker + `status.sh` 里多一个 `gpu=0/1` 字段，前端每 1.5 秒瞄一眼，标记没了再读 `gpu.log`。

**③ 前端必须防堆积，且不能等回调才改 UI。**

```js
var polling = false;   // 有请求在飞就不再发
function refresh(){
  if (polling) return Promise.resolve(null);
  polling = true;
  return exec(...).then(function(r){ polling = false; ... });
}
```

点按钮时**先乐观改界面**（文字变「…」、按钮加呼吸动画、其余按钮变灰），再发命令，并且 1.8 秒后**无论回调来没来都解锁**。绝不能「等回调再解锁」——回调丢了界面就永久假死。

**④ mksh 的 `alias r` 会把你的函数静默吃掉。**

```sh
r() { echo "$*" >> "$LOG"; }     # ★ 这个函数永远不会执行
r "===== Vulkan ====="            # 报 fc: history functions not available
```

Android 的 `/system/bin/sh` 是 mksh，内置 `alias r='fc -e -'`。别名在解析阶段就替换，函数定义被绕过，而且**不报错只是日志里那行凭空消失**。函数名一律用有意义的词：`emit` / `say` / `logit` / `note`。

### a7.8 验收

```sh
M=/data/adb/modules/chroot_manager
ksud module list | grep -A3 chroot_manager        # 要有 "web": "true"
for f in $M/bin/lib.sh $M/bin/tasks/*.sh $M/uninstall.sh; do sh -n "$f"; done

# 每个任务都能脱离网页单独跑
sh $M/bin/tasks/status.sh
sh $M/bin/tasks/start.sh ; sleep 25 ; sh $M/bin/tasks/status.sh
sh $M/bin/tasks/suspend.sh ; sh $M/bin/tasks/suspend.sh
sh $M/bin/tasks/gpu.sh ; sleep 15 ; sh $M/bin/tasks/log.sh gpu
sh $M/bin/tasks/restart.sh ; sleep 30 ; sh $M/bin/tasks/status.sh
sh $M/bin/tasks/stop.sh ; sleep 10 ; sh $M/bin/tasks/status.sh
```

正常的 status 输出：

```
state=running
xs=5702
kwin=6249
plasma=6804
svc=7993
mounts=10
task=
gpu=0
disk=21G
diskpct=98%
mem=8.7
swap=1.0
debug=1
```

`state` 四种：`running`（桌面活着）、`frozen`（挂起中）、`half`（X 起着但桌面没拉起）、`stopped`（全停）。

然后进管理器点模块卡片的 `<> 打开`，能看到界面并操作，才算完。

**UI 视觉上有一件事必须做对**：可点和不可点要一眼分得出来。全深色底 + 细边框那一版，禁用只有 `opacity: .4`，实际根本看不出区别。做法是给可点按钮加渐变+内高光+外投影（有立体感），禁用态改成 `background:#0d1218` + `border:1px dashed` + 灰字 + `box-shadow:none`（明显塌下去）。正在执行的那个再叠一层呼吸动画。

## a8：A 部分验收

```sh
# X server 与桌面
pgrep -f "termux-x11 :0"
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin /bin/bash -c 'pgrep -x kwin_x11; pgrep -x plasmashell'
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin /bin/bash -c 'DISPLAY=:0 xdpyinfo | head -3'

# socket
ls -l /data/data/com.termux/files/usr/tmp/.X11-unix/
ls -l /data/linuxroot/tmp/.X11-unix/
mount | grep X11-unix

# 不连客户端直接看画面（最快的验证手段）
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root DISPLAY=:0 /bin/bash -c 'scrot -o /tmp/desk.png'
# 三条落点，按有没有 agent 挑一条
# (a) 有 agent 且挂了 /mnt/ws：直接扔进工作区，比记 uuid 省事
cp /data/linuxroot/tmp/desk.png /data/linuxroot/mnt/ws/desk-$(date +%H%M%S).png
# (b) 有 agent 但没挂 /mnt/ws：退回工作区真路径（WS_SRC 见 a3）
[ -n "$WS_SRC" ] && cp /data/linuxroot/tmp/desk.png "$WS_SRC/"
# (c) 没有 agent：直接落 /sdcard，文件管理器/相册直接能看，不依赖任何 App
cp /data/linuxroot/tmp/desk.png /sdcard/Download/desk-$(date +%H%M%S).png

# 桌面进程有没有拿到语言（拿不到就会是英文界面）
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin /bin/bash -c \
  'tr "\0" "\n" < /proc/$(pgrep -x plasmashell)/environ | grep -E "^(LANG|LANGUAGE)"'
# 期望 LANG=zh_CN.UTF-8 和 LANGUAGE=zh_CN:en

# 看 X 上有哪些窗口
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin DISPLAY=:0 /bin/bash -c 'xwininfo -root -tree | head -20'
```

- 容器里唯一现成的截图工具是 `scrot`。`xwd`、`import`、`convert`、`ffmpeg` 都没装，别去调。
- 看图分两种人：**用 agent 的**要拷到工作区（agent 的读图工具通常读不了 `/sdcard`，实测报 `image encode failed: File does not exist`，也读不了容器内路径）；**没有 agent 的**按 (c) 扔 `/sdcard/Download`，用相册或文件管理器打开，这条路线不依赖工作区、不依赖 Termux，也不依赖 a7。
- 桌面上如果只显示「没有面板」，通常是 plasmashell 没起来或起崩了，去 `/data/linuxroot/tmp/pshell.log` 看。

**A 阶段 `glxinfo` 显示 llvmpipe、`Accelerated: no` 是正常的。** 这时候还没做 b1，没有 Turnip，Mesa 会自己退回 CPU 软件渲染，不会报错也不会崩。实测过：把 Turnip 的 ICD 藏起来、用纯 A 的环境重启桌面，kwin 和 plasmashell 都正常起来，窗口树里 66 个节点，截图 324KB 有画面。原因是桌面本身不吃 GL——kwin 合成关着（`Enabled=false` + `KWIN_COMPOSE=0`），plasmashell 走 `QT_QUICK_BACKEND=software`。所以**只做 A 就能点亮，GPU 不是前置条件**，它只决定 3D 程序快不快。

### 重启的粒度（别动不动重启手机）

容器没有自己的内核，也没有自己的 init（容器里 `ps -p 1` 看到的就是安卓的 init），所以「重启容器」不是系统级动作，只是杀进程重拉。三档：

| 想干的事 | 怎么做 | 耗时 |
|---|---|---|
| 重启桌面 | 杀掉 `kwin_x11` 和 `plasmashell`，重跑启动脚本 | 十几秒 |
| 重启整个容器 | 杀掉所有 root 指向 `/data/linuxroot` 的进程，重新 mount 加重跑脚本 | 半分钟 |
| 重启手机 | 只有内核态被搞脏时才需要 | 几分钟，且越狱 root 会失效 |

重启容器的正确做法（**不要用 pkill -f**）。注意这个脚本会把容器里所有进程一律杀掉，用户手上跑着的东西会断，执行前先问一句：

```sh
M=/data/linuxroot
for p in /proc/[0-9]*; do
  [ "$(readlink $p/root 2>/dev/null)" = "$M" ] || continue
  kill -9 "${p#/proc/}" 2>/dev/null
done
sleep 2
sh /data/local/tmp/start-x11-desktop.sh
```

- 判据是 `/proc/PID/root` 这个软链，指向 `/data/linuxroot` 的就是容器进程。shizuku 自己、安卓进程、X server（root 是 `/`）都不会被误杀。
- 容器里的 `reboot` 是 systemctl 的软链，而容器里没有 systemd 在跑（`systemctl is-system-running` 返回 `offline`），所以它只会报 `System has not been booted with systemd`，不会重启手机。
- `/proc/sysrq-trigger` 那条路也被堵着，本机 `kernel.sysrq=0`。**如果哪天这个值变成 1，`echo b > /proc/sysrq-trigger` 会立刻重启整机而且不落盘**，绝对不要碰。
- 真需要重启手机的情况：GPU 驱动崩到无法恢复、改过 SELinux 或内核参数要还原、mount 彻底卡死。代价是越狱 root 失效、所有 mount 丢失、还要重新提权，所以这是最后手段。
- 容器和宿主的 `/proc/uptime` 是同一个值（都是 9 天 6 小时），这本身就是「没有第二个内核」的铁证。

### 整机重启之后

这一套全是**内存态**：越狱 root 重启即失效，所有 bind 挂载消失，X server、桌面、PulseAudio、音频桥全部退出，模块目录里的文件还在，但没有任何东西在跑。

恢复就三步：

1. 重新拿 root（越狱模式下这是每次重启都要重做的）
2. 进 KSU 管理器 → chroot 综合管理器 → 点「启动桌面」
3. 等 20 到 40 秒

「启动桌面」是一条完整链，一次就把挂载、X server、KDE、PulseAudio、音频桥按顺序全部拉起，不需要再单独点音频。它是幂等的，重复点不会起两份。例外是把「跟随容器启动」取消勾选的情况，那时点完桌面还要再点一次「启动音频」。

**这条只对做了 b11 的机器成立。** 只做到 A 层时 `$MODDIR/bin/aaudio_bridge_unix` 还不存在（它在 b11.1 才编），start.sh 第 5 步会打一行「音频桥没起来」然后继续走完，桌面照常能用。不想看到这行，就在 a7.5 里把第 5 步整段注释掉，或者只装 A 层的脚本、不装 `audio.sh`。WebUI 上的「启动音频 / 停止音频」两个按钮在没编桥之前点了只会报错，不影响其它按钮。

**不要指望开机自启。** 模块里没有 `service.sh`、`post-fs-data.sh`、`action.sh`，`module.prop` 里也明写了「装上去什么都不跑」。这是有意的：桌面在开机时抢先拉起会和用户抢手机，而且越狱 root 在重启后本来就不存在，写了开机脚本也执行不了。持久 root 的设备不受这条限制（见 a7.2 末尾的补充），但那属于另一套做法，本文没有验证。

重启后验证音频，按 b11.5 那套走一遍就够了：

```sh
pactl list sinks short          # 期望 audio_bridge 在，RUNNING
tail -5 /data/local/tmp/bridge.log
```

---

# B、兼容层与修补

前提：A 部分已经能点亮桌面、`DISPLAY=:0` 是 Xlorie。B 里每一节都是可选的，按需要做，做之前先确认 A 还活着。

## b1：GPU（Vulkan Turnip + OpenGL zink）

**这一节只对高通 Adreno 成立。** 两个前提：`/dev/kgsl-3d0` 存在，且 SELinux 对 appdomain 放行。先跑这一条确认：

```sh
ls -l /dev/kgsl-3d0; getprop ro.board.platform
# 有文件不等于能用：真打开一次。SELinux 拦的话这里直接 Permission denied
: < /dev/kgsl-3d0 && echo "open ok" || echo "open 失败（SELinux 或权限），B 层跳过 GPU"
cat /proc/self/attr/current      # 当前执行域；u:r:ksu:s0 / u:r:magisk:s0 一般放行 appdomain
```

`open ok` 是编译前唯一能提前发现 Selinux 拦截的办法。只有 `ls` 通过就去编 Mesa，会花 9 分钟外加 1.6GB，最后在 b1 的验证三连才失败，很难判断是驱动编坏了还是权限。

看不到 `/dev/kgsl-3d0` 就不是高通（联发科 Mali 走 panfrost/panvk + `/dev/mali0`，完全另一套），本节一条命令都不要照抄，直接跳到 b2。GPU 不是点亮桌面的前置条件，跳过不影响 a 层。

本节所有数字（863 个编译目标、9 分钟、675 FPS、26.24 t/s）都是 Adreno 725 加 8 核的量级参考，换芯片只会差不会一样。**判断成没成看的是那三条验证的结论，不是看数字对不对得上。**

### 为什么要自己编

Ubuntu 24.04 自带的 `libvulkan_freedreno.so` **只编了 msm 这个 KMD 后端**，设备发现全走 `/dev/dri/renderD128`。而安卓内核的 `msm_drm` 不给非 App 进程建 DRI context，所以 chroot 里怎么试都是 `Failed to detect any valid GPUs`。

Mesa 的 freedreno 支持 `msm`、`kgsl`、`virtio`、`wsl` 四个后端。开 `kgsl` 之后驱动直接怼 `/dev/kgsl-3d0`，而这条在 SELinux 策略里是对所有 appdomain 放行的，root 的 chroot 进程当然能开。

`/dev/kgsl-3d0` 能 `open()` 只说明权限够，不代表能用。判定驱动有没有编 kgsl 后端，看这一条：

```sh
strings /data/linuxroot/usr/lib/aarch64-linux-gnu/libvulkan_freedreno.so | grep -c "kgsl-3d0"
```

- 必须命中**完整路径串** `/dev/kgsl-3d0`。只出现 `kgsl` 字样是别的（profiling、debugfs），代表不了后端。Ubuntu 自带那份是 0。

### 编之前装依赖

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin \
  DEBIAN_FRONTEND=noninteractive HOME=/root /bin/bash -c '
  apt-get update
  apt-get install -y --no-install-recommends \
    meson ninja-build build-essential pkg-config bison flex \
    python3-mako python3-yaml python3-pip \
    libdrm-dev libxcb1-dev libx11-xcb-dev libxcb-dri3-dev libxcb-present-dev \
    libxcb-sync-dev libxcb-randr0-dev libxcb-shm0-dev libxshmfence-dev \
    libzstd-dev libexpat1-dev libwayland-dev wayland-protocols libx11-dev \
    glslang-tools glslc spirv-headers \
    libvulkan-dev vulkan-tools mesa-utils mesa-utils-bin libegl1 libgles2 \
    libxext-dev libxfixes-dev libxdamage-dev libxrandr-dev libxrender-dev \
    libxcb-glx0-dev libxxf86vm-dev libxcb-shape0-dev libxcb-xfixes0-dev libxcb-keysyms1-dev'

# Ubuntu 的 meson 1.3.2 编 Mesa 25.2 会少 feature，必须升
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/local/bin HOME=/root /bin/bash -c \
  'pip3 install --break-system-packages -i https://pypi.tuna.tsinghua.edu.cn/simple -U meson'
```

- `spirv-headers` 不是 `spirv-tools`，装错的话 llama.cpp 会报 `SPIRV-HeadersConfig.cmake` 缺失。
- pip 装完在 `/usr/local/bin/meson`，后面的 meson 命令 PATH 里必须有 `/usr/local/bin`，否则还是 1.3.2。`meson --version` 应该是 1.12.x。

### 源码

`mesa-25.2.8.tar.xz`，43,813,260 字节，sha256 `097842f3e49d996868b38688db87b006f7d4541e93ce86d2f341d8b3e7be7c93`，下载地址 `https://archive.mesa3d.org/mesa-25.2.8.tar.xz`。解到 `/data/linuxroot/root/mesa-build/mesa-25.2.8`（约 294MB）。下完先 `sha256sum` 对一遍再解。

- `archive.mesa3d.org` 单线程实测 3MB 卡好几分钟，硬等不划算。有 aria2、IDM、1DM 之类的多线程下载器就先用它下（在宿主或 agent 工作区下都行），下好拷进容器；手边没有这类工具，就在容器里 `curl -C -` 慢慢磨，断了还能续。

### 编 Vulkan 的 Turnip

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/local/bin:/usr/sbin:/sbin HOME=/root /bin/bash -c '
cd /root/mesa-build/mesa-25.2.8
meson setup /root/mesa-build/build /root/mesa-build/mesa-25.2.8 \
  --prefix=/opt/turnip \
  -Dvulkan-drivers=freedreno \
  -Dgallium-drivers= \
  -Dglx=disabled \
  -Dplatforms=x11 \
  -Dfreedreno-kmds=kgsl \
  -Dbuild-tests=false -Dtools= -Dvideo-codecs= \
  -Dzstd=enabled -Dllvm=disabled -Dglvnd=disabled \
  -Dxlib-lease=disabled -Dlibunwind=disabled -Dlmsensors=disabled
cd /root/mesa-build/build && ninja -j8 && ninja install'
```

| 参数 | 为什么必须这么给 |
|---|---|
| `-Dfreedreno-kmds=kgsl` | 整件事的核心，不给就编 msm，白编 |
| `-Dgallium-drivers=` 留空 | 只要 Vulkan 时留空。带 `freedreno` 会额外要 libclc、xcb-glx 一堆东西 |
| `-Dplatforms=x11` | 不给的话 Vulkan WSI 的 xcb surface 扩展不编进去，`VK_KHR_xcb_surface` 就没了 |
| `-Dglx=disabled` | 这轮不要 GL，省一半时间 |
| `-Dllvm=disabled` | 不要 llvmpipe，省 30 分钟 |
| `-Dglvnd=disabled` | 不要 libglvnd 分发层 |
| `-Dlibunwind=disabled` `-Dlmsensors=disabled` | 这两个依赖 rootfs 里没装，不关掉 configure 会挂 |

863 个目标，手机 8 核约 9 分钟。这段时间 8 个核基本跑满，手机烫、界面卡，用户如果在用要提前说一声，或者等他不用的时候再编。产物：

```
/opt/turnip/lib/aarch64-linux-gnu/libvulkan_freedreno.so   63112704 字节
/opt/turnip/share/vulkan/icd.d/freedreno_icd.aarch64.json
```

### 注册 ICD

容器内写 `/usr/share/vulkan/icd.d/freedreno_kgsl.json`：

```json
{
    "ICD": {
        "api_version": "1.4.318",
        "library_arch": "64",
        "library_path": "/opt/turnip/lib/aarch64-linux-gnu/libvulkan_freedreno.so"
    },
    "file_format_version": "1.0.1"
}
```

- `api_version` 跟着编出来的 libvulkan 走。不确定就写 `1.4.318`，或者干脆删掉这一行，loader 会自己读。验证方式是 `vulkaninfo` 报的 apiVersion 跟你编的 Mesa 版本对得上。

- 放进 `/usr/share/vulkan/icd.d/` 之后**不用设任何环境变量**，`vulkaninfo` 自动就能看到。原来的 `freedreno_icd.json`（指向 msm 那份）保持不动，两个并存不冲突。
- 文件名别跟自带的 `freedreno_icd.json` 同名，否则互相覆盖。

### 验证三连（缺一不可）

**一、枚举到设备**

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin /bin/bash -c \
  'vulkaninfo --summary 2>&1 | grep -E "deviceName|driverName|apiVersion"'
# deviceName = Turnip Adreno (TM) 725
# driverName = turnip Mesa driver
```

**二、离屏渲染真的画出像素**（比 vulkaninfo 可信得多，vulkaninfo 成功不代表能画）

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c 'cat > /tmp/vkoff.c <<"EOF"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#define VKCHK(x) do{VkResult r=(x); if(r!=VK_SUCCESS){printf("FAIL %s -> %d\n",#x,r);return 1;}}while(0)
int main(void){
    VkApplicationInfo ai={0}; ai.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName="vkoff"; ai.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici={0}; ici.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ici.pApplicationInfo=&ai;
    VkInstance inst; VKCHK(vkCreateInstance(&ici,NULL,&inst));
    uint32_t n=0; vkEnumeratePhysicalDevices(inst,&n,NULL);
    VkPhysicalDevice pd; vkEnumeratePhysicalDevices(inst,&n,&pd);
    VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(pd,&p);
    uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(pd,&qn,NULL);
    VkQueueFamilyProperties *qf=malloc(sizeof(*qf)*qn);
    vkGetPhysicalDeviceQueueFamilyProperties(pd,&qn,qf);
    int qi=-1; for(uint32_t i=0;i<qn;i++) if(qf[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){qi=i;break;}
    if(qi<0){printf("no gfx queue\n");return 1;}
    float prio=1.0f; VkDeviceQueueCreateInfo qci={0};
    qci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex=qi;qci.queueCount=1;qci.pQueuePriorities=&prio;
    VkDeviceCreateInfo dci={0}; dci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
    VkDevice dev; VKCHK(vkCreateDevice(pd,&dci,NULL,&dev));
    VkQueue q; vkGetDeviceQueue(dev,qi,0,&q);
    const uint32_t W=64,H=64;
    VkImageCreateInfo ic2={0}; ic2.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ic2.imageType=VK_IMAGE_TYPE_2D; ic2.format=VK_FORMAT_R8G8B8A8_UNORM;
    ic2.extent.width=W; ic2.extent.height=H; ic2.extent.depth=1;
    ic2.mipLevels=1; ic2.arrayLayers=1; ic2.samples=VK_SAMPLE_COUNT_1_BIT;
    ic2.tiling=VK_IMAGE_TILING_OPTIMAL;
    ic2.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ic2.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage img; VKCHK(vkCreateImage(dev,&ic2,NULL,&img));
    VkMemoryRequirements mr; vkGetImageMemoryRequirements(dev,img,&mr);
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd,&mp);
    int mt=-1; for(uint32_t i=0;i<mp.memoryTypeCount;i++) if((mr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){mt=i;break;}
    VkMemoryAllocateInfo mai={0}; mai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize=mr.size; mai.memoryTypeIndex=mt;
    VkDeviceMemory mem; VKCHK(vkAllocateMemory(dev,&mai,NULL,&mem));
    VKCHK(vkBindImageMemory(dev,img,mem,0));
    VkImageViewCreateInfo vci={0}; vci.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image=img; vci.viewType=VK_IMAGE_VIEW_TYPE_2D; vci.format=VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount=1; vci.subresourceRange.layerCount=1;
    VkImageView view; VKCHK(vkCreateImageView(dev,&vci,NULL,&view));
    VkAttachmentDescription att={0};
    att.format=VK_FORMAT_R8G8B8A8_UNORM; att.samples=VK_SAMPLE_COUNT_1_BIT;
    att.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; att.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; att.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; att.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference aref={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp={0}; sp.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount=1; sp.pColorAttachments=&aref;
    VkRenderPassCreateInfo rpci={0}; rpci.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount=1; rpci.pAttachments=&att; rpci.subpassCount=1; rpci.pSubpasses=&sp;
    VkRenderPass rp; VKCHK(vkCreateRenderPass(dev,&rpci,NULL,&rp));
    VkImageView views[1]={view};
    VkFramebufferCreateInfo fbci={0}; fbci.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbci.renderPass=rp; fbci.attachmentCount=1; fbci.pAttachments=views;
    fbci.width=W; fbci.height=H; fbci.layers=1;
    VkFramebuffer fb; VKCHK(vkCreateFramebuffer(dev,&fbci,NULL,&fb));
    VkCommandPoolCreateInfo cpci={0}; cpci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex=qi;
    VkCommandPool cp; VKCHK(vkCreateCommandPool(dev,&cpci,NULL,&cp));
    VkCommandBufferAllocateInfo cbai={0}; cbai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool=cp; cbai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount=1;
    VkCommandBuffer cb; VKCHK(vkAllocateCommandBuffers(dev,&cbai,&cb));
    VkCommandBufferBeginInfo cbbi={0}; cbbi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VKCHK(vkBeginCommandBuffer(cb,&cbbi));
    VkClearValue cv; cv.color.float32[0]=1.0f; cv.color.float32[1]=0.5f; cv.color.float32[2]=0.25f; cv.color.float32[3]=1.0f;
    VkRenderPassBeginInfo rpbi={0}; rpbi.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass=rp; rpbi.framebuffer=fb;
    rpbi.renderArea.extent.width=W; rpbi.renderArea.extent.height=H;
    rpbi.clearValueCount=1; rpbi.pClearValues=&cv;
    vkCmdBeginRenderPass(cb,&rpbi,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cb);
    VKCHK(vkEndCommandBuffer(cb));
    VkSubmitInfo si={0}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount=1; si.pCommandBuffers=&cb;
    VKCHK(vkQueueSubmit(q,1,&si,VK_NULL_HANDLE));
    VKCHK(vkQueueWaitIdle(q));
    VkBufferCreateInfo bci={0}; bci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size=(VkDeviceSize)W*H*4; bci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buf; VKCHK(vkCreateBuffer(dev,&bci,NULL,&buf));
    VkMemoryRequirements bmr; vkGetBufferMemoryRequirements(dev,buf,&bmr);
    int bmt=-1; for(uint32_t i=0;i<mp.memoryTypeCount;i++) if((bmr.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){bmt=i;break;}
    VkMemoryAllocateInfo bmai={0}; bmai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bmai.allocationSize=bmr.size; bmai.memoryTypeIndex=bmt;
    VkDeviceMemory bmem; VKCHK(vkAllocateMemory(dev,&bmai,NULL,&bmem));
    VKCHK(vkBindBufferMemory(dev,buf,bmem,0));
    VKCHK(vkResetCommandBuffer(cb,0));
    VKCHK(vkBeginCommandBuffer(cb,&cbbi));
    VkBufferImageCopy bic={0}; bic.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    bic.imageSubresource.layerCount=1;
    bic.imageExtent.width=W; bic.imageExtent.height=H; bic.imageExtent.depth=1;
    vkCmdCopyImageToBuffer(cb,img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buf,1,&bic);
    VKCHK(vkEndCommandBuffer(cb));
    VkSubmitInfo si2={0}; si2.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si2.commandBufferCount=1; si2.pCommandBuffers=&cb;
    VKCHK(vkQueueSubmit(q,1,&si2,VK_NULL_HANDLE));
    VKCHK(vkQueueWaitIdle(q));
    void *ptr; VKCHK(vkMapMemory(dev,bmem,0,VK_WHOLE_SIZE,0,&ptr));
    unsigned char *px=(unsigned char*)ptr;
    printf("GPU=%s  pixel(0,0)=%u,%u,%u,%u\n",p.deviceName,px[0],px[1],px[2],px[3]);
    int ok=(px[0]>240 && px[1]>110 && px[1]<145 && px[2]>50 && px[2]<80);
    printf("RENDER_%s\n", ok?"OK":"MISMATCH");
    return ok?0:1;
}
EOF
gcc -O1 -o /tmp/vkoff /tmp/vkoff.c -lvulkan && /tmp/vkoff'
```

期望输出：

```
GPU=Turnip Adreno (TM) 725  pixel(0,0)=255,128,64,255
RENDER_OK
```

**三、拿真程序压一遍**

这一步要两份外部东西，本文都不提供：llama.cpp 源码（`git clone https://github.com/ggml-org/llama.cpp /root/llama/llama.cpp-master`，或者下 master 的 zip 解开）和一个 gguf 模型（随便哪个量化版都行，示例用的是 Qwen2.5-3B Q4_K_M，路径 `/mnt/termux/...` 换成你自己的；没装 Termux 就放容器里任意目录）。两样缺一样就整段跳过——前面 vkoff 探针出了 `RENDER_OK` 已经证明硬件在算像素。

这一步是整篇唯一会把 GPU 拉到 99% 的地方，也是唯一会让用户以为手机崩了的地方。

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/local/bin HOME=/root /bin/bash -c '
cd /root/llama/llama.cpp-master
cmake -B build-vk -DGGML_VULKAN=ON -DCMAKE_BUILD_TYPE=Release -DLLAMA_CURL=OFF -DGGML_NATIVE=ON
cmake --build build-vk -j8 --target llama-bench llama-cli
./build-vk/bin/llama-bench -m /mnt/termux/qwen2.5-3b-instruct-q4_k_m.gguf -ngl 99 -p 128 -n 32 -r 3'
```

`-ngl 99` 是全量 offload，跑起来 Adreno 会拉到 99% 占用、频率锁最高（实测 580MHz），CPU 也在同时跑。**这段时间 SurfaceFlinger 抢不到 GPU，画面会冻住一到三秒**，看起来跟内核 panic 一模一样，其实是排队，会自己恢复。

- **动手前一定要跟用户说一句**。不打招呼就起，他会以为手机挂了，而且这个反应完全合理。
- 第一次跑先用小参数试水（`-p 32 -n 8 -r 1`），确认没异常再上完整参数。
- 想边跑边看，另开一个窗口 `watch -n1 cat /sys/class/kgsl/kgsl-3d0/gpubusy`，能看到占用从 10% 出头爬到 90% 以上。
- 用户手机上要是没有模型文件，这一步可以整个跳过。前面 vkoff 探针出 `RENDER_OK` 就证明硬件真的在算像素了，不需要再压一次。

实测（Qwen2.5-3B-Instruct Q4_K_M，全量 offload）：`pp128` 26.24 t/s，`tg32` 5.30 t/s。`pp` 是 prefill，`tg` 才是生成速度，别混。

- 编译期刷 `Unsupported SPIR-V capability: SpvCapabilityStorageBuffer8BitAccess` 是无害的，Turnip 会软件模拟。
- `matrix cores: none`。a6xx 的 Turnip 不支持 cooperative matrix，llama.cpp 最快的矩阵乘内核用不上，这是速度天花板。想更快只能等 Mesa，别在这上面耗。

### OpenGL：走 zink，但必须用系统的 zink

**别用自编的 zink**。第二次编译带上 `-Dgallium-drivers=freedreno,zink -Dglx=dri -Degl enabled` 那一版，`libgallium-25.2.8.so` 编出来了，但用起来两种死法：

- `MESA_LOADER_DRIVER_OVERRIDE=zink` → `MESA: error: ZINK: failed to choose pdev`
- `GALLIUM_DRIVER=zink` → `xmlconfig.c:1321: driQueryOptionb: Assertion 'cache->info[i].name != NULL' failed`

根因在 `zink_screen.c` 的 `choose_pdev`：有 DRM 设备号时它按设备号匹配，而 kgsl 后端没有 DRM 设备号，匹配结果 `idx == -1`，`screen->pdev` 保持 NULL。

**正解：用 Ubuntu 自带的 `/usr/lib/aarch64-linux-gnu/dri/zink_dri.so`，只让 Vulkan 侧吃自编的 Turnip**（通过上面那个 ICD json）。不要设 `LD_LIBRARY_PATH`。

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root DISPLAY=:0 /bin/bash -c \
  'GALLIUM_DRIVER=zink glxinfo -B 2>&1 | grep -E "Accelerated|Device:"'
# Device: zink Vulkan 1.4(Turnip Adreno (TM) 725 (MESA_TURNIP)) (0x7030002)
# Accelerated: yes
```

gallium 是 25.2.8-0ubuntu0.24.04.5、Turnip 是自编 25.2.8，版本混用完全没问题，Vulkan 接口是稳定的。

性能对照（1920x1080、16 位色）：zink + Turnip 约 675 FPS，llvmpipe 约 312 FPS。

**单程序加速包装** `/usr/local/bin/gpuon`：

```sh
#!/bin/sh
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/freedreno_kgsl.json
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/freedreno_kgsl.json
export GALLIUM_DRIVER=zink
export ZINK_DESCRIPTORS=lazy
export MESA_SHADER_CACHE_MAX_SIZE=512MB
export mesa_glthread=true
export TU_DEBUG=noconform
export VK_LOADER_DRIVERS_SELECT=freedreno*
exec "$@"
```

用法：`DISPLAY=:0 gpuon glxgears`。

- **别把 `GALLIUM_DRIVER=zink` 写进全局启动脚本**。笔者实测全局生效后 X server 会卡死：kwin 和 plasmashell 的 PID 都在，但 `xdpyinfo` 十秒无响应，`dbus-launch --sh-syntax` 返回空。判断是 KWin 每帧 `glXSwapBuffers` 把 X server 锁住了。想试只能单独试 kwin，别动 plasmashell。
- Xlorie 自带真 DRI3，所以**不设任何环境变量时 `glxinfo` 默认就是 `Accelerated: yes` + zink**。`gpuon` 主要是给需要强制指定 ICD 或调 shader 缓存的程序用的。
- 装完之后记得把 `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/freedreno_kgsl.json` 加回 a6 的启动脚本（`start-x11-desktop.sh` 里 chroot 起桌面的那一段）。不加也不影响点亮，只是桌面里起的 Vulkan 程序看不到这块 GPU。
- 这套只对 Linux 程序成立，**wine 里的 OpenGL 程序用不了**：box64 模拟的 `opengl32` 调 zink 必段错误（退出码 139），修法是回 llvmpipe，见 b3 末尾那段。
- 三件笔者踩过或者明确不建议的事：让 Xorg 去抢 `/dev/dri/card0`（Android 自己的显示设备，笔者试黑过一次，只能重启手机）；给 `xorgxrdp` 配 `DRMDevice` + `DRI3`（每次都崩在 `rdpRRSetRdpOutputs` 段错误）；指望 VirGL 那套（宿主端 `virgl_test_server` 能起、socket 也能建，但 guest 侧 `lost connection to rendering server`，因为 EGL 在非 App 进程里拿不到 gralloc/Surface，必须有真 App 进程在用 Android 图形栈）。

### 边界：DRI3 只有 GLX 侧能用，EGL 拿不到

同一套环境里跑两个探针，结论不一样：

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root DISPLAY=:0 \
  XDG_RUNTIME_DIR=/tmp/runtime-root /bin/bash -c 'glxgears -info | grep GL_RENDERER'
# GL_RENDERER   = zink Vulkan 1.4(Turnip Adreno (TM) 725 (MESA_TURNIP))

chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root DISPLAY=:0 \
  XDG_RUNTIME_DIR=/tmp/runtime-root /bin/bash -c 'eglinfo 2>&1 | grep -i renderer'
# OpenGL core profile renderer: llvmpipe (LLVM 20.1.2, 128 bits)
# libEGL warning: DRI3 error: Could not get DRI3 device
```

走 GLX 的程序（glxgears、KDE 自己）能吃到 Turnip；走 EGL 的程序自动退到 llvmpipe。Xlorie 的 DRI3 只覆盖 GLX 那条通道。

- 用 EGL 的大户就是火狐（`MOZ_X11_EGL` 默认开着）和一票 Electron 应用。它们在这套环境里没有 GPU 加速这条路，只能软件渲染。
- 别拿 `GALLIUM_DRIVER=zink` 去给 EGL 程序加速，它绕不过 DRI3 那一关。
- 软件渲染不是不能用，但要接受两个后果：3D 和动画慢，以及 llvmpipe 的 LLVM JIT 在某些 ARM 芯片上会吐非法指令（b14 火狐那条就是）。

## b2：box64

apt 里那个 0.2.6 太老，跑 Wine 会崩。自己编 0.4.4。

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root \
  DEBIAN_FRONTEND=noninteractive /bin/bash -c '
  apt-get install -y --no-install-recommends cmake build-essential pkg-config git xz-utils
  cd /usr/src
  curl -sL -o b64.tgz "https://gh-proxy.com/https://github.com/ptitSeb/box64/archive/refs/tags/v0.4.4.tar.gz"
  tar xzf b64.tgz && cd box64-0.4.4
  mkdir build && cd build
  cmake -DARM_DYNAREC=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr ..
  make -j8 && make install'
```

- `make -j8` 约 6 分钟，**必须放后台跑**（`setsid nohup ... &` 加日志），别占着 agent 工具那几十秒的超时。
- 用 `gh-proxy.com` 前缀拉 GitHub，直连只有 16 KB/s。`ghfast.top` 返回 403，`ghproxy.net` 约 20 KB/s。

验证（`/usr/bin/box64` 约 75MB）：

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c '
  box64 --version
  curl -sL -o /tmp/busybox.deb "https://mirrors.tuna.tsinghua.edu.cn/ubuntu/pool/main/b/busybox/busybox-static_1.37.0-7ubuntu1.1_amd64.deb"
  cd /tmp && dpkg-deb -x busybox.deb bb
  box64 /tmp/bb/usr/bin/busybox echo box64_works'
# 期望 Box64 arm64 v0.4.4 with Dynarec
# 期望 box64_works
```

- `dpkg-deb -x` 解出来是 `bb/usr/bin/busybox`，不是 `bb/bin/busybox`。
- 验证这一步别省。box64 编出来了但跑不动的情况，直接上 Wine 会看不出是哪一层的问题。

## b3：Wine

**先判定 exe 是什么，再决定要不要救**

```sh
python3 - <<'PY'
import struct
d = open("/path/to.exe", "rb").read(0x400)
e = struct.unpack("<I", d[0x3c:0x40])[0]
print("machine", hex(struct.unpack("<H", d[e+4:e+6])[0]))    # 0x014c=x86, 0x8664=x64
print("optmagic", hex(struct.unpack("<H", d[e+24:e+26])[0])) # 0x10b=PE32, 0x20b=PE32+
PY

strings -a "$F" | grep -i -E \
  "mscoree|mscorlib|PresentationFramework|_MEI|pyinstaller|KERNEL32|USER32|WS2_32|COMCTL32|vcruntime" | sort -u
```

- 导入表里有 `PresentationFramework` / `PresentationCore` → .NET WPF，大概率跑不动（见本节末尾的记录）。
- 有 `_MEI` → PyInstaller 打包，单文件 bundle 也容易卡在解包，见本节末尾。
- 只有 KERNEL32 / USER32 / WS2_32 / COMCTL32 → 普通 Win32，能救。

**先说结论：笔者试下来只有 box64 加 Kron4ek 的 Wine 10.0 amd64-wow64 能跑通。** 下面这些组合都是笔者亲手撞倒的，现象记在这里，你要试别的组合完全可以，只是先知道这堵墙大概有多厚：Ubuntu 24.04 的 `wine64:amd64` 9.0 在 box64 下 `free(): invalid pointer` 崩在 ntdll.so（Debian 打包去掉了 clang 检查，Launchpad #2102681，调 `BOX64_DYNAREC_*` 救不了）；`BOX64_EMULATED_LIBS=libc.so.6` 会引出 GLIBC_PRIVATE 符号缺失，更糟；Kron4ek Wine 11.22 报 `set_mem_area_state: Assertion !(size & granularity_mask) failed`；`qemu-x86_64-static` 跑 wine 报 `could not exec the wine loader`。

```sh
# 下载（GitHub 直连 16 KB/s，必须走代理）。三条路线选一条，别混位置（见 a0）：
#   ① 容器里 curl（推荐，最省事，位置 ②）
#   ② 宿主 root shell 用 Termux 的 curl / toybox wget（位置 ①）
#   ③ 任意多线程下载器（aria2 / 1DM / IDM；在宿主或 agent 工作区跑都行），下完必须再拷到 /sdcard/Download
# 下面按 ① 写：
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c '
  curl -L -C - --retry 20 --max-time 1800 \
    -o /tmp/wine100.tar.xz "https://gh-proxy.com/https://github.com/Kron4ek/Wine-Builds/releases/download/10.0/wine-10.0-amd64-wow64.tar.xz"
  ls -lh /tmp/wine100.tar.xz'

# 解压必须在容器里做：安卓本身没有 xz（Termux 装了才有）
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c \
  'mkdir -p /opt/wine10 && tar -xJf /tmp/wine100.tar.xz -C /opt/wine10 --strip-components=1'

chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c \
  'box64 /opt/wine10/bin/wine --version'
# 期望 wine-10.0
```

- 宿主动手 `tar -xJf` 会报 `exec xz: No such file or directory`，安卓没有 xz。
- `--strip-components=1` 之后是 `bin/ lib/ include/ share/`，可执行文件是 `bin/wine`，**没有 `wine64` 这个名字**，别去找。
- 代理会静默截断大文件，下完先看一眼大小再解压。

初始化前缀：

```sh
chroot /data/linuxroot /usr/bin/env -i \
  PATH=/opt/wine10/bin:/usr/bin:/bin TMPDIR=/tmp HOME=/root DISPLAY=:0 \
  WINEPREFIX=/root/.wine10 WINEDEBUG=-all /bin/bash -c \
  'box64 /opt/wine10/bin/wine wineboot --init'
```

成功标志：`/root/.wine10` 下出现 `dosdevices drive_c system.reg user.reg userdef.reg`，`ps -ef` 能看到 `wineserver`、`services.exe`、`explorer.exe`。

- `wineboot --init` **会挂住不返回**（前端 UI 一直开着），别傻等。看到前缀文件生成了就继续。
- 这个包是 wow64 构建，理论上 32 位和 64 位 Windows 程序都能跑，但 32 位没实测。

**包装脚本 `/usr/local/bin/win`（正式版）**

中文文件名会让 wine 报 `failed to open "\xe7\xab\xaf..."` 加 `c0000135`。修法是「生成 ASCII 副本」而不是「改 locale 就完事」，两者都要做（locale 与字体见 b4）：

```bash
#!/bin/bash
# /usr/local/bin/win  --  Wine 包装：处理中文名 + 固定 locale + 默认软件渲染
export WINEPREFIX=${WINEPREFIX:-/root/.wine10}
export DISPLAY=${DISPLAY:-:0}
export LANG=${LANG:-zh_CN.UTF-8}
export WINEDEBUG=${WINEDEBUG:--all}
# 走 zink 会段错误，默认退回 llvmpipe；要试硬加速就传 LIBGL_ALWAYS_SOFTWARE=0
export LIBGL_ALWAYS_SOFTWARE=${LIBGL_ALWAYS_SOFTWARE:-1}
mkdir -p /tmp

if [ -f "$1" ]; then
  src="$1"; f="$(basename "$src")"; dir="$(dirname "$src")"
  if printf '%s' "$f" | LC_ALL=C grep -q '[^ -~]'; then
    [ "$f" = "run_ascii.exe" ] || cp -f "$src" "$dir/run_ascii.exe"
    cd "$dir" 2>/dev/null
    echo "[win] 中文名已转存为 $dir/run_ascii.exe"
    exec box64 /opt/wine10/bin/wine run_ascii.exe "${@:2}"
  fi
  cd "$dir" 2>/dev/null
  exec box64 /opt/wine10/bin/wine "$f" "${@:2}"
fi
exec box64 /opt/wine10/bin/wine "$@"
```

- 副本必须生成在**原目录**，不能复制到 `/tmp`：很多老程序按 exe 所在目录找同目录的 dll 和资源。
- `cd` 到 exe 所在目录也很重要，原因同上。
- `c0000135` 是 STATUS_DLL_NOT_FOUND 的泛化错误，**不代表真缺 DLL**，别去装 vcrun。

用法：

```sh
win notepad
win /sdcard/Download/xxx.exe
win /root/.wine10/drive_c/Program\ Files/xxx/xxx.exe
```

程序常驻在 `/sdcard`（fuse，慢且权限怪）时，做个专用启动器先把它复制进容器。下面这段是示例，`your-app.exe` 和 `/root/yourapp` 换成你自己的程序名和目录：

```bash
#!/bin/bash
# /usr/local/bin/your-app   示例启动器，文件名自己定
SRC="/sdcard/Download/your-app.exe"
mkdir -p /root/yourapp
[ -f "$SRC" ] && cp -f "$SRC" /root/yourapp/your-app.exe
[ -f /root/yourapp/your-app.exe ] || { echo "找不到 your-app.exe，先把它放到 $SRC"; exit 1; }
exec /usr/local/bin/win /root/yourapp/your-app.exe "$@"
```

**Wine 里的 OpenGL 程序会闪退（退出码 139）**

现象：窗口一闪就没了，`echo $?` 是 `139`（SIGSEGV）。`WINEDEBUG=+loaddll` 能看到 `opengl32.dll`、`winex11.drv`、`libGL.so.1` 都加载完，然后死在第一次 GL 调用上。

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root DISPLAY=:0 \
  XDG_RUNTIME_DIR=/tmp/runtime-root /bin/bash -c 'glxinfo -B | head -8'
# Device: zink Vulkan 1.4(Turnip Adreno (TM) 725 (MESA_TURNIP)) (0x7030002)
```

显示 `zink` 就是它：box64 模拟出来的 wine `opengl32.so` 去调 zink 会稳定段错误。`BOX64_DYNAREC_STRONGMEM=3`、`BIGBLOCK=0`、`SAFEFLAGS=2` 三种都试过，救不回来。

修法是这个进程回退 llvmpipe，`win` 脚本里已经默认带上：

```sh
LIBGL_ALWAYS_SOFTWARE=1 /usr/local/bin/win app.exe
```

- 代价是 3D 程序走纯 CPU 软渲染，帧数低。安装器、扫描器这类小工具完全够用。
- 想给某个程序试硬加速：`LIBGL_ALWAYS_SOFTWARE=0 gpuon win app.exe`，崩了还是回软渲染，别在这上面耗时间。`gpuon` 里设了 `GALLIUM_DRIVER=zink`，套在 wine 上大概率同样崩。
- 只影响 OpenGL，不影响 Vulkan；X server 本身还在走 zink，桌面流畅度不变。
- 顺带一个假警报：`.NET Framework Initialization Error ... v1.0.3705` 那类弹窗是 .NET 1.x 只有 32 位、在 x64 prefix 里装不上导致的，跟当前程序无关，关掉即可。

**这两类笔者没跑通**

- **.NET 单文件 bundle**：`.NET 6 single-file` 在 wine 下连 CLR 都起不来，`Fatal error. Internal CLR error. (0x80131506) at System.AppContext.Setup`。十种开关（`DOTNET_gcServer`、`GCHeapHardLimit`、`EnableWriteXorExecute`、`TieredCompilation`、`EnableDiagnostics`、`BOX64_DYNAREC_STRONGMEM`、`X87DOUBLE`、`BIGBLOCK=0`、`SAFEFLAGS=2`、bundle 解压目录）全部无效。根因是三层模拟叠在一起：x86 PE 托管代码 → wine 的 PE loader → wine unix 部分（x86-64）→ box64 转译 → ARM64，CoreCLR 要的页对齐和可执行内存 box64 给不了。要跑只能换给安卓打过补丁的 GameNative，或者把功能重写成原生。
- **.NET 8 WPF self-contained**：能绕过 `0x8007000E`（`DOTNET_gcServer=0 DOTNET_GCConserveMemory=9 DOTNET_GCHeapHardLimit=0x20000000`），进程跑到 92% CPU，`xwininfo` 能看到 `WineD3D fake window` 和 `MediaContextNotificationWindow`，但**没有主窗口**。milcore 硬依赖 D3D 加 DirectWrite，wine 支持极薄。识别信号就是导入表里有 `PresentationFramework`。至于能不能救，笔者是没救活。
- 顺带：bundle 的解压目录要写 **Windows 风格路径**（`C:\temp\dnbundle`），写 Linux 路径会报 `Failed to create directory [Z:\...]`。这条对能跑起来的 bundle 同样适用。

这一节的结论是「十种开关都没救活」，不是「物理上不可能」。要是哪天有人翻到别的组合，这条就该被推翻。

### 补 winemenubuilder（Kron4ek 的包没有）

这个 build 的构建脚本里写死了 `--disable-winemenubuilder`，所以装完 Windows 软件，Linux 侧的文件关联一条都不会出现。补回来要三个文件：

```sh
# 1) 从 winehq 官方 10.0 amd64 包装配一个 winemenubuilder.exe
cd /tmp
for i in 1 2 3; do
  curl -L -s -C - -o wstab.deb \
    "https://dl.winehq.org/wine-builds/ubuntu/pool/main/w/wine/wine-stable-amd64_10.0.0.0~noble-1_amd64.deb"
  dpkg-deb -c wstab.deb >/dev/null 2>&1 && break
done
mkdir -p wh && dpkg-deb -x wstab.deb wh
cp wh/opt/wine-stable/lib64/wine/x86_64-windows/winemenubuilder.exe \
   /opt/wine10/lib/wine/x86_64-windows/

# 2) advpack.dll 必须一起换，Kron4ek 那个版本缺 RegInstallA
cp /opt/wine10/lib/wine/x86_64-windows/advpack.dll{,.orig}
cp wh/opt/wine-stable/lib64/wine/x86_64-windows/advpack.dll \
   /opt/wine10/lib/wine/x86_64-windows/
```

- 112 MB 的包，必须带 `-C -` 断点续传，一次性经常被截断成 12 MB 然后 `dpkg-deb` 报 Unexpected EOF。
- 不换 advpack 的话，一跑就崩：`unimplemented function advpack.dll.RegInstallA`。
- 内置 DLL 的唯一真相是 `lib/wine/x86_64-windows/`，prefix 里 `system32` 的副本不用管。
- 只补了 x86_64，32 位的 `i386-windows/winemenubuilder.exe` 在 `wine-stable-i386` 包里，没补。

补完手动跑一次 `wine winemenubuilder.exe -a`，会刷出 120 多个 `wine-extension-*.desktop` 和 `~/.local/share/mime/packages/x-wine-extension-*.xml`。

但它生成的条目有两个毛病，所以还得配 b15 的同步器：`NoDisplay=true`（KDE 的「打开方式」里看不见），`Exec=env WINEPREFIX=... wine start /ProgIDOpen ...`（依赖 PATH 里有 `wine`，得自己补最小包装）。

```sh
# 3) 它的 Exec 用 `wine start ...`，这个 build 的 PATH 里没有 wine
cat > /usr/local/bin/wine <<'EOF'
#!/bin/bash
export WINEPREFIX="${WINEPREFIX:-/root/.wine10}"
export DISPLAY="${DISPLAY:-:0}"
export WINEDEBUG="${WINEDEBUG:--all}"
exec box64 /opt/wine10/bin/wine "$@"
EOF
chmod +x /usr/local/bin/wine
```

补完这套，`wine start /ProgIDOpen VLC.mp4 /tmp/test.mp4` 能直接把 Windows 版 VLC 拉起来（实测弹窗正常）。

## b4：中文（locale、字体、界面语言）

中文出问题分三类，先分清再动手，判错方向能白折腾一晚上：

| 现象 | 根因 | 看哪小节 |
|---|---|---|
| 方块、问号、空白 | 字体里没有汉字字形 | 二 |
| `å£æ`、`¿ÚÊÖ` 这种拉丁垃圾 | locale 不对，Wine 的 ANSI 代码页跟着 LANG 走 | 四 |
| 菜单、按钮、窗口标题全英文 | KDE 的 .mo 翻译文件被 dpkg 排除了 | 三 |

**一、生成真 locale**

base rootfs 里没有 i18n 数据，直接 `localedef` 会报 `character map file 'UTF-8' not found` 加 `cannot read character map directory /usr/share/i18n/charmaps`。

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root \
  DEBIAN_FRONTEND=noninteractive /bin/bash -c '
  apt-get install -y --no-install-recommends locales
  localedef -i zh_CN -f UTF-8 zh_CN.UTF-8
  LC_ALL=zh_CN.UTF-8 locale charmap'
# 期望 UTF-8
```

**二、Wine 的字体：拷进去 + 改替换表，指向 TTF 不要 TTC**

（位置②，容器里；两个路径都是容器内的）

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin /bin/cp \
  /usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf \
  /root/.wine10/drive_c/windows/Fonts/
```

光拷字体不够。程序按字体名找 Arial / MS Shell Dlg / SimSun，找不到就退到无中文字体，界面还是一堆问号。必须写替换表：

下面这段存成容器里的 `/tmp/font.reg`（宿主视角同一个文件是 `/data/linuxroot/tmp/font.reg`，在 ① 写也行，下一步的 `regedit` 读的是容器内的 `/tmp/font.reg`）：

```reg
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes]
"MS Shell Dlg"="Droid Sans Fallback"
"MS Shell Dlg 2"="Droid Sans Fallback"
"MS Sans Serif"="Droid Sans Fallback"
"Tahoma"="Droid Sans Fallback"
"SimSun"="Droid Sans Fallback"
"Arial"="Droid Sans Fallback"
"Microsoft Sans Serif"="Droid Sans Fallback"
"Microsoft YaHei"="Droid Sans Fallback"
"System"="Droid Sans Fallback"
```

```sh
chroot /data/linuxroot /usr/bin/env -i \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin HOME=/root \
  WINEPREFIX=/root/.wine10 DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
  /bin/bash -c 'box64 /opt/wine10/bin/wine regedit /tmp/font.reg'
```

- **别指 `WenQuanYi Micro Hei`**。`wqy-microhei.ttc` 是字体集合，wine 对 TTC 支持不稳，典型症状是主界面中文正常、某个弹窗整排方块。`DroidSansFallbackFull.ttf` 是单体 TTF，稳。
- 字体名别猜：`fc-query --format "%{family}\n" <ttf 路径>` 直接读，`Droid Sans Fallback` 就是这么来的。
- `DroidSansFallbackFull.ttf` 来自 `fonts-droid-fallback` 包，a5 的安装列表里已经带上。桌面自己继续用 WenQuanYi / Noto CJK 渲染没问题，只有 wine 需要单体 TTF。
- 执行 `regedit` 那步**必须带 `WINEPREFIX=/root/.wine10`**。漏了 wine 会按默认值在 `/root/.wine` 现建一套空 prefix，然后刷 `nodrv_CreateWindow` 加一句 `no driver could be loaded`，看着像显示驱动坏了，其实是跑错前缀。撞到就 `rm -rf /root/.wine` 收场。
- 改完 `wineserver -k` 重启一次最稳。

**三、KDE 界面语言：翻译文件被 dpkg 排除了**

```sh
ls /usr/share/locale/zh_CN/LC_MESSAGES | wc -l
ls /usr/share/locale/zh_CN/LC_MESSAGES | grep -c '\.mo$'
```

- 两个数都是 24 上下、而且列出来全是 `*_qt.qm` → 翻译文件被排除了，按下面修。
- 数量 200 以上、能看到 `dolphin.mo` → 文件在，是 LANG 没传对，跳到四。

**根因**是 `/etc/dpkg/dpkg.cfg.d/excludes` 里的一条：

```
path-exclude=/usr/share/locale/*/LC_MESSAGES/*.mo
```

做精简镜像时加的（顺手连 man 和 doc 一起排了），效果是所有包解包时跳过 `.mo`，装多少个语言包都没用。KDE 的翻译本来就在各应用自己的包里（`dolphin`、`plasma-workspace-data`、`kwin-data`、`kio`），`language-pack-kde-zh-hans` 只有 changelog 是 Ubuntu 的正常设计，不是空壳。

**修法**两步，顺序不能反：

```sh
# 1) 去掉排除规则。不先做这步，重装一百遍也拿不回文件
cd /data/linuxroot/etc/dpkg/dpkg.cfg.d
cp excludes excludes.bak
sed -i 's|^path-exclude=/usr/share/locale/\*/LC_MESSAGES/\*\.mo|# &|' excludes

# 2) 把所有带 .mo 的包重装一遍
chroot /data/linuxroot /usr/bin/env -i \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin HOME=/root \
  DEBIAN_FRONTEND=noninteractive /bin/bash -c '
    dpkg -S "/usr/share/locale/zh_CN/LC_MESSAGES/*.mo" 2>/dev/null | cut -d: -f1 | sort -u > /tmp/pkgs.txt
    wc -l < /tmp/pkgs.txt          # 本机 61 个
    cd / && apt-get install --reinstall -y --no-install-recommends \
      -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" $(cat /tmp/pkgs.txt)'
```

验证：

```sh
ls /usr/share/locale/zh_CN/LC_MESSAGES | wc -l     # 24 → 294
ls -l /usr/share/locale/zh_CN/LC_MESSAGES/dolphin.mo
```

- 判断成没成别拿 `plasma_workspace.mo` 当标准，那个名字不存在，plasma 的翻译拆成 `kcm_*.mo`、`plasma*.mo` 一堆。
- 重装那 61 个包约 5 分钟，大头是 apt、iso-codes、kwin-data。
- 重装完输出全是 `Setting up ...`，看着成功不代表文件回来了，必须 `ls` 验证。
- 24.04 是 KF5，重建缓存用 `kbuildsycoca5`，没有 `kbuildsycoca6`。

**四、LANG 要写三处，最关键的那处在启动脚本里**

```sh
# 1) 系统级
printf 'LANG=zh_CN.UTF-8\nLANGUAGE=zh_CN:en\n' > /data/linuxroot/etc/default/locale

# 2) KDE 自己的语言设置
cat > /data/linuxroot/root/.config/plasma-localerc <<'EOF'
[Formats]
LANG=zh_CN.UTF-8

[Translations]
LANGUAGE=zh_CN
EOF

echo zh_CN > /data/linuxroot/root/.config/user-dirs.locale
```

3) a6 那个启动脚本里，起 kwin / plasmashell 的 `env -i` 那行要带 `LANG=zh_CN.UTF-8 LANGUAGE=zh_CN:en`（a6 贴的脚本已经写进去了）。

- **第三处不写，前两处全部白做。** 桌面进程树是从 `env -i` 长出来的，`/etc/default/locale` 对这种起来方式完全无效。
- 生效信号：任务栏从 `2026/11/18 4:27 PM` 变成 `2026/11/18 4:27 下午`，Dolphin 左侧从 `Places / Home` 变成 `常用位置 / 主文件夹`。
- 重启要 kwin 和 plasmashell 一起杀。只重启 plasmashell 会出现菜单中文、标题英文的半吊子状态。
- Wine 里的中文乱码是同一个根因：LANG 空着的时候 Wine 按 CP1252 处理，程序吐的 GBK 字节全变拉丁垃圾。所以 `win` 脚本里的 `LANG=${LANG:-zh_CN.UTF-8}` 跟这里是同一套东西。
- 用户目录名（Downloads / Pictures / Music）不跟着界面语言变，那是 `~/.config/user-dirs.dirs` 的事。要改得跑 `xdg-user-dirs-update --force` 并处理旧目录，不改成中文反而省事，因为不少脚本写死了 `~/Downloads`。


## b5：exe 双击运行（文件关联）

```sh
mkdir -p /root/.local/share/applications
cat > /root/.local/share/applications/wine-exe.desktop << 'EOF'
[Desktop Entry]
Name=Wine Windows Program
Comment=Run Windows executables with Wine + box64
Exec=/usr/local/bin/win %f
Icon=wine
Terminal=false
Type=Application
Categories=System;
MimeType=application/x-ms-dos-executable;application/x-msdownload;application/x-dosexec;application/vnd.microsoft.portable-executable;
EOF

xdg-mime default wine-exe.desktop application/vnd.microsoft.portable-executable
xdg-mime default wine-exe.desktop application/x-ms-dos-executable
xdg-mime query default application/vnd.microsoft.portable-executable   # 期望 wine-exe.desktop
```

- **exe 不能带执行位**。带 `+x` 时 KDE 把它当 Linux 可执行文件，报 `For security reasons, launching executables is not allowed in this context.`。一律 `chmod 644 xxx.exe`。
- 两个 MIME 类型都要设。Ubuntu 24.04 判出来是 `application/vnd.microsoft.portable-executable`，把 `x-ms-dos-executable` 也写上保险（b6 会把判型拨到后者，那时正好用上）。
- **`kde-open5 /path/x.exe` 在命令行测试会报上面那句，但这不代表桌面双击失败**。验证必须模拟真实双击：

```sh
DISPLAY=:0 HOME=/root XDG_RUNTIME_DIR=/tmp/runtime-root xdotool mousemove 62 50
DISPLAY=:0 xdotool click --repeat 2 --delay 120 1
ps -ef | grep -c '[n]otepad.exe'
```

- 上面这条要切到桌面、会抢用户的前台。不打扰人的验法是用 KService 的打开链路，双击走的就是同一条，只是不经过 plasma 的图标那一跳：

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root \
  DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
  sh -c 'kioclient5 exec /root/Desktop/notepad.exe &'
sleep 8; ps -ef | grep -c '[n]otepad.exe'
```

### 双击没反应时的排查顺序

实测过一次：双击 exe 完全没动静，没有报错、没有窗口、没有日志。原因是 `wine-exe.desktop` 这个文件本体不见了，而 `mimeapps.list` 里的记录照样留着，看起来「配置正常」。

按这个顺序查，一步就能定位：

1. **先看文件本体在不在**：`ls -l /root/.local/share/applications/wine-exe.desktop`。文件缺失是零报错的，KDE 找不到处理程序就直接什么都不做。
2. **再看默认程序**：`xdg-mime query default application/vnd.microsoft.portable-executable`。这里有个容易误判的地方：desktop 文件缺失时这条命令返回**空**，但 `cat /root/.config/mimeapps.list` 里明明写着 `wine-exe.desktop`。query 空 + 配置有值，基本就是文件丢了，不用再往下猜。
3. **重建**：照 b5 开头的 heredoc 重新写一遍，`chmod 644`，两个类型各跑一次 `xdg-mime default wine-exe.desktop`，然后 `update-desktop-database /root/.local/share/applications` 加 `kbuildsycoca5 --noincremental`。不刷 sycoca 的话当前会话里 KDE 可能还拿着旧缓存。
4. **确认 exe 没带执行位**：`stat -c %a xxx.exe`，带 `+x` 会被 KDE 拒掉，表现同样是双击没动静。
5. **验证**：用上面的 `kioclient5 exec`，进程起来了就算通。此时不用管窗口有没有画出来，X11 在后台也能正常创建窗口。
6. **顺手看一眼 `wine-mime-watch` 在不在**：`ps -ef | grep '[w]ine-mime-watch'`。它的 autostart 条目在 `~/.config/autostart/` 里，但桌面重启后不一定真起来；没起来就手动 `nohup /usr/local/bin/wine-mime-watch > /tmp/wine-mime-watch.log 2>&1 &`，否则之后 wine 注册表里新增的关联不会同步。

## b6：exe 图标与缩略图

目标：文件管理器和桌面上显示 exe 自己内嵌的图标，而不是统一的灰色齿轮。

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root \
  DEBIAN_FRONTEND=noninteractive /bin/bash -c '
  apt-get update -qq
  apt-get install -y --no-install-recommends icoutils kio-extras'
```

- `icoutils` 提供 `wrestool`（读 PE 资源段）和 `icotool`（ico 转 png），才 55 KB。
- `kio-extras` 才是关键，KDE 的 exe 缩略图插件 `windowsexethumbnail.so` 在这个包里。装完 `ls /usr/lib/aarch64-linux-gnu/qt5/plugins/kf5/thumbcreator/` 应该有 17 个左右，装的之前只有 `fontthumbnail.so`。
- `kio-extras` 会顺带拉 samba-libs 等几十 MB，正常。

**真正的坑：mime 判出来的类型，插件不认**

插件只认三个类型：`application/x-ms-dos-executable`、`application/x-msdownload`、`application/x-wine-extension-cpl`。而 Ubuntu 24.04 的 shared-mime-info 里 PE 文件的主类型是 `application/vnd.microsoft.portable-executable`，magic priority 80，把 priority 60 的 `x-ms-dos-executable` 打赢了。

```sh
xdg-mime query filetype /path/to/your-app.exe      # 换成任意一个 Windows exe
# application/vnd.microsoft.portable-executable   ← 插件不认，只会看到灰齿轮
```

修法是写一条 priority 90 的 magic 覆盖。`/usr/share/mime/packages/zz-ms-exe-priority.xml`：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-ms-dos-executable">
    <comment>Windows executable</comment>
    <magic priority="90">
      <match type="string" value="MZ" offset="0">
        <match type="string" value="PE\0\0" offset="64:256"/>
      </match>
    </magic>
  </mime-type>
</mime-info>
```

```sh
update-mime-database /usr/share/mime
xdg-mime query filetype /path/to/your-app.exe   # 期望 application/x-ms-dos-executable
xdg-mime query filetype /path/to/your-app.dll   # dll 也吃这一条
```

- 这条覆盖没有副作用：`mimeapps.list` 里两个类型都指向 `wine-exe.desktop`，双击行为不变。没有它，图标基本出不来，而且**零报错**，只能看到灰齿轮。
- 文件名 `zz-` 开头只是让它在目录里排后面，`update-mime-database` 按 priority 数值算，文件名不影响结果。

**命令行抽图标 `/usr/local/bin/exe-icon`**

单图标组的程序可以用 `wrestool -x -t14 x.exe > a.ico` 一把抓，但程序一旦有主图标加文件类型图标两组，就会拼成坏 ico（`icotool` 只报 `premature end`）。所以按 ID 逐个抽：

```bash
#!/bin/bash
# exe-icon —— 从 Windows PE 文件里抽出内嵌图标
# 用法: exe-icon <文件> [输出目录]
set -u
[ $# -lt 1 ] && { echo "用法: exe-icon <文件.exe> [输出目录]"; exit 1; }
SRC="$1"
[ -f "$SRC" ] || { echo "找不到文件：$SRC"; exit 1; }
command -v wrestool >/dev/null || { echo "缺 icoutils"; exit 1; }
command -v icotool  >/dev/null || { echo "缺 icoutils（icotool）"; exit 1; }

NAME=$(basename "$SRC"); NAME="${NAME%.*}"
OUT="${2:-$PWD/${NAME}_icon}"; mkdir -p "$OUT" || exit 1
WORK=$(mktemp -d) || exit 1; trap 'rm -rf "$WORK"' EXIT

IDS=$(wrestool -l -t14 "$SRC" 2>/dev/null \
      | sed -n 's/.*--name=\([^ ]*\).*/\1/p' | tr -d '"#' | sort -u)
[ -z "$IDS" ] && { echo "$NAME 里没有图标资源"; exit 2; }
NGROUP=$(echo "$IDS" | wc -l); CNT=0

for ID in $IDS; do
    ICO="$WORK/g_$ID.ico"
    wrestool -x -t14 -n "$ID" "$SRC" -o "$ICO" 2>/dev/null
    [ -s "$ICO" ] || continue
    if [ "$NGROUP" -eq 1 ]; then DEST="$OUT/$NAME.ico"; else DEST="$OUT/${NAME}_$ID.ico"; fi
    cp "$ICO" "$DEST"
    mkdir -p "$WORK/p_$ID"
    ( cd "$WORK/p_$ID" && icotool -x "$ICO" >/dev/null 2>&1 )
    CNT=$((CNT+1))
done
[ "$CNT" -gt 0 ] || { echo "$NAME 的图标资源解析失败"; exit 3; }

python3 - "$WORK" "$OUT" "$NAME" "$NGROUP" <<'PY'
import glob, os, shutil, struct, sys
work, out, name, ngroup = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
def dims(p):
    d = open(p, "rb").read(33)
    return struct.unpack(">II", d[16:24])
for d in sorted(glob.glob(os.path.join(work, "p_*"))):
    pngs = glob.glob(os.path.join(d, "*.png"))
    if not pngs: continue
    best = max(pngs, key=lambda p: dims(p)[0] * dims(p)[1])
    w, h = dims(best); gid = os.path.basename(d)[2:]
    dest = os.path.join(out, name + ".png" if ngroup == "1" else f"{name}_{gid}.png")
    shutil.copyfile(best, dest)
    print(f"最大图标 {w}x{h} -> {os.path.basename(dest)}")
PY
echo "输出目录：$OUT"; ls -l "$OUT"
```

- `wrestool -l` 的 ID 带引号和井号（`--name=#1`、`--name='309'`），原样喂给 `-n` 会失败，先 `tr -d '"#'`。
- `icotool` 输出名是 `组号_宽x高_位深.png`，别靠文件名猜尺寸，解析 PNG 的 IHDR（第 16 到 24 字节，大端两个 u32）最稳。
- PE 里最高就 256x256，Windows 图标格式的上限，想要 512 只能放大。
- 有些 exe 压根没资源（pip 自带的 `gui-64.exe` 之类是 stub），`wrestool` 直说 `file contains no resources`，这不是工具坏了。批量找有图标的样本：`for F in $(find /path -iname "*.exe" -size +20k); do wrestool -l "$F" 2>/dev/null | grep -q icon && echo "HAS: $F"; done`

**换桌面图标**

```sh
mkdir -p /usr/local/share/icons
exe-icon /path/to/your-app.exe /usr/local/share/icons   # 换成你自己的程序
# 改 .desktop 的 Icon=/usr/local/share/icons/your-app.png
```

- `Icon=` 给绝对路径合法，png 最保险（个别地方只吃 png/svg）。
- 旧 `.desktop` 备份别留在桌面上，会被当成绿色脚本再显示一遍。挪进 `/root/.backup-desktop/`。

**让缩略图真正出现**

```sh
kbuildsycoca5 --noincremental
kill $(pidof plasmashell)
ADDR=$(grep -h "^DBUS_SESSION_BUS_ADDRESS=" /root/.dbus/session-bus/* 2>/dev/null | head -1 | cut -d"'" -f2)
setsid /usr/bin/env DISPLAY=:0 XDG_RUNTIME_DIR=/tmp/runtime-root \
  DBUS_SESSION_BUS_ADDRESS="$ADDR" QT_QUICK_BACKEND=software KWIN_COMPOSE=0 \
  plasmashell > /tmp/plasma2.log 2>&1 &
```

- 缓存目录 `/root/.cache/thumbnails/{normal,large}`，文件名是 md5，看不出对应谁。验证直接截图，别去翻缓存。
- 重启 plasmashell 必须带 `DBUS_SESSION_BUS_ADDRESS`，不然托盘、通知这些会异常。
- root 跑 Dolphin 会有橙色横条 `Running Dolphin as root can be dangerous`，忽略。首次进几百个 dll 的目录，图标是一格一格刷出来的，等三五秒再截。
- 用 xdotool 导航 Dolphin：`xdotool search --class dolphin | head -1` 再用 `--sync windowactivate`（会报 `_NET_WM_DESKTOP failed`，无害），然后 `key ctrl+l`、`type --delay 30 "/root/Desktop"`、`key Return`。`--name` 匹配中文标题不可靠，用 `--class`。

## b7：窗口尺寸与显示适配

现象：某个 exe 窗口固定 1310x669，竖屏 1080 宽下右边 230 像素（含退出/关于/扫描按钮）全在屏幕外，点不到。

这几条全试过、全无效，别重复：`xprop -remove WM_NORMAL_HINTS` 加 `xdotool windowsize` 能改尺寸但程序不重绘，按钮区一片空白；wine DPI 压到 72 是 983x498、压到 80 是 1091x554，布局都不重排。

**唯一有效的办法是给足屏幕**。Xlorie 的 maximum 是 32767，可以直接改（改的是整个桌面，用户正看着的话画面会变，先说一句）：

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root DISPLAY=:0 /bin/bash -c \
  'xrandr --fb 1920x1080'
```

- 顺手把 DPI 记下来，调窗口时有用：`LogPixels` 是 `HKCU\Control Panel\Desktop` 的 dword，0x48=72、0x50=80、0x60=96（默认）。改完必须 `wineserver -k`。
- 判断窗口是不是被切掉，用 `xwininfo -root -tree | grep -i 程序名`，输出里尺寸和位置都有，一眼能看出比屏幕宽还是被挪出界，别截屏猜。
- 标题里出现 `failure in conversion from UTF8_STRING to ANSI_X3.4-1968` 说明 X 的 locale 不是 UTF-8，不影响使用，但说明 LANG 没传对，按 b4 的四补齐。

## b8：性能调优

按性价比排：

1. 关 KWin 合成（`KWIN_COMPOSE=0` 加 kwinrc `Enabled=false`），这条其实在 a5 就做了。
2. `QT_QUICK_BACKEND=software`，Plasma 的 QML 走软件渲染，避免找 GL 失败。
3. 换 Xlorie 之后，VNC/RDP 那层编码没了，这是最大的一次提速，不是参数能比的。
4. 降分辨率和色深（16 位色）只对 Xvfb 那套有意义；Xlorie 跟屏幕走，能调但收益小。
5. 别旋转（旋转是逐像素重排，纯 CPU）。

- 「画面一点一点扫出来」在老的 Xvfb + VNC 链路里是固有行为（逐块拷像素再编码），不是故障。换 Xlorie 之后应该消失。
- 桌面流畅度和 GPU 基本无关：瓶颈在软件合成。别指望打完 b1 桌面就起飞，b1 加速的是程序自己画的内容。

## b9：跑现代 .NET 应用

**先判断是框架依赖还是自包含**

```sh
cat App.runtimeconfig.json
```

有 `frameworks` 段（`Microsoft.NETCore.App`、`Microsoft.AspNetCore.App`）的是框架依赖，要装运行时；有 `includedFrameworks` 的是自包含，直接跑。

**装 arm64 运行时（拿 tarball，别用 dotnet-install.sh）**

`dotnet-install.sh` 要访问 dot.net，国内绕。

```sh
mkdir -p /data/linuxroot/opt/dotnet
curl -sL --max-time 270 \
  "https://builds.dotnet.microsoft.com/dotnet/aspnetcore/Runtime/11.0.0/aspnetcore-runtime-11.0.0-linux-arm64.tar.gz" \
  -o /data/linuxroot/tmp/aspnet.tgz
tar xzf /data/linuxroot/tmp/aspnet.tgz -C /data/linuxroot/opt/dotnet

chroot /data/linuxroot /usr/bin/env -i PATH=/opt/dotnet:/usr/bin:/bin HOME=/root \
  /opt/dotnet/dotnet --list-runtimes
```

- aspnetcore 运行时已经含 NETCore，Web 应用装这一个就够，不用 apt 装 `dotnet-sdk`。
- `--info` 里看 `RID: linux-arm64`、`Architecture: arm64` 确认没拿错架构。

**先跑通再谈常驻**

```sh
chroot /data/linuxroot /usr/bin/env -i \
  PATH=/opt/dotnet:/usr/bin:/bin HOME=/root LANG=zh_CN.UTF-8 TZ=Asia/Shanghai \
  DOTNET_gcServer=0 DOTNET_CLI_TELEMETRY_OPTOUT=1 \
  ASPNETCORE_URLS=http://0.0.0.0:1066 \
  /bin/bash -c "cd /root/app && timeout 40 /opt/dotnet/dotnet App.dll"
```

成功标志是 `Now listening on: http://[::]:1066` 加 `Application started`。

- `DOTNET_gcServer=0` 别漏。默认可能开 Server GC，手机上吃内存还拖慢启动。
- `ASPNETCORE_URLS` 别漏。不设可能只听 localhost，外面连不上。
- 容器里的时区要先设好（`ln -sf /usr/share/zoneinfo/Asia/Shanghai $M/etc/localtime`），否则日志时间全是 UTC。

**启动脚本 `/root/app/start.sh`**（`/root/app`、`App.dll`、端口 1066 都是示例，换成你自己程序的目录、dll 名和端口；b10 的 guard.sh 同理）

```bash
#!/bin/bash
export PATH=/opt/dotnet:/usr/bin:/bin
export HOME=/root
export LANG=zh_CN.UTF-8
export TZ=Asia/Shanghai
export DOTNET_gcServer=0
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export ASPNETCORE_URLS=http://0.0.0.0:1066
cd /root/app
exec /opt/dotnet/dotnet App.dll
```

## b10：常驻服务与守护

```bash
#!/bin/bash
# /root/app/guard.sh
while true; do
  if ! /usr/bin/pgrep -f "App.dll" > /dev/null 2>&1; then
    echo "[$(date)] restarting app" >> /tmp/app.log
    /bin/bash /root/app/start.sh >> /tmp/app.log 2>&1 &
    sleep 20
  fi
  sleep 60
done
```

起法：

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root \
  /bin/bash -c "setsid /bin/bash /root/app/guard.sh > /tmp/guard.log 2>&1 < /dev/null &"
```

- **`pgrep` 必须写绝对路径 `/usr/bin/pgrep`**。guard 可能在没有 PATH 的环境里被拉起，写裸 `pgrep` 会静默失效，判断永远为真，变成永不停机地重复启动。
- 必须验证守护真的有效，否则等于没装：

```sh
P=$(pidof dotnet); [ -n "$P" ] && kill -9 $P
sleep 75
ps -ef | grep "[A]pp.dll"
curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:1066/
```

同时确认没重复起：`ps -ef | grep -c "[A]pp.dll"` 和 `ps -ef | grep -c "[g]uard.sh"` 都该是 1。

挂到桌面自启。**先分清你用的是哪条路，这两条路的执行位置完全相反**，混着抄必失败：

**① 装了 a7 管理器（推荐）**：写进 a7.5 的 `start.sh` 第 5 步。那份脚本是容器内视角，容器里的命令用 `inbox` 包一层（a7.4 已定义）：

```sh
# 追加到 start.sh 的「5/5」之后。模式里必须带方括号：
# pgrep 自己的 cmdline 里有这个串，不带方括号就永远判成"在跑"
if [ -z "$(inbox "/usr/bin/pgrep -f 'App[.]dll'" 2>/dev/null)" ]; then
  inbox 'setsid /bin/bash /root/app/start.sh >> /tmp/app.log 2>&1 < /dev/null &'
fi
if [ -z "$(inbox "/usr/bin/pgrep -f 'app/guard[.]sh'" 2>/dev/null)" ]; then
  inbox 'setsid /bin/bash /root/app/guard.sh >> /tmp/guard.log 2>&1 < /dev/null &'
fi
```

**② 没装 a7、用 a6 的手工脚本**：入口是**宿主**的 `/data/local/tmp/start-x11-desktop.sh`（不是容器里的 `/start-x11-desktop.sh`，那个不存在，写进去也不会被执行）。追加的必须是宿主视角的命令：

```sh
# 宿主 root shell 里，追加到 /data/local/tmp/start-x11-desktop.sh 末尾
if ! chroot $M /usr/bin/env -i PATH=/usr/bin:/bin /bin/bash -c \
     "/usr/bin/pgrep -f 'App[.]dll'" > /dev/null 2>&1; then
  chroot $M /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c \
    'setsid /bin/bash /root/app/start.sh >> /tmp/app.log 2>&1 < /dev/null &'
fi
```

- 不判重的话，桌面重启一次就多一份，抢同一个端口。
- 改前备份，改后先验语法。走 ① 时 `sh -n "$MODDIR/bin/tasks/start.sh"`；走 ② 时在宿主上 `sh -n /data/local/tmp/start-x11-desktop.sh`（Android 只有 mksh，别用 `bash -n`，宿主上没有 bash）。
- 这是**跟着 Linux 环境启动**走（走 ① 就是每次在 WebUI 里点「启动桌面」都会执行一遍），不是跟着手机开机走。要打游戏腾 CPU 就点「挂起」，别去关它。

## b11：音频（让容器出声）

先给结论：**不要去碰 `/dev/snd`**。这台的 PCM 是 QCOM AudioReach（AGM）的虚拟节点，不是标准 ALSA PCM。`ffmpeg -f alsa hw:0,0`、`plughw:`、`tinyplay` 全部能 open 但 write 返回 `EINVAL`，root 也一样。这不是权限问题，也不是"缺声卡驱动"。`/proc/asound/pcm` 里那一堆 `CODEC_DMA-LPAIF_*` 都是 FE 节点，启流要走 AGM 的 graph，用户态直连没戏。

真正能做的是让一个**能调用 Android 音频 API 的进程**来接活。链路：

```
容器内程序 → 容器内 PulseAudio（null sink 混音）→ monitor → module-simple-protocol-unix
          → UNIX socket → 宿主 aaudio_bridge_unix（C，root 跑）→ AAudio → 喇叭/蓝牙
```

全程不需要额外装 App，不依赖 Termux，也不依赖 `/mnt/hosttmp` 的挂载顺序。

**不要用 `module-pipe-sink`**。它的 FIFO 是非阻塞写，写不进去就丢样本，同时时间戳照样往前推：源码里就写着 `dropped = chunk.length - written`。听感是"每隔一两秒跳一下"，而且 FIFO 多大，延迟就多大（1MB 对应 5 秒延迟），调小只是减少延迟、增加跳音频率，两头都堵死。

**也不要用 `module-alsa-sink` 配 `type file` + `slave.pcm "null"`**。null 设备的 poll fd 是 `/dev/null`（永远可写）、delay 恒为 0、RUNNING 后 avail 恒等于 buffer_size，等于告诉 PulseAudio"我这有个不存在的声卡"。再加上 file 插件是阻塞打开 FIFO，PulseAudio 的 IO 线程可能整个卡在 `write()` 上，而 FIFO 已写出的字节又没法像硬件环形缓冲那样 rewind。

**动手之前先过三个探针**（都很快，失败就别往后做）：

```sh
# ① AAudio 从 Android 8.0（API 26）才有
getprop ro.build.version.sdk        # 低于 26 直接放弃本节

# ② 宿主侧能不能 dlopen libaaudio.so。桥跑起来会先打一行 started，那就是 dlopen 成功了
MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
timeout 3 "$MODDIR/bin/aaudio_bridge_unix" /nonexistent 48000 2 5000 2>&1 | head -3
# 期望 [bridge-unix] started: socket=/nonexistent rate=48000 ch=2 idle_ms=5000
# 出现 dlopen libaaudio.so: ... 说明这台机器没有这个库，或者当前域不许打开

# ③ 容器侧 PulseAudio 的 socket 目录必须可写（后面 simple-protocol 要在这建 socket）
mkdir -p /data/linuxroot/tmp/runtime-root/pulse && chmod 700 /data/linuxroot/tmp/runtime-root/pulse \
  && echo "socket 目录 ok"
```

三关能过只代表「这套链路在这台机器上具备前提」，真正开流成不成只有 b11.5 能验：`openStream: -889/-890` 是设备不给这个格式/缓冲，改 `norewinds` 或换 44.1kHz 再试，别怀疑桥本身。

### b11.1 编 aaudio_bridge_unix

源码全文如下（496 行，单文件无依赖；存成 `aaudio_bridge_unix.c`，下面的编译命令都用这个名字）：

```c
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
```

用 Termux 的 clang，它的 target 本来就是 bionic，不用装 NDK：

```sh
export PREFIX=/data/data/com.termux/files/usr
export LD_LIBRARY_PATH=$PREFIX/lib:$PREFIX/lib/clang/20/lib/linux
$PREFIX/bin/clang -O2 -std=c11 -o aaudio_bridge_unix aaudio_bridge_unix.c -ldl
```

产物约 18KB，`file` 显示 `ELF shared object, 64-bit LSB arm64, dynamic (/system/bin/linker64), for Android 24`。只 NEEDED `libdl.so` 和 `libc.so`。

**没有 Termux 的机器：借 NDK r27 的 sysroot 编**

NDK 官方只发布 x86_64 宿主的工具链（Windows / macOS / Linux-x86_64），它自带的 `aarch64-linux-android24-clang` 是 x86_64 可执行文件，在 arm64 手机上直接跑会 `Exec format error`，所以别去跑它。这条路线只借 NDK 的 sysroot（纯头文件加 stub 库，与宿主架构无关），编译器用容器自带的 clang 18（和 NDK r27c 同一代 LLVM，ABI 一致），链接器用 lld。

```sh
# ① 宿主 root shell：下 NDK r27d（约 634MB），解进容器
curl -L --max-time 1800 -o /data/local/tmp/ndk.zip \
  "https://dl.google.com/android/repository/android-ndk-r27d-linux.zip"
unzip -q /data/local/tmp/ndk.zip -d /data/linuxroot/opt
# 没有 unzip 时用 /data/adb/ksu/bin/busybox unzip -q ...
# 解出来是 /data/linuxroot/opt/android-ndk-r27d

# ② 容器里装 clang 和 lld
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root \
  DEBIAN_FRONTEND=noninteractive /bin/bash -c \
  'apt-get update -qq && apt-get install -y --no-install-recommends clang lld'

# ③ 把源码存成 /root/aaudio_bridge_unix.c，然后在容器里编
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c '
NDK=/opt/android-ndk-r27d/toolchains/llvm/prebuilt/linux-x86_64/sysroot
clang --target=aarch64-linux-android24 --sysroot=$NDK -fuse-ld=lld \
  -O2 -std=c11 -o /root/aaudio_bridge_unix /root/aaudio_bridge_unix.c -ldl'
```

判据和上面一样：`file` 里要有 `for Android 24`，`readelf -d` 只列 `libdl.so` 和 `libc.so`。编完把二进制拷到 `$MODDIR/bin/`。

- 这条路线本机没跑过（本机有 Termux，走的是上面那条）。NDK 的 sysroot 是官方包的标准布局，`--target` 加 `--sysroot` 也正是 NDK 自带 clang wrapper 内部本来就在做的事，等价性有依据；但 Ubuntu 24.04 的 clang 是 18.1.3，遇到版本差异以实际产物为准。
- 装 clang 会占约 500MB，`b13` 收尾时如果不再编译可以卸掉。

AAudio 用 `dlopen("libaaudio.so")` + `dlsym`，不要在编译期链接 NDK stub。

实现上有六条不能省：

1. **收到的是字节流，不是帧流**。UNIX socket 的 `read()` 只保证字节数，不保证按 PCM 帧对齐。必须留一个 `carry` 缓冲，把不足一帧的尾巴放到下次输出最前面，否则左右声道会整体错位。
2. **空闲判据看"非零 PCM"，不看"有没有字节"**。source-output 挂在 monitor 上时会持续推静音块，按字节判闲永远关不了流。改成连续 30 秒没有非零样本就关流。
3. **关流期间必须排空 socket**。否则队列里攒着一堆静音，下次开流先放 5 秒静音。主循环每 100ms 读干 socket 并扫描样本，发现非零立刻开流。
4. **注册 AAudio error callback**。蓝牙断开、设备切换会让流报错，只置一个原子标志，由主线程重建流；回调线程里绝不 close。
5. **压 socket 缓冲和 AAudio 缓冲**：`setsockopt(SO_RCVBUF, ...)` 和 `setBufferSizeInFrames(...)`。默认值（内核给 128KB + 8192 帧）光缓冲就接近 1 秒。源码里最终是 64KB + 8192 帧：16KB 会让上游持续缺空间，2048 帧实测 underrun 40 次，两处都回滚过，原因写在源码注释里。
6. **统计分成四项**：`pcm`（收到多少帧）、`nonzero`（其中非零）、`pad`（补了多少静音）、`xrun`。只看 `pcm` 会被"静音也是数据"骗过去。

### b11.2 容器侧：null sink + simple protocol

（位置②，容器里）

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin \
  DEBIAN_FRONTEND=noninteractive HOME=/root /bin/bash -c \
  'apt-get install -y --no-install-recommends pulseaudio pulseaudio-utils'
```

`/etc/pulse/default.pa`，把 `module-alsa-sink` / `module-pipe-sink` 那几行全部注释掉，换成：

```
load-module module-null-sink sink_name=audio_bridge format=s16le rate=48000 channels=2 norewinds=1
load-module module-simple-protocol-unix socket=/tmp/runtime-root/pulse/aaudio-bridge.sock source=audio_bridge.monitor record=1 playback=0 format=s16le rate=48000 channels=2
set-default-sink audio_bridge
```

`norewinds=1` 不能省：null sink 默认块长 2 秒，加了这个才是 50ms。同时把 `load-module module-detect` 注释掉，容器里没有真硬件。

`/etc/pulse/client.conf`：

```
default-server = unix:/tmp/runtime-root/pulse/native
autospawn = yes
daemon-binary = /usr/bin/pulseaudio
enable-shm = no
```

`/etc/asound.conf`，让只认 ALSA 的程序（SDL 游戏、部分播放器）也走 pulse：

```
pcm.!default { type pulse }
ctl.!default { type pulse }
```

`XDG_RUNTIME_DIR` 必须是 `/tmp/runtime-root`，跟 KDE 那个 `env -i` 一致，否则 socket 路径对不上。

启动（**必须 `--daemonize`，不能 `--start`**）：

```sh
chroot $M /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root \
  XDG_RUNTIME_DIR=/tmp/runtime-root \
  /bin/bash -c '/usr/bin/pulseaudio --daemonize --exit-idle-time=-1 --log-target=file:/tmp/pulse.log'
```

`--start` 会读 client.conf 的 default-server，发现 socket 还不存在就拒绝启动，报 `refusing to start/autospawn`。

### b11.3 GStreamer 程序（Strawberry、Haruna 等）

KDE 默认装的 GStreamer **没有任何音频输出插件**，`libgstpulseaudio.so` 和 `libgstalsa.so` 都不存在，目录里只有一个 `libgstplayback.so`。凡是用 GStreamer 的播放器都会静默无声音。

（位置②，容器里）

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin \
  DEBIAN_FRONTEND=noninteractive HOME=/root /bin/bash -c \
  'apt-get install -y --no-install-recommends gstreamer1.0-pulseaudio'
```

这条会带上 `gstreamer1.0-plugins-good`，mp3/flac/ogg/vorbis 的解码器一并补齐。装完清一次插件缓存，让它重新扫描：

```sh
rm -rf /root/.cache/gstreamer-1.0
```

验证插件都在：

```sh
ls /usr/lib/aarch64-linux-gnu/gstreamer-1.0/ | grep -E 'pulse|mpg123|flac|vorbis'
```

### b11.4 宿主侧：起 bridge

```sh
# 没装 a7 就没有 MODDIR 这个变量，这里给个兜底，桥本身不依赖模块
MODDIR=${MODDIR:-/data/adb/modules/chroot_manager}
SOCK=/data/linuxroot/tmp/runtime-root/pulse/aaudio-bridge.sock
setsid "$MODDIR/bin/aaudio_bridge_unix" "$SOCK" 48000 2 30000 \
  >> /data/local/tmp/bridge.log < /dev/null &
```

桥的二进制也可以放在任何地方（`/data/local/tmp/aaudio_bridge_unix` 之类），a7 只是顺手统一到 `$MODDIR/bin/`。没装 a7 的话，第 4 条那个 `audio.sh` 的说法整段忽略，手动起停这两条命令就够了。

顺序是**先 pulse 后 bridge**：等 socket 出现（最多 8 秒）再拉 bridge。bridge 自己会重连，等待只是让启动日志干净。停止时反过来，先停 bridge 再停 pulse。

`bridge_pid()` 必须同时匹配 `aaudio_bridge` 和 `aaudio_bridge_u`：Linux 的 `comm` 只有 15 字节，`aaudio_bridge_unix` 会被截断。

日常不用手敲这几条。综合管理器里有独立的一组控件：`audio.sh` 负责启停，「跟随容器启动」复选框决定点「启动桌面」时要不要顺带拉起音频。复选框的状态存在 `/data/adb/chroot_manager/state/audio.follow`，`0` 为关、缺省为开。实现见 a7.5。

### b11.5 验证

```sh
# 容器里看 sink
pactl list sinks short
# 期望 0  audio_bridge  module-null-sink.c  s16le 2ch 48000Hz  RUNNING
# Latency 应该在 20ms 以内（norewinds=1 生效的标志）

# 看 source-output 有没有连上
pactl list source-outputs | grep -E 'Source Output|Corked|Source Latency'

# 放歌之后看 bridge 日志
tail -5 /data/local/tmp/bridge.log
```

一条健康的心跳长这样：

```
[bridge-unix] stat 5s: pcm=237312 nonzero=237312 pad=0 (0.0%) xrun=0
```

判定标准：

- `pcm` 和 `nonzero` **必须相等**。`nonzero` 长期为 0 说明数据到了但内容是静音，问题在 pulse 之前
- `pad` 应该接近 0%。持续偏高说明上游供数不稳或缓冲给太小，会听到断续
- `xrun` 保持 0

### b11.6 延迟

蓝牙耳机下整体约 1 秒不要紧张：A2DP 自己就占 300-600ms（编码攒包 + 传输重传 + 耳机解码缓冲），这部分改不了。用手机扬声器对照一次就能分辨。

桥这边可调的就两处：socket 的 `SO_RCVBUF` 和 AAudio 的 `setBufferSizeInFrames`。源码里当前是 64KB + 8192 帧（16KB 会让上游持续缺空间、2048 帧实测 underrun 40 次，都回滚过）。再往下压要看 `pad` 和 `xrun` 的脸色。

### b11.7 已知限制

- **录音**：没做，容器里麦克风仍不可用
- **音量**：走 pulse 的软件音量（`pactl set-sink-volume`），Android 媒体音量会整体再缩放一次
- **体积**：`simple-protocol` 的录制队列硬上限 5 秒（源码常量 `RECORD_BUFFER_SECONDS`），溢出时丢新块。健康消费时永远碰不到

## b12：排查工具箱

`xdotool` 那几行会真的移动鼠标、点到桌面上的东西。用户可能正在用手机，动手前说一声。

```sh
# 后台起 GUI 程序：三件套缺一不可
setsid nohup /usr/local/bin/win app.exe > /tmp/app.log 2>&1 < /dev/null &

# 窗口位置尺寸
DISPLAY=:0 xwininfo -root -tree | grep -i 程序名

# 手动挪窗口 / 解除固定尺寸（先拿到窗口 id，别用 <id> 这种占位符，会被当成重定向）
WID=$(DISPLAY=:0 xdotool getactivewindow)
xdotool windowmove $WID 49 699
xdotool windowraise $WID
xdotool windowsize $WID 1080 554
xprop -id $WID -remove WM_NORMAL_HINTS

# 容器内截图
DISPLAY=:0 HOME=/root scrot -o /tmp/desk.png

# Wine 是否正常
PATH=/usr/local/bin:$PATH HOME=/root win --version
```

- 少 `< /dev/null` 会挂在管上导致 GUI 不出窗口；少 `setsid` 会在 shizuku 会话结束时被一起收掉。
- agent 的读图工具读不了 `/sdcard`，也读不了容器路径，必须先把图拷到工作区可读目录。这是工具边界，不是 Linux 权限问题，`chmod` 半天没用。
- 宿主侧（① 号位置）没有 `/workspace`，那是 agent 沙盒里的挂载名。真路径用 `ls -dt /data/data/*/files/workspaces/*/files | head -1` 查（有多个工作区时注意挑对），容器里则统一是 `/mnt/ws`。

### 程序崩了：先读 minidump，别急着重装

Gecko 系（火狐）和 Chromium 系（Electron 应用）崩溃时会自己写 minidump：

```sh
# Gecko
ls -lt ~/.mozilla/firefox/*.default-*/"Crash Reports"/pending/ | head
ls -lt ~/.mozilla/firefox/*.default-*/minidumps/ 2>/dev/null | head
# Chromium / Electron
ls -lt ~/.config/<程序名>/Crashpad/ 2>/dev/null | head
```

`.extra` 是一行 JSON，不用全解，两个字段够用：

```sh
grep -o '"UptimeTS":"[^"]*"' *.extra      # 启动后多少秒崩的
grep -o '"CrashReason":"[^"]*"' *.extra   # 常常没有，靠信号判断
```

`.dmp` 不用装 minidump_stackwalk，下面这段只回答三件事：什么信号、哪个线程、崩在哪个库。

```python
# mdprobe.py  <崩溃.dmp>   只支持 aarch64
import struct, sys, os, bisect
b = open(sys.argv[1], 'rb').read()
u32 = lambda o: struct.unpack_from('<I', b, o)[0]
u64 = lambda o: struct.unpack_from('<Q', b, o)[0]
n, dd = u32(8), u32(12)
st = {}
for i in range(n):
    o = dd + i * 12
    st.setdefault(u32(o), []).append(u32(o + 8))
mods = []
for rva in st.get(4, []):
    for i in range(u32(rva)):
        o = rva + 4 + i * 108
        base, size, nr = u64(o), u32(o + 8), u32(o + 20)
        ln = u32(nr)
        raw = b[nr + 4:nr + 4 + ln]
        try:
            name = raw.decode('utf-16-le')
        except Exception:
            name = raw.decode('utf-8', 'replace')
        mods.append((base, size, os.path.basename(name)))
mods.sort(); bases = [m[0] for m in mods]
def sym(a):
    i = bisect.bisect_right(bases, a) - 1
    if i >= 0:
        bs, sz, nm = mods[i]
        if bs <= a < bs + sz: return "%s+0x%x" % (nm, a - bs)
    return None
names = {}
for rva in st.get(24, []):
    if not rva:
        continue
    for i in range(u32(rva)):
        o = rva + 4 + i * 12
        nr = u32(o + 4); ln = u32(nr)
        names[u32(o)] = b[nr + 4:nr + 4 + ln].decode('utf-16-le', 'replace')
SIG = {4: 'SIGILL 非法指令', 6: 'SIGABRT', 7: 'SIGBUS', 11: 'SIGSEGV 段错误'}
for rva in st.get(6, []):
    tid, er = u32(rva), rva + 8
    cr = u32(er + 156)
    print("信号      :", u32(er), SIG.get(u32(er), ''), "si_code=%d" % u32(er + 4))
    print("故障地址  : 0x%x" % u64(er + 16))
    print("崩溃线程  :", names.get(tid))
    print("PC        :", sym(u64(cr + 264)) or "匿名映射 → JIT 现编的代码")
    print("LR        :", sym(u64(cr + 248)) or "0x%x" % u64(cr + 248))
```

输出四行就够定位：

```
信号      : 4 SIGILL 非法指令 si_code=1
崩溃线程  : CanvasRenderer
PC        : 匿名映射 → JIT 现编的代码
LR        : libgallium-25.2.8-0ubuntu0.24.04.5.so+0x78ef68
```

怎么读：

- PC 落在某个 `.so` 范围内 → 那个库自己的代码崩了
- PC 是匿名映射、LR 在某个 `.so` → 那个库现编（JIT）出来的代码崩了，调用点是 LR 那个库
- 信号 11 是段错误（野指针、越界），4 是非法指令（JIT 生成了 CPU 不认的指令，或撞到 `udf`），6 是程序自己 abort
- 崩溃线程名比什么都说明问题：`CanvasRenderer` 是画布，`Compositor` 是合成器，`Renderer` 是渲染进程

判读只服务一个决定：这事该不该重装。PC 落在某个第三方库、线程名又指向具体功能，那重装应用没用，要修的是那个库或者绕开它。火狐那条就是这么定案的（见 b14）。

## b13：收尾清理

装完确认能用之后，这些可以删（都能重下或重编）。**删之前先把清单发给用户确认**，别自作主张清盘：这些目录加起来好几个 G，而且用户可能往里面放过自己的东西。

```sh
# 安装素材，200MB+
rm -f /sdcard/Download/ubuntu-base-arm64.tar.gz /sdcard/Download/wine100.tar.xz
rm -f /sdcard/Download/*.downloading

# 失败的 Wine 11
rm -rf /data/linuxroot/opt/wine11

# 编译目录：box64 源码约 1.5GB，确认 /usr/bin/box64 能用后再删
rm -rf /data/linuxroot/usr/src/box64-0.4.4

# Mesa 编译目录：约 1.6GB，Turnip 装好并验证过三连再删
rm -rf /data/linuxroot/root/mesa-build

# 测试残留
rm -rf /data/local/tmp/hello_test /data/local/tmp/chroottest /data/local/tmp/pyt
rm -f /data/local/tmp/*.apk /data/local/tmp/install*.log
```

- 删之前先 `df -h /data` 看余量。这套东西装完会吃掉 4 到 6GB，加上编译目录很容易把盘吃到 95% 以上。
- `/data/linuxroot/tmp` 里的日志可以清，但排障时它是第一现场（`kwin.log`、`pshell.log`、`tx11.log`），别在排查阶段清。
- xfce4 和 KDE 可以共存，想切回 xfce 把启动脚本里的 `kwin_x11` + `plasmashell` 换成 `startxfce4` 就行，但会话管理那套又回到 `dbus-launch` 的老问题，不如留在 KDE。

## b14：常用软件（看图、看视频、听歌、上网）

这四类全用 Linux 原生。别想着在 wine 里跑播放器或浏览器：多套一层 box64，解码退到软件，内存翻倍，崩得还勤。

（位置②，容器里）

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin \
  DEBIAN_FRONTEND=noninteractive HOME=/root /bin/bash -c '
  # 图片 gwenview / 视频 haruna+mpv / 音乐 strawberry
  apt-get install -y gwenview haruna mpv strawberry'
```

浏览器不能用 apt 里的 firefox：Ubuntu 24.04 那个是 snap 转包，chroot 里没有 snapd，装完是空壳。下 Mozilla 官方 aarch64 压缩包：

```sh
cd /tmp && curl -L -o firefox.tar.xz \
  "https://download.mozilla.org/?product=firefox-latest-ssl&os=linux64-aarch64&lang=zh-CN"
cd /opt && tar -xf /tmp/firefox.tar.xz
ln -sf /opt/firefox/firefox /usr/local/bin/firefox
```

`/usr/share/applications/firefox.desktop`：

```ini
[Desktop Entry]
Type=Application
Name=Firefox
Name[zh_CN]=Firefox 浏览器
Exec=/opt/firefox/firefox %u
Icon=/opt/firefox/browser/chrome/icons/default/default128.png
Terminal=false
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
StartupNotify=true
```

文件关联（双击直接打开，全部实测过 query 回读）：

```sh
xdg-mime default org.kde.gwenview.desktop image/png image/jpeg image/webp image/gif
xdg-mime default org.kde.haruna.desktop video/mp4 video/x-matroska video/webm
xdg-mime default org.strawberrymusicplayer.strawberry.desktop \
  audio/mpeg audio/x-mp3 audio/flac audio/x-flac audio/ogg audio/x-wav audio/mp4 audio/x-m4a
xdg-mime default firefox.desktop x-scheme-handler/http x-scheme-handler/https text/html
```

### 音乐播放器 Strawberry

`apt-get install -y --no-install-recommends strawberry`，只多 4 个包、5.6MB，Qt 库 KDE 已经带着了。

**装完必须确认它真的有声音**。它走 GStreamer，而 KDE 默认那套 GStreamer 一个音频输出插件都没有（见 b11.3）。没装 `gstreamer1.0-pulseaudio` 的话，界面一切正常、进度条照走、就是没声音，很容易误判成别的问题。

音频输出显式钉到 pulsesink，配置文件 `/root/.config/strawberry/strawberry.conf`：

```ini
[Backend]
backend=gstreamer

[GStreamerAudio]
output=pulsesink
```

不写这一段也行（`autoaudiosink` 会自己找到 pulsesink），但显式钉死能避免它哪天挑到 alsasink 上去。

第一次启动会弹 "Sponsoring Strawberry" 募捐窗口。关掉之后就不会再出现，防复现的配置项是：

```ini
[MainWindow]
do_not_show_sponsor_message=true
```

**界面是英文的**。Ubuntu 源里的 strawberry 不带中文翻译（`/usr/share/strawberry/translations/` 里没有 `zh_*`），上游也一直没做。功能布局和 Clementine 同源：左侧播放列表、中间曲库、右侧封面歌词，底部播放控制、音量、随机、循环。日常听歌不需要认太多英文。

桌面快捷方式 `/root/Desktop/音乐播放器.desktop`：

```ini
[Desktop Entry]
Version=1.0
Type=Application
Name=音乐播放器
GenericName=Strawberry Music Player
Comment=播放本地音乐
Exec=/usr/bin/strawberry %U
TryExec=/usr/bin/strawberry
Icon=strawberry
Terminal=false
Categories=AudioVideo;Player;Qt;Audio;
StartupNotify=false
StartupWMClass=strawberry
MimeType=x-content/audio-player;audio/flac;audio/ogg;audio/mpeg;audio/x-mp3;audio/x-wav;audio/x-flac;audio/mp4;audio/aac;
```

写完 `chmod 755`。删掉任何旧播放器后要重设文件关联，否则会掉回 wine 的关联项（`.mkv` 双击弹 wine 就是这么来的）：

```sh
xdg-mime default org.strawberrymusicplayer.strawberry.desktop \
  audio/mpeg audio/x-mp3 audio/x-mpeg audio/mp3 audio/mp4 audio/x-m4a \
  audio/aac audio/flac audio/x-flac audio/ogg audio/x-ogg audio/x-vorbis \
  audio/wav audio/x-wav audio/x-wavpack audio/x-ape audio/x-ms-wma
update-desktop-database ~/.local/share/applications
xdg-mime query default audio/mpeg    # 回读确认
```

### 坑：火狐开一两分钟就崩（llvmpipe 的 SIGILL）

从桌面图标点开，20 秒到 1 分钟之间整页消失，弹崩溃报告窗口。连崩三次时间点几乎一样，跟打开什么网页、装没装扩展都无关。

不是火狐的锅。它落在了 llvmpipe（Mesa 的软件 OpenGL）上，LLVM 现编出来的代码里有一条当前 CPU 不认的指令。重装火狐只会把扩展、书签、设置白扔一遍，症状一个不变。

怎么确认：拿崩溃包跑 b12 里的 mdprobe，看到这个组合就定案：

```
信号      : 4 SIGILL 非法指令 si_code=1
崩溃线程  : CanvasRenderer
PC        : 匿名映射 → JIT 现编的代码
LR        : libgallium-25.2.8-.......so+0x78ef68
```

为什么会落到 llvmpipe：火狐走 EGL，而 EGL 这条路上拿不到 DRI3（b1 末尾有专门的边界说明），只能退软件渲染。启动日志里会同时刷这两行：

```
libEGL warning: DRI3 error: Could not get DRI3 device
libEGL warning: Ensure your X server supports DRI3 to get accelerated rendering
```

修法是让火狐彻底不碰 GL。写 `user.js`，它每次启动都覆盖 prefs.js，比直接改 prefs.js 稳：

```sh
cat > ~/.mozilla/firefox/*.default-esr/user.js <<'EOF'
// 绕开 llvmpipe：把所有 GL 路径关掉
user_pref("gfx.canvas.accelerated", false);
user_pref("gfx.canvas.accelerated.force-enabled", false);
user_pref("webgl.disabled", true);
user_pref("webgl.force-enabled", false);
user_pref("gfx.x11-egl.force-disabled", true);
user_pref("layers.acceleration.disabled", true);
user_pref("gfx.webrender.software", true);
user_pref("gfx.webrender.software.opengl", false);
user_pref("media.hardware-video-decoding.enabled", false);
EOF
```

只有一个 profile 时通配符会正常展开（新版可能叫 `*.default-release`，按 `ls` 结果改）；有多个就写死目录名。

验收就看 pending 目录还长不长：

```sh
ls -1 ~/.mozilla/firefox/*.default-esr/"Crash Reports"/pending/ | wc -l
# 开火狐，等两分钟
ls -1 ~/.mozilla/firefox/*.default-esr/"Crash Reports"/pending/ | wc -l   # 数字不变
```

改之前是启动后 22 到 25 秒必崩；改之后连跑几分钟没事，日志里那堆 `GL_EXT_shader_texture_lod unsupported in fragment shader` 也一起消失。

代价是从此纯 CPU 渲染，长页面滚动比正经桌面慢一些。这是这条路的票价，不是新故障。

### 坑：VLC 直接拒绝启动

```
VLC is not supposed to be run as root. Sorry.
```

VLC 3.0 在 `bin/vlc.c` 里写死了 `getuid() == 0` 就退出，没有任何命令行开关能绕过。容器里一切都是 root，所以它装得上、图标也在，点下去就吐这一句然后没了。

换 haruna（mpv 后端，root 下正常）就行。别为了这个去 `useradd` 建普通用户，文件权限会散一地。

### 坑：XDG_RUNTIME_DIR 没设

gwenview / strawberry / haruna 启动都会刷 `QStandardPaths: XDG_RUNTIME_DIR not set, defaulting to '/tmp/runtime-root'`。不影响使用，想干净就在启动脚本里补：

```sh
export XDG_RUNTIME_DIR=/tmp/runtime-root
mkdir -p $XDG_RUNTIME_DIR && chmod 700 $XDG_RUNTIME_DIR
```

### 坑：haruna 其实起来了，只是窗口被挡

窗口标题是 `文件名 — Haruna Media Player`，不是 `haruna`。判断有没有起，别看截图，直接：

```sh
DISPLAY=:0 xdotool search --onlyvisible --name "." getwindowname %@ | sort -u
```

### 解码

mpv/haruna 默认 `--hwdec=auto`，在 Turnip 上能吃到硬解。日志里 `Cannot load libcuda.so.1` 是它在找 NVIDIA，Adreno 上必然出现，无视。

自测：

```sh
ffmpeg -y -f lavfi -i testsrc=duration=6:size=640x360:rate=25 -pix_fmt yuv420p /tmp/test.mp4
mpv /tmp/test.mp4
```

---

## b15：Windows 程序的文件关联自动同步

装完 Windows 版 VLC，双击 mp4 时 KDE 的「打开方式」里看不到它。原因是这套 wine 编译时没带 `winemenubuilder`：

```sh
find /opt/wine10 -name "winemenubuilder*"   # 空的
```

wine 只把关联写进自己的注册表（`HKCR`），不会落到 Linux 的 `~/.local/share/applications`。所以得自己补一个同步器。

### 原理

1. 扫 `~/.wine10/system.reg` 和 `user.reg`
2. 找 `[Software\Classes\.<ext>]` 的默认值，拿到 ProgID
3. 找 `[Software\Classes\<ProgID>\shell\open\command]`，拿到命令行
4. `C:\...` 换成 `<prefix>/drive_c/...`，生成 `wine-<exe>.desktop`
5. `update-desktop-database` 刷新缓存

一个 exe 只生成一条 desktop，所有扩展名并进 `MimeType=`。不合并的话「打开方式」里会刷出几十条同名 VLC。

reg 文件有两层转义要注意：section 名和值里的 `\\` 是反斜杠、`\xXXXX` 是 Unicode 码点。不还原的话一个 ProgID 都解析不出来，会静默返回 0 个程序。

### 脚本

`/usr/local/bin/wine-mime-sync`，Python 3，无第三方依赖：

```python
#!/usr/bin/env python3
import os, re, sys, glob, shutil, subprocess
HOME = os.environ.get("HOME", "/root")
PREFIX = os.environ.get("WINEPREFIX", HOME + "/.wine10")
APP_DIR = os.path.join(HOME, ".local/share/applications")
ICON_DIR = os.path.join(HOME, ".local/share/wine-mime-icons")
TAG = "X-Wine-Mime-Sync"
SEC = re.compile(r"^\[(.+?)\](?:\s+\d+)?$")

def unesc(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i+1]
            if n in "\\\"": out.append(n); i += 2; continue
            if n == "x":
                try: out.append(chr(int(s[i+2:i+6], 16))); i += 6; continue
                except ValueError: pass
            out.append(n); i += 2; continue
        out.append(c); i += 1
    return "".join(out)

def load(path):
    d, cur = {}, None
    if not os.path.exists(path): return d
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\r\n")
        m = SEC.match(line)
        if m:
            cur = unesc(m.group(1)).lower(); d.setdefault(cur, {}); continue
        if cur is None or "=" not in line: continue
        k, v = line.split("=", 1)
        if v.startswith('"') and v.endswith('"') and len(v) >= 2:
            d[cur][k.strip().strip('"').lower()] = unesc(v[1:-1])
    return d

def globs():
    t = {}
    p = "/usr/share/mime/globs2"
    if os.path.exists(p):
        for line in open(p, encoding="utf-8", errors="replace"):
            if line.startswith("#"): continue
            parts = line.split(":")
            if len(parts) >= 3 and parts[2].startswith("*."):
                t.setdefault(parts[2][2:].strip().lower(), parts[1])
    return t

def to_unix(p):
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", p.strip().strip('"'))
    if not m: return None
    d, rest = m.group(1).lower(), m.group(2).replace("\\", "/")
    base = PREFIX + "/drive_c" if d == "c" else PREFIX + "/dosdevices/" + d + ":"
    return os.path.join(base, rest)

def split_cmd(c):
    c = c.strip()
    if c.startswith('"'):
        e = c.find('"', 1)
        if e > 0: return c[1:e], c[e+1:].strip()
    p = c.split(" ", 1)
    return p[0], p[1].strip() if len(p) > 1 else ""

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode in ("-h", "--help"):
        print("用法: wine-mime-sync [--list | --remove]"); return
    reg = {}
    for n in ("system.reg", "user.reg"):
        reg.update(load(os.path.join(PREFIX, n)))
    g = globs()
    apps = {}
    for sec, v in reg.items():
        m = re.match(r"^software\\classes\\(\.\w+)$", sec)
        if not m: continue
        pid = v.get("@", "").strip()
        if not pid: continue
        ext = m.group(1)[1:]
        cmd = reg.get("software\\classes\\" + pid.lower() + "\\shell\\open\\command", {}).get("@", "")
        if not cmd: continue
        exe, args = split_cmd(cmd)
        u = to_unix(exe)
        if not u or not os.path.exists(u) or "winebrowser" in u.lower(): continue
        mime = g.get(ext)
        if not mime: continue
        a = apps.setdefault(u, {"mimes": set(), "args": args})
        a["mimes"].add(mime)
        fa = reg.get("software\\classes\\applications\\" + os.path.basename(u).lower(), {}).get("friendlyappname")
        if fa: a["name"] = fa
    os.makedirs(APP_DIR, exist_ok=True)

    def slug(p):
        return re.sub(r"[^a-z0-9]+", "-", os.path.basename(p).lower()).strip("-")

    if mode == "--list":
        if not apps:
            print("没有解析到任何可关联的程序（注册表里没有带 exe 的 Classes 关联）")
            return
        for exe in sorted(apps):
            a = apps[exe]
            print("%s  ->  wine-%s.desktop" % (exe, slug(exe)))
            for m in sorted(a["mimes"]):
                print("    %s=wine-%s.desktop" % (m, slug(exe)))
        return

    if mode == "--remove":
        n = 0
        for f in glob.glob(os.path.join(APP_DIR, "wine-*.desktop")):
            try:
                txt = open(f, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            if (TAG + "=true") in txt:      # 只认自己生成的，手工加的条目不碰
                os.remove(f); n += 1
        subprocess.run(["update-desktop-database", APP_DIR],
                       stdout=subprocess.DEVNULL, timeout=60)
        print("已删除 %d 个同步生成的 desktop" % n)
        return

    if mode:
        print("未知参数: %s（只支持 --list / --remove）" % mode); return

    for exe, a in apps.items():
        base = slug(exe)
        icon = os.path.join(ICON_DIR, base + ".png")
        icon = icon if os.path.exists(icon) else "wine"
        args = a["args"].replace("%1", "%f") or '"%f"'
        open(os.path.join(APP_DIR, "wine-%s.desktop" % base), "w").write(
            "[Desktop Entry]\nType=Application\nName=%s (Wine)\nExec=/usr/local/bin/win \"%s\" %s\n"
            "Icon=%s\nMimeType=%s\nTerminal=false\nCategories=Wine;\n%s=true\n"
            % (a.get("name", os.path.basename(exe)), exe, args, icon,
               ";".join(sorted(a["mimes"])), TAG))
    subprocess.run(["update-desktop-database", APP_DIR],
                   stdout=subprocess.DEVNULL, timeout=60)

main()
```

图标那步在上面省掉了：完整版会调 `exe-icon` 抠 png 缓存到 `~/.local/share/wine-mime-icons/<exe>.png`，抠不出来就退回 `Icon=wine`。

### 自动触发

`/usr/local/bin/wine-mime-watch` 每 8 秒比一次注册表的时间戳和大小，变了就跑 sync。装完 Windows 软件最多 8 秒，Linux 侧就能看到：

```sh
#!/bin/bash
PREFIX="${WINEPREFIX:-$HOME/.wine10}"
SYNC=/usr/local/bin/wine-mime-sync
sig() { stat -c "%Y:%s" "$PREFIX/system.reg" "$PREFIX/user.reg" 2>/dev/null | tr '\n' '|'; }
last=""
while true; do
  cur=$(sig)
  if [ -n "$cur" ] && [ "$cur" != "$last" ]; then
    last="$cur"; [ -x "$SYNC" ] && "$SYNC" >/dev/null 2>&1
  fi
  sleep 8
done
```

挂进 KDE 自启 `~/.config/autostart/wine-mime-watch.desktop`：

```ini
[Desktop Entry]
Type=Application
Name=Wine 文件关联同步
Exec=/usr/local/bin/wine-mime-watch
Terminal=false
NoDisplay=true
X-KDE-autostart-after=panel
```

### 验证

```sh
# 解析到哪些程序、各自挂了哪些 MIME
wine-mime-sync --list

# 确认 mp4 挂上了
grep -o "video/mp4=[^;]*" ~/.local/share/applications/mimeinfo.cache
# 期望 video/mp4=wine-vlc.desktop（指的是 Wine 里的 Windows 版 VLC；b14 装的那个 Linux 版 VLC 起不来，两者别混）

# 按 desktop 里的 Exec 行手跑一次，能弹出 VLC 就说明整条链通了
/usr/local/bin/win "/root/.wine10/drive_c/Program Files/VideoLAN/VLC/vlc.exe" \
  --started-from-file "/tmp/test.mp4"
```

测自动：删掉 `wine-vlc.desktop`，再 `touch ~/.wine10/system.reg`，14 秒内它应该自己长回来。

### 坑：VLC 的图标抠不出来

VLC 的图标资源名是 `VLC_ICON`，不是数字 ID。`exe-icon` 按数字 ID 找会漏掉这个程序。手动来：

```sh
wrestool -x -t14 --name=VLC_ICON "…/vlc.exe" > /tmp/vlc.ico
icotool -x -o /tmp/vlcpng /tmp/vlc.ico
cp /tmp/vlcpng/vlc_3_256x256x32.png ~/.local/share/wine-mime-icons/vlc-exe.png
```

`icotool` 会报 `clr_important field in bitmap should be zero`，无视，导出的 png 是好的。

### 和 winemenubuilder 的关系

按 b3 补过 winemenubuilder 之后，它每次装完软件会自己刷一批 `wine-extension-*.desktop`。那批条目的毛病是 `NoDisplay=true`，KDE 的「打开方式」里看不见；而这里的同步器产出是 `NoDisplay=false`，一个 exe 合并成一条，能直接选。

两套可以共存，重复的条目最多让列表多一行，不冲突。

顺带一句：winemenubuilder 生成的那 120 多个条目，文件名是 `wine-extension-<ext>.desktop`；同步器生成的是 `wine-<exe>.desktop`，互不覆盖。`--remove` 只删后者。

### 注意

- 同步器不设默认程序，只把条目注册进 MIME 库，双击时 KDE 弹「打开方式」让你选。要设默认再 `xdg-mime default wine-vlc.desktop video/mp4`。
- `--list` 只读不写：列出解析到的程序和它挂的 MIME，用来确认链路通没通。`--remove` 只删自己生成的（认 `X-Wine-Mime-Sync=true` 标记），不会碰手工加的条目。
- 同一个 exe 关联几十种扩展名是正常的（VLC 关联了 60 多个），合并成一条 desktop 后不影响使用。

---

# C、附录：作者自己装东西的记录（不是必做流程）

这一块是作者在自己机器上装某些具体软件的实操记录。**A 和 B 做完就算完工**，C 不参与「容器能点亮、能用」的判定。

留在这里只是因为下次装类似东西时可能有点启发：怎么改第三方 .NET 应用的 SQLite 配置、怎么给一个保活程序配 Azure 权限、在国内网盘怎么把文件抠下来。没有这些需求就直接跳过。

## c1：改 SQLite 里的 JSON 配置

下面的 `/root/app/appdata/App.db` 是示例，指 c2 部署的 E5 Renew X 的库，换成你自己应用的库路径；表 `AppConfigs`、列 `Key`/`Value` 也是那套程序的结构，别当通用模板。

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin HOME=/root /bin/bash -c '
/usr/bin/python3 - <<PY
import sqlite3, json
c = sqlite3.connect("/root/app/appdata/App.db", timeout=15)
v = c.execute("select Value from AppConfigs where Key=\"SystemConfig\"").fetchone()[0]
print(json.dumps(json.loads(v), ensure_ascii=False, indent=1))
PY'
```

写回时照原样：

```python
d["Serivce"]["PauseOnHighErrorRate"] = False
c.execute("update AppConfigs set Value=? where Key='SystemConfig'",
          (json.dumps(d, ensure_ascii=False),))
c.commit()
```

- **键名可能有拼写错误，照原样用，别「修正」**。E5 Renew X 里字段就叫 `Serivce` 不是 `Service`，改了程序读不到。
- 改完必须重启进程，这类配置一般在启动时读进内存。顺手清运行态字段：`update Runs set IsPaused=0, PausedReason=""`。
- 有些保活类程序带「错误率过高自动暂停」，手机上网络一抖就被停，还得人工重启。找 `PauseOnHighErrorRate` 之类关掉。

## c2：E5 续期工具（Azure 注册 + Graph 补权限 + 部署）

以 E5 Renew X 为例，完整链路。凭据用占位符，实际值别写进文档。

**这一节动的是用户的微软账号和租户权限，每一步开始前都要问一句**，尤其是注册应用、授予管理员同意、把密码写进文件这三步。别自己跑完再汇报，账号相关的东西用户必须知情。

### 一、注册应用（不能借别人的 client_id）

微软 2024 年起封了借第一方 client_id 这条路，试探必得 `AADSTS65002: Consent between first party application ... must be configured via preauthorization`。必须自己注册。

1. 用**目标租户的管理员账号**登录 `portal.azure.com`（`xxx@xxx.onmicrosoft.com` 这种，不是建订阅时用的个人微软账号）。
2. 搜「应用注册」→「新注册」。
3. 账户类型选「仅此组织目录中的账户」。
4. 重定向 URI 留空，点注册。

- 注册完默认停在「拥有的应用程序」标签，新应用经常不显示，切到「所有应用程序」才看得到。
- 表单在跨域 iframe（`sandbox-1.reactblade.portal.azure.net`）里，往页面注入 JS 也进不去（本文实测用的是 agent 的 browser_eval_js），React 受控输入用原生 setter 改值也不会更新 state，注册按钮一直 `aria-disabled=true`。**填名字和点注册必须人来点**，别硬试。
- 左侧「身份验证」→「高级设置」→「**允许公共客户端流**」改成「是」。不开会报 `AADSTS7000218`。
- 左侧「API 权限」→「添加权限」→「Microsoft Graph」→「**委托的权限**」→ 勾选 → 添加 → 再点「**代表 XXX 授予管理员同意**」。不做会报 `AADSTS65001`。

### 二、scope 必须一次性全给

有些保活程序把全部 scope 拼在一个请求里，缺任何一个整个 token 都拿不到，日志只写「应用程序缺少API权限」，看不出缺哪个。排查时从 dll 里抠：

```sh
strings -e l -n 8 App.dll | grep -aiE "graph\.microsoft\.com/[A-Za-z.]+|\.Read|\.ReadWrite" | sort -u
```

E5 Renew X 的完整委托权限（16 个）：

```
BookingsAppointment.ReadWrite.All  Calendars.Read  Contacts.Read  Directory.Read.All
Files.Read.All  Files.ReadWrite.All  Group.Read.All  Mail.Read  Mail.Send
MailboxSettings.Read  Notes.Read.All  People.Read.All  Presence.Read.All
Sites.Read.All  Tasks.ReadWrite  User.Read.All
```

- `BookingsAppointment.ReadWrite.All` 在没开 Bookings 的租户里搜不到，跳过一般不影响其他。
- 表现不是「某个 API 失败」而是「**全挂**」，排查时别一个个 API 找原因。

### 三、ROPC 拿 token 前的两条硬规则

- 端点必须 `/organizations` 或指定租户 ID，**不能 `/common`**，否则 `AADSTS9001023`。
- 账号开了 MFA 就走不通（`AADSTS50076`），ROPC 是微软明确不支持 MFA 的流程。这种情况只能改走客户端凭据。

先花两分钟确认密码流程通不通：

```sh
curl -s -X POST "https://login.microsoftonline.com/organizations/oauth2/v2.0/token" \
  -d "client_id=<ClientId>" \
  --data-urlencode "scope=https://graph.microsoft.com/.default" \
  --data-urlencode "username=<账号>" \
  --data-urlencode "password=<密码>" \
  -d "grant_type=password"
```

能拿到 `access_token` 就不用去折腾门户那一坨。

### 四、补权限不用点界面（直接调 Graph REST）

门户页面自己就拿着高权限 Graph token，存在 `sessionStorage` 里，key 形如 `msal.3|<clientId>.<tenantId>|login.windows.net|accesstoken|<resourceId>|`，其中一个 `aud` 是 `https://graph.microsoft.com`、`scp` 带 `Application.ReadWrite.All`。

在门户页面的控制台里执行（浏览器的 DevTools 也行，本文实测用的是 agent 的 browser_eval_js）：

```js
(function(){
  let tok = "";
  Object.keys(sessionStorage).filter(k=>/accesstoken/i.test(k)).forEach(k=>{
    if (tok) return;
    let raw = sessionStorage.getItem(k), sec = raw;
    try { const o = JSON.parse(raw); sec = o.secret || o.accessToken || raw; } catch(e){}
    if (typeof sec !== "string") return;
    const p = sec.split(".");
    if (p.length < 3) return;
    let b = p[1].replace(/-/g,"+").replace(/_/g,"/");
    while (b.length % 4) b += "=";
    let j = {};
    try { j = JSON.parse(atob(b)); } catch(e){ return }
    if ((j.aud||"").indexOf("graph.microsoft.com") >= 0
        && (j.scp||"").indexOf("Application.ReadWrite") >= 0) tok = sec;
  });
  window.__tok = tok;
  return tok ? ("tok ok len " + tok.length) : "no token";
})()
```

接着：查应用对象 ID 和现有 `requiredResourceAccess` → 查 Microsoft Graph 的 servicePrincipal 和每个 scope 的 GUID → `PATCH /applications/<对象ID>` → `PATCH /oauth2PermissionGrants/<grant id>` 把 scope 并进去。

- **必须用同步 `XMLHttpRequest`**。在这种注入沙箱里，`fetch` 的 await 不会返回，只拿到 `{}`。
- `servicePrincipals?$filter=appId eq '00000003-0000-0000-c000-000000000046'` **返回空数组且不报错**。要用 `$filter=displayName eq 'Microsoft Graph'`，或者用已知对象 ID `6e9e7b8e-037f-4eb5-ae1b-f4d6f19bf2c3`。
- `consentType` 是 `AllPrincipals` 就等价于「代表整个组织授予管理员同意」。
- 补完必须逐端点验证，全 200 才算通：

```sh
T="粘贴你拿到的 access_token"   # 占位符别写成 <token>，尖括号会被当成重定向
for EP in "me" "me/messages?\$top=1" "me/drive/root" "me/events?\$top=1" "me/contacts?\$top=1"; do
  C=$(curl -s -o /dev/null -w "%{http_code}" -H "Authorization: Bearer $T" "https://graph.microsoft.com/v1.0/$EP")
  echo "  /$EP -> $C"
done
```

- 应急歪招（已回滚，记一下方法）：把未授权的 scope 名在 DLL 里替换成等长空格（UTF-16LE，字符数不变，不破坏 .NET 的 #US 堆），35 条全失败变成 19 成功 10 失败。这是应急不是正解，权限补齐后必须还原。

### 五、部署与凭据

数据库是 SQLite，配置文件在 `appdata/RenewX.db` 的 `AppConfigs` 表（改法见 c1）。日志写 `/tmp/renewx.log`，守护用 b10 那套。

- 凭据让用户写文件，别在聊天里贴：`/sdcard/Download/e5.txt` 第一行账号、第二行密码。读的时候要处理 BOM 和 CRLF：`sed -n '1p' "$F" | tr -d '\r' | sed 's/^\xEF\xBB\xBF//'`。
- 管理员密码之类的初始化参数在程序首次启动后生成，别在文档里写死。

## c3：国内网盘下载（蓝奏云为例）

以 `lanzoux.com/<shareid>` 形式（子域是你自己的分享页）为例，四道关，全流程可脚本化。

**第一关：域名被反海外解析成 127.0.0.1**

```sh
getent hosts <子域>.lanzouq.com    # 127.0.0.1
```

连阿里 DoH 和腾讯 DoH 都返回 127.0.0.1，说明是域名本身的限制，**换 DNS 没用**。解法是换备用域名挨个试：

```sh
for h in www.lanzouq.com lanzouq.com www.lanzoui.com \
         www.lanzoux.com www.lanzoup.com; do
  printf "%-28s " "$h"; getent hosts "$h" 2>/dev/null | head -1 || echo FAIL
done
```

分享页给的那种带子域的地址（子域.lanzoux.com）要单独再试一遍，能解析的往往只有它。

**第二关：分享页里的 fn 路径带签名**

页面里有两样东西：`<iframe src="/fn?AmRQOl04...">`（带签名的路径）和 `var fid = 89419686;`（文件 id），两个都要。

**第三关：阿里云 acw_sc__v2 JS 挑战**

直接 GET 那个 fn 路径会拿回一坨 gzip 混淆 JS，里面有 `var arg1='0FAD...'`。算法是「按固定置换表重排 arg1，再和常量逐字节 XOR」：

```javascript
function acw(arg1) {
  const m = [0xf,0x23,0x1d,0x18,0x21,0x10,0x1,0x26,0xa,0x9,0x13,0x1f,0x28,0x1b,
             0x16,0x17,0x19,0xd,0x6,0xb,0x27,0x12,0x14,0x8,0xe,0x15,0x20,0x1a,
             0x2,0x1e,0x7,0x4,0x11,0x5,0x3,0x1c,0x22,0x25,0xc,0x24];
  const p = '3000176000856006061501533003690027800375';
  const q = [];
  for (let i = 0; i < arg1.length; i++)
    for (let j = 0; j < m.length; j++)
      if (m[j] === i + 1) q[j] = arg1[i];
  const u = q.join('');
  let v = '';
  for (let i = 0; i < u.length && i < p.length; i += 2) {
    let A = (parseInt(u.substr(i, 2), 16) ^ parseInt(p.substr(i, 2), 16)).toString(16);
    if (A.length === 1) A = '0' + A;
    v += A;
  }
  return v;
}
```

算出来带 `Cookie: acw_sc__v2=<值>` 再请求。

**第四关：网络异常二次验证**

过了 acw 可能返回「系统发现您的网络异常」，页面 JS 里有 `$.ajax({url: 'ajax.php', data: {file, el, sign}})`。

- 是**相对路径**，实际是 `https://<host>/file/ajax.php`，不是根目录的 `/ajax.php`（写错返回 404 HTML 而不是 JSON）。`el` 取 1/2/3，实测 2 能过。

**直链会超时，必须续传**

```sh
curl -L -C - --max-time 200 -A "Mozilla/5.0" -e "https://developer4.lanrar.com/" "$URL" -o /tmp/x.rar
```

实测 240 KB/s，67MB 的文件在 260 秒里只能下 62MB。多跑几次 `-C -`。

- OneDrive / SharePoint 源先 HEAD，302 跳到 `login.microsoftonline.com` 就是要登录，没 cookies 拿不到，自己硬啃比较费劲。
- 实在下不动就让用户用 IDM 之类自己下，之后比 md5 确认是同一份。
- **RAR5 要装 nonfree 的 unrar**。7z 23.01 和 unrar-free 都报 `Unsupported Method`。装 `unrar`（7.0.7）后 `unrar x -o+ file.rar /root/out/`。
- 装 APK 同理有坑：真机 `pm install /sdcard/xxx.apk` 会因 SELinux 拒绝 system_server 读 fuse 而失败（`avc: denied { read } ... tcontext=u:object_r:fuse:s0`）。必须先 `cp` 到 `/data/local/tmp` 再装，而且下完必须校验 zip 完整（代理会静默截断，`python3 -c "import zipfile; print(len(zipfile.ZipFile('x.apk').namelist()))"` 报 BadZipFile 就是残包）。

---

## c4：给网页版服务做桌面快捷方式（网易云音乐为例）

目的是把「打开浏览器 → 输网址 → 登录 → 找到播放器」压成桌面点一下。做法就是一个 `.desktop`，让 Firefox 新开一个窗口直接落到目标地址。

前提是 b14 的 Firefox 已经装好（`/opt/firefox/firefox`）。

`/root/Desktop/网易云音乐.desktop`：

```ini
[Desktop Entry]
Type=Application
Name=网易云音乐
Exec=/opt/firefox/firefox --new-window https://music.163.com/st/webplayer
Icon=/opt/firefox/browser/chrome/icons/default/default128.png
Terminal=false
Categories=AudioVideo;Audio;Player;
StartupNotify=true
```

```sh
chmod 755 /root/Desktop/网易云音乐.desktop
```

三个位置的讲究：

- **`--new-window` 不能省**。不加的话会变成在现有窗口里新开标签页，用户点了图标却觉得"什么都没发生"。
- **`Icon` 指绝对路径**，不要写 `firefox` 之类的主题名。KDE 只在这个图标名能被 `Icon=` 解析到具体文件时才显示，绝对路径最稳。
- **`StartupNotify=true`** 让用户点击后有加载反馈，网页版首次加载要几秒。

要换别的服务，改 `Name`、`Exec` 里的 URL、`Icon` 三处就行。同类常见目标：

```
网易云音乐    https://music.163.com/st/webplayer
QQ 音乐       https://y.qq.com/
B 站          https://www.bilibili.com/
YouTube Music https://music.youtube.com/
```

图标想用自己的，把 png 放到 `/root/.local/share/icons/` 下再改 `Icon=` 指向它。

第一次点开如果弹出「不受信任的启动器」，点「启动」或「信任并启动」即可，这是 KDE 对新建 `.desktop` 的默认保护。之后不再提示。

## c5：X11 在后台时怎么截屏

X server（Xlorie）是独立进程，framebuffer 在内存里，跟 Android 的 Surface 可不可见无关。所以在 chroot 里抓 root window，就能拿到后台的桌面画面：

```sh
chroot /data/linuxroot /usr/bin/env -i \
  PATH=/usr/bin:/bin HOME=/root DISPLAY=:0 XAUTHORITY=/root/.xauthority \
  sh -c 'scrot -o /tmp/shot.png'
```

实测：Android 前台停在 TIM 聊天界面时，截出来的是完整的 KDE 桌面。

别用 Android 层的截图（无障碍截图、`screencap` 那类）干这个活：它们截的是 Android 合成后的那一帧，X11 在后台时只会截到前台那个 App。

三个环境变量缺一不可：

- `DISPLAY=:0` —— X server 就在 :0
- `XAUTHORITY=/root/.xauthority` —— root 用户的 X 授权文件，不指过去 scrot 连不上
- `HOME=/root` —— 有些工具用 HOME 找配置

`-o` 是覆盖已有文件，不加的话同名文件存在时直接失败。

### 截图之后怎么交给 agent

产物落在 **chroot 的 /tmp**，也就是宿主 `/data/linuxroot/tmp/shot.png`。agent 的文件工具一般只能读工作区，读不到 `/data/linuxroot` 下面的路径，所以要先挪一手：

```sh
chroot /data/linuxroot /usr/bin/env -i PATH=/usr/bin:/bin \
  sh -c 'cp -f /tmp/shot.png /mnt/ws/'
```

`/mnt/ws` 是工作区 bind mount，之后 agent 从工作区里就能读到这张图。

### 窗口被挡住

先把它顶到前面，再截全屏：

```sh
export DISPLAY=:0 XAUTHORITY=/root/.xauthority
xdotool search --name "Haruna" windowactivate --sync
scrot -o /tmp/shot.png
```

### 同一招的延伸

`xdotool` 也是这个道理：它通过 X11 协议往窗口发事件，不需要 App 在前台：

```sh
xdotool search --name "Wine 游戏运行库合集" windowactivate --sync
xdotool key --window <id> Return
xdotool windowminimize <id>
```

用这套在后台点完了整个 NSIS 安装器的向导（欢迎页 → 预设 → 组件 → 安装 → 完成），全程 Android 前台都是别的 App。

### 坑

- X server 没起来时 scrot 连不上 display，先 `ps -ef | grep termux` 看 Xlorie 在不在
- 截图尺寸是 **X 的分辨率**（这台 720x1600），不是 Android 的屏幕尺寸，坐标换算别搞混
- 想让窗口在前台再截，用 `xdotool windowactivate --sync <id>`，别去切 Termux:X11 那个 App
