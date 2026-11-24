# 完整构建指南

## 前置条件

- loongarch64 机器（实测龙芯 3B6000M），AOSC OS，GCC 16
- 依赖（AOSC 均为系统自带）：`pkg-config`、`libx11`、`libfontconfig`、`libfreetype2`、`zlib`、`libpng`、`libjpeg`、`sqlite3`、`glib-2.0`
- Python 3（用于 VNC viewer 与回归测试）

源码与补丁准备见根目录 README 第 2、3 节。

## configure 参数逐项说明

```bash
echo yes | $QTOPIA_DEPOT_PATH/configure \
  -force-build-qt -build-qt \
  -arch loongarch64 -xplatform linux-loongarch64-g++ \
  -ui-type home -release -no-error \
  -prefix $PWD/install \
  -no-dbus -no-dbusipc -no-ssl -no-sxe -no-phonon -no-v4l2 -no-vpn -no-libamr \
  -remove-module bluetooth,drm,infrared \
  -extra-qt-config "-qt-zlib -qt-libpng -qt-libjpeg -qt-freetype -no-webkit -no-phonon -no-dbus -no-cups -no-nis -no-qt3support" \
  -extra-qt-embedded-config "-qt-zlib -qt-libpng -qt-libjpeg -qt-freetype -no-webkit -no-phonon -no-dbus -no-cups -no-nis -qt-gfx-linuxfb -qt-gfx-vnc -no-gfx-qvfb -no-mouse-qvfb -no-kbd-qvfb"
```

| 参数 | 作用 |
|------|------|
| `-force-build-qt -build-qt` | AOSC 无 Qt4 开发包，从内置源码自举 host Qt 与 target Qt/E |
| `-arch loongarch64` | 目标 CPU 家族（对应 `src/corelib/arch/loongarch64/` 目录） |
| `-xplatform linux-loongarch64-g++` | **只写 bare 名**——configure 会自己拼 `mkspecs/qws/`；写成 `qws/linux-loongarch64-g++` 会变双层导致失败 |
| `-ui-type home` | Home UI（桌面风格） |
| `-no-error` | 不把警告当错误（GCC 16 产生大量新警告） |
| `-prefix $PWD/install` | 运行期安装路径，QPE 二进制内嵌该路径检查 |
| `-no-dbus -no-ssl -no-phonon ...` | 禁用非必需组件，减少老代码在 GCC 16 下的编译风险 |
| `-remove-module bluetooth,drm,infrared` | 移除不兼容模块（infrared 依赖缺失的 `linux/irda.h`） |
| `-extra-qt-config ...` | host Qt 配置：用内置 zlib/png/jpeg/freetype，关掉 webkit |
| `-extra-qt-embedded-config ...` | target Qt/E：启用 linuxfb 与 vnc 显示驱动 |

## configure 后的手工步骤

configure 会重新生成 `sdk/configs/default.pri`，其中可能包含 `enable_qvfb` / `enable_infrared`，需手工剔除（命令行 `-remove-module` 对这两项不生效）：

```bash
F=sdk/configs/default.pri
perl -ni -e 'print unless /enable_qvfb|enable_infrared/' $F
perl -pi -e 's/(QTOPIA_MODULES=.*?) infrared /$1 /' $F
```

## 编译

```bash
./bin/qbuild -j8
./bin/qbuild image
cp -a image/. install/
```

注意：
- `qbuild` 依赖图庞大，完整构建较慢，但补丁修复后不会死锁。
- `qbuild image` 可能有一个 GSM 链接错误 `undefined reference to GSM_ADD_inline`（`src/3rdparty/libraries/gsm`），不影响 QPE 运行与 image 产物（已生成的 image 仍可用）。

## 重新生成补丁

如果你在修改版源码上又改了东西，可从 pristine 源码重新生成补丁：

```bash
# 准备 pristine 源码
mkdir -p /tmp/pristine && tar -xzf qt-extended-opensource-src-4.4.3.tar.gz -C /tmp/pristine
# 生成
scripts/gen-patches.sh /tmp/pristine <含修改版 qt-extended-4.4.3 的目录> patches/
# 验证（在源码根用 -p1）
cd /tmp/pristine/qt-extended-4.4.3
for p in /path/to/patches/*.patch; do patch -p1 --dry-run --batch < "$p"; done
```

## GCC 16 与 -O0

host mkspec（`mkspecs/linux-g++/qmake.conf`）与 target mkspec（`mkspecs/qws/linux-loongarch64-g++/qmake.conf`）都强制了 `QMAKE_CFLAGS_RELEASE = -O0` / `QMAKE_CXXFLAGS_RELEASE = -O0`。原因：Qt 4.4.3 的若干老代码在 GCC 16 `-O2` 下有未定义行为（实测 `libQtSvg::QSvgRenderer::load` 段错误、`ApplicationLayer::remHandle` 段错误）。host 工具是构建辅助，target 速度对本测试无影响，`-O0` 换取稳定性是正确取舍。

## 输出产物

- `src/server/qpe`：QPE 可执行文件（约 3.3 MB）
- `image/`：运行镜像（bin/lib/etc/pics/plugins 等，约 65 MB）
- `install/`：从 image 复制的运行根（QPE 必须从此处运行）
