# Qt Extended 4.4.3 在龙芯 loongarch64 上的原生移植

本项目记录 **Qt Extended 4.4.3（Qtopia 最后一版）** 在 **龙芯 3B6000M / loongarch64 / AOSC OS / GCC 16** 上原生编译并运行的全过程，包含可复现的补丁、构建脚本、回归测试与排错记录。

最终成果：`qpe`（Qtopia Server）在本机原生运行，通过 VNC 提供 640×480 图形界面，鼠标/键盘交互正常。

![平台](https://img.shields.io/badge/arch-loongarch64-blue) ![OS](https://img.shields.io/badge/OS-AOSC%20OS-green) ![GCC](https://img.shields.io/badge/GCC-16-orange) ![License](https://img.shields.io/badge/LICENSE-GPLv2-lightgrey)

---

## 1. 环境

| 项目 | 值 |
|------|-----|
| CPU | 龙芯 3B6000M（loongarch64，8 核） |
| OS  | AOSC OS，glibc 2.44 |
| GCC | 16.2.0（默认 `-O2`，本项目强制 `-O0`，见后文） |
| Qt  | Qt Extended 4.4.3 内置 Qt 4.4.3（QWS/Embedded） |
| 显示 | QWS VNC 后端（无需 root、无需 framebuffer 权限） |

为什么不用 X11/framebuffer：`/dev/fb0` 属 root:video，普通用户无权限，且本项目坚持不动系统盘/不用 root。VNC 后端是纯用户态方案。

## 2. 获取源码

官方 `qtextended.org` 已下线，从 SourceForge 下载（181 MB）：

```bash
wget -O qt-extended-opensource-src-4.4.3.tar.gz \
  https://downloads.sourceforge.net/project/qpe/QPE/qtopia/qt-extended-opensource-src-4.4.3.tar.gz
sha256sum qt-extended-opensource-src-4.4.3.tar.gz
# 期望: 6878deed374f37c86214da728945c8b1402a0e3d2c722368f70e62d050cd6d87
tar -xzf qt-extended-opensource-src-4.4.3.tar.gz
```

SourceForge 单连接约 65 KB/s 且大文件常中途截断。如果下载慢/断流，用并行 Range 分块下载（见 `docs/DOWNLOAD.md`）。

## 3. 应用补丁

在**源码根目录的父目录**执行（补丁路径带 `qt-extended-4.4.3/` 前缀，`-p1` 进入源码根后应用）：

```bash
cd <解压目录>/qt-extended-4.4.3
for p in /path/to/qt-extended-loongarch64-repo/patches/*.patch; do
    patch -p1 --batch < "$p"
done
```

四个补丁按主题分组，按 `0001`→`0004` 顺序应用：

| 补丁 | 内容 |
|------|------|
| `0001-loongarch64-arch.patch` | 新增 loongarch64 架构目录与 mkspec；generic 原子操作改为 GCC `__sync_*` 无锁实现；host mkspec 强制 `-O0` |
| `0002-gcc16-core.patch` | Qt core 在 GCC 16 下的未定义行为修复（`Q_FOREACH` 宏、指针比较、`if_indextoname` 原型等） |
| `0003-vnc-backend.patch` | QWS VNC 后端：`readFully`、像素格式初始化、16→32 转换、Raw 编码、消息类型 |
| `0004-gcc16-apps.patch` | Qt Extended 应用代码的 GCC 16 兼容（柔性数组、私有枚举、`abs(uint)`、指针提升、隐式声明等） |

重新生成补丁：`scripts/gen-patches.sh <pristine-root> <modified-root> <output-dir>`。

## 4. 编译

核心要点：AOSC 无 Qt4 开发包，必须用 `-force-build-qt -build-qt` 从内置源码自举 host Qt；`-xplatform` 只写 bare 名 `linux-loongarch64-g++`（脚本会自己拼 `mkspecs/qws/`）。

```bash
cd <解压目录>                       # 与 qt-extended-4.4.3 同级的构建根
mkdir -p build
export QTOPIA_DEPOT_PATH=$PWD/qt-extended-4.4.3
export QPEDIR=$PWD
export MAKEFLAGS=-j8

echo yes | $QTOPIA_DEPOT_PATH/configure \
  -force-build-qt -build-qt \
  -arch loongarch64 -xplatform linux-loongarch64-g++ \
  -ui-type home -release -no-error \
  -prefix $PWD/install \
  -no-dbus -no-dbusipc -no-ssl -no-sxe -no-phonon -no-v4l2 -no-vpn -no-libamr \
  -remove-module bluetooth,drm,infrared \
  -extra-qt-config "-qt-zlib -qt-libpng -qt-libjpeg -qt-freetype -no-webkit -no-phonon -no-dbus -no-cups -no-nis -no-qt3support" \
  -extra-qt-embedded-config "-qt-zlib -qt-libpng -qt-libjpeg -qt-freetype -no-webkit -no-phonon -no-dbus -no-cups -no-nis -qt-gfx-linuxfb -qt-gfx-vnc -no-gfx-qvfb -no-mouse-qvfb -no-kbd-qvfb"

# configure 生成 default.pri 后，手工剔除 qvfb/infrared 模块（否则会拉入不兼容代码）
perl -ni -e 'print unless /enable_qvfb|enable_infrared/' sdk/configs/default.pri
perl -pi -e 's/(QTOPIA_MODULES=.*?) infrared /$1 /' sdk/configs/default.pri

./bin/qbuild -j8
./bin/qbuild image
cp -a image/. install/
```

完整 configure 参数与已知坑见 `docs/BUILD.md`。

## 5. 运行

`qpe` 内部写死了 prefix 路径检查，必须从 `install/` 运行。用 `scripts/run-qpe.sh` 一键启动（含全部环境变量与 `setsid` 脱离终端）：

```bash
# 把启动脚本放到构建根的 scripts/ 下，或直接从本仓库调用
cp <本仓库>/scripts/run-qpe.sh <构建根>/
cd <构建根>
setsid ./run-qpe.sh &
```

脚本按自身位置解析构建根（`install/` 所在目录），不硬编码绝对路径，换机器也能用。

关键环境变量（脚本已包含）：

```bash
QPEDIR=$PWD/install
HOME=$PWD/qpe-home
LD_LIBRARY_PATH=$PWD/install/lib:$PWD/install/qt_plugins/gfxdrivers
QT_PLUGIN_PATH=$PWD/install/qt_plugins
QWS_DISPLAY='VNC:0'
QWS_SIZE=640x480        # 必须设置，否则 hextile 坐标越界
```

`QWS_SIZE` 必须与 viewer 期望的桌面尺寸一致，否则 viewer 报 `Rect too big`。

**重要：部署时 `install/lib/*.so.4` 是 `qbuild image` 用 `install -c` 复制的独立文件，不是符号链接。** 若你更新了某个 `.so.4.4.3`，必须同步更新 `.so.4`，否则 QPE 仍加载旧库：

```bash
rm -f install/lib/libQtGui.so.4
ln -s libQtGui.so.4.4.3 install/lib/libQtGui.so.4
```

用 `grep libQtGui /proc/<qpe-pid>/maps` 确认实际加载的库。

重启前清理残留 System V 共享内存（否则报 `QSharedMemory::create: already exists`）：

```bash
for id in $(ipcs -m | awk '$3=="LoongBox"{print $2}'); do ipcrm -m $id; done
```

## 6. VNC 客户端（重要）

**不要用系统自带的 TigerVNC 1.16.2 `vncviewer`**：它默认请求大量扩展编码（Tight/H.264/伪编码），与 Qt 4.4 的 RFB 3.3 服务端协商后状态错乱，报 `Unknown message type 120` 且无法交互。

本项目提供一个基于 `gtk-vnc`（独立 RFB 实现，系统已装）的轻量 viewer，画面与交互均验证正常：

```bash
python3 tools/gtk-vnc-viewer.py 127.0.0.1 5900
```

依赖：Python `gi` + `GtkVnc 2.0` typelib（`gtk-vnc` 包提供）。

`tools/xclick.py` 用 XTEST 注入鼠标点击（无 xdotool 时可用）：

```bash
python3 tools/xclick.py 790 570 1
```

## 7. 回归测试

`tests/vnc_pixel_conv/`（源码在 `tools/vnc_pixel_conv_test.*`）是针对 loongarch64 VNC 16→32 像素转换的回归测试，覆盖偶数/奇数像素数、缓冲区越界 canary、逐通道颜色校验，并在编译期断言 loongarch64 快速转换路径已启用。

```bash
cd tools
QTTREE=<构建根>/qt-extended-4.4.3/qtopiacore/qt \
  <构建根>/qtopiacore/target/bin/qmake vnc_pixel_conv_test.pro \
  -spec $QTTREE/mkspecs/qws/linux-loongarch64-g++
make
LD_LIBRARY_PATH=<构建根>/sdk/qtopiacore/target/lib ./vnc_pixel_conv_test
# 期望: All VNC pixel conversion checks passed on LoongArch
```

这个测试在开发中实际抓出两个真实 bug：GCC 定义的是 `__loongarch__`（不是 `__loongarch64__`），以及奇数像素时尾部像素写到错误偏移导致越界。

## 8. 关键问题与根因

本项目排障中定位并修复的问题（细节见 `docs/TROUBLESHOOTING.md`）：

1. **qbuild `waitUntilDone()` 卡死**：generic 原子操作用全局 `pthread_mutex_t`，多线程 fork 时子进程继承已锁定的 mutex 死锁 → 改为 GCC `__sync_*` 无锁实现。
2. **GCC 16 破坏 Qt 4.4 的 `Q_FOREACH`**：statement-expression 里的 `break` 跳出外层循环，foreach 只迭代 1 次 → 用 brk 计数版宏替换。
3. **VNC 16→32 像素转换越界**：`__loongarch64__` 宏名错误 + 奇数像素尾部偏移错误 → 修复并加回归测试。
4. **`HANDLE` 截断**：`qvaluespace.h` 只对 `QT_ARCH_X86_64` 定义 64 位 handle → 加上 `QT_ARCH_LOONGARCH64`，解决 QPE 启动即段错误。
5. **VNC 协议读取用 partial `read()`**：TCP 分片导致像素格式读到垃圾值 → 统一替换为 `readFully()`。
6. **libQtSvg 在 `-O2` 下段错误**：Qt 4.4 老代码在 GCC 16 优化下的未定义行为 → host mkspec 强制 `-O0`。
7. **TigerVNC 1.16.2 viewer 不兼容**：换用 gtk-vnc viewer。

## 9. 目录结构

```
.
├── patches/            # 4 个按主题分组的补丁（可直接 patch -p1）
├── scripts/
│   ├── gen-patches.sh  # 从 pristine 源码重新生成补丁
│   └── run-qpe.sh      # 一键启动 QPE
├── tools/
│   ├── gtk-vnc-viewer.py       # 可用的 VNC viewer
│   ├── xclick.py               # XTEST 点击注入
│   └── vnc_pixel_conv_test.*   # 像素转换回归测试
├── docs/
│   ├── BUILD.md            # 完整构建参数与注意事项
│   ├── DOWNLOAD.md         # 并行分块下载方法
│   └── TROUBLESHOOTING.md  # 排错记录与根因
└── README.md
```

## 10. 许可

Qt Extended 4.4.3 源码为 GPLv2/GPLv3。本项目只包含**针对龙芯 loongarch64 的适配补丁与文档**，不含 Qt Extended 源码本身。使用源码请遵守原许可证。
