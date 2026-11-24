# 排错记录与根因

本文件记录移植过程中定位到的关键问题。每个问题都经过实测验证（gdb / strace / core dump / 协议级客户端），不是猜测。

## 1. qbuild 卡死在 `RuleEngineScheduler::waitUntilDone()`

**现象**：`qbuild` 编译一段时间后停滞，日志不再增长，所有 worker 线程空闲，主循环卡在 `QCoreApplication::exec()`。

**根因链**（gdb + `/proc/<pid>/fd` 证据）：
1. loongarch64 走 generic atomics，而 `qatomic_generic_unix.cpp` 用**一个全局 `pthread_mutex_t qAtomicMutex`** 模拟所有原子操作。
2. qbuild 多线程，fork 执行规则命令时，若另一线程正持有这把全局锁，子进程继承已锁定的 mutex。
3. 子进程在 `process.cpp:92` 的 `cwd.toAscii()` → `QBasicAtomicInt_fetchAndAddOrdered` 永久死锁，**还没来得及 exec**，却持有从父进程继承的 stdout/stderr 管道写端。
4. 父进程对应 worker 卡在 `ShellProcess` 的 `select()` 等 EOF，永远等不到 → 不调用 `threadIdle()` → `idleThreads.count()` 凑不齐 → `waitUntilDone()` 事件循环永不退出。

**验证**：卡住的命令本身是瞬间完成的 `rm -f .../libemailcomposer.*`，与命令无关；`/proc` 显示主进程 worker 读的管道写端被僵死子进程持有。

**修复**：`0001-loongarch64-arch.patch` 把 6 个原子函数改为 GCC `__sync_*` builtins（loongarch64 上是 LL/SC 无锁指令），移除全局 mutex。

## 2. GCC 16 破坏 Qt 4.4 的 `Q_FOREACH`

**现象**：qbuild 完全失效（规则只处理一次），QString 相关操作异常。

**根因**：Qt 4.4 的 `Q_FOREACH` 内层循环用 GNU statement-expression `({--brk; break;})`；GCC 16 下其中的 `break` 会跳出外层循环，导致 foreach 只迭代一次。

**验证**：独立小程序用 `foreach` 遍历 `QList<QByteArray>` 只输出 1 项。

**修复**：`0002-gcc16-core.patch` 用 brk 计数版宏替换（无 statement expression），并保留所有语义（break/continue 行为一致）。

## 3. VNC 16→32 像素转换越界

**现象**：VNC 客户端连接后 QPE 段错误，core 显示 `QVNCServer::convertPixels → memmove_lsx` 越界。

**根因**（两层）：
1. 宏名错误：GCC 在龙芯上定义的是 `__loongarch__` / `__loongarch64`（无尾部下划线），**没有** `__loongarch64__`；最初的修复条件用了错误宏名，实际从未生效（回归测试的编译期 `#error` 抓出）。
2. 真正的越界：16→32 优化循环写奇数 count 的尾部像素时用已 `++` 推进的 `dst32[count-1]`，写到缓冲区外。光标宽度常为奇数，故客户端一连就崩。

**修复**：`0003-vnc-backend.patch` 用正确的 `__loongarch__` 条件，提取 `qt_vnc_conv16To32()` helper 并修复尾部偏移；`tools/vnc_pixel_conv_test.*` 是对应的回归测试（开发中实际抓出这两个 bug）。

## 4. QPE 启动即段错误：`ApplicationLayer::remHandle()`

**现象**：QPE 启动即崩，栈在 `libqtopiabase` 的 `ApplicationLayer::remHandle()`。

**根因**：`qvaluespace.h` 只对 `QT_ARCH_X86_64` 定义 64 位 `HANDLE`（`unsigned long`），loongarch64 落到 32 位 `unsigned int` 分支；代码把 64 位 `ReadHandle*` 指针转成 32 位 HANDLE 再转回，指针被截断。

**修复**：`0004-gcc16-apps.patch` 把条件改为 `#if defined(QT_ARCH_X86_64) || defined(QT_ARCH_LOONGARCH64)`。全树仅此一处 `QT_ARCH_X86_64` 引用。

## 5. VNC 协议 partial read 导致像素格式垃圾

**现象**：gdb 显示 `pixelFormat.bitsPerPixel = 1953591405`（垃圾值），传入 `convertPixels` 越界。

**根因**：Qt 4.4 的 VNC 代码用 `QTcpSocket::read()` 并假设一次读满固定长度；TCP 分片时只读到部分字节，剩余是栈垃圾；调用方还不检查返回值。多次连接后拿到垃圾 bpp。

**修复**：`0003-vnc-backend.patch` 加 `readFully()` helper（先 memset buf，循环 `waitForReadyRead` 读满）替换所有固定长度读取；`QRfbPixelFormat` 加构造函数初始化；`setPixelFormat()` 校验 bpp 只许 8/16/32。

## 6. libQtSvg 在 `-O2` 下段错误

**现象**：`svgtopicture` 一启动就崩（`QSvgRenderer::load`），导致 `qbuild image` 在 `install_qpepics` 失败。

**根因**：Qt 4.4 老代码在 GCC 16 `-O2` 下有未定义行为（`-O0` 正常）。

**修复**：host mkspec 强制 `-O0`。host 工具是构建辅助，速度无关紧要。

## 7. hextile 编码器产生损坏画面

**现象**：viewer 报 `Rect too big: ... exceeds 640x480` / `Hextile out of bounds`，只解码 1 个 tile。

**根因**：Qt 4.4 的 `QRfbHextileEncoder<quint16>` 在 loongarch64 上生成的 tile 数据错位（客户端请求的编码列表解析也出现垃圾值如 `0x57455448`="WHET"）。

**修复**：`0003-vnc-backend.patch` 在 `setEncodings()` 的 `case Hextile` 跳过 hextile 编码器创建，回落到 Raw。Raw 编码实测完整传输 640×480（307.2 kpixels）。

## 8. `Unknown message type 120` 与无法交互

**最终结论**：**server 无需再改，问题在 TigerVNC 1.16.2 viewer 端**。

用严格按 RFB 协议实现的 Python 客户端验证：
- 握手、ServerInit（640×480, 16bpp）、请求、收到完整帧（614400 字节）全部正确；
- 读完整帧后静默 2 秒，server 发 0 字节（没有主动推送垃圾——之前看到的"垃圾帧"是测试脚本读帧长度错误的假象）；
- 发鼠标点击后 server 正确返回增量帧 `(80,0,32,32)`，交互正常。

TigerVNC 1.16.2 默认请求大量扩展编码（Tight/H.264/伪编码），与 Qt 4.4 的 RFB 3.3 服务端协商后 viewer 状态错乱。AOSC 仓库里两个 tigervnc 包都是 1.16.2，没有真正的旧版。

**解决**：用 `tools/gtk-vnc-viewer.py`（基于系统已装的 `gtk-vnc`，独立 RFB 实现），画面与交互均验证正常。

## 9. 部署陷阱

- **`install/lib/*.so.4` 不是符号链接**：`qbuild image` 用 `install -c` 复制成独立文件。更新 `.so.4.4.3` 后必须同步更新 `.so.4`，否则 QPE 仍加载旧库（表现为修复"无效"）。用 `grep libQtGui /proc/<pid>/maps` 确认。
- **System V 共享内存残留**：`kill -9 qpe` 后会残留（可达 40+ 个段），导致新 QPE 报 `QSharedMemory::create: already exists`。重启前用 `ipcrm -m` 清理。
- **必须设 `QWS_SIZE=640x480`**：否则 QWS 默认屏幕尺寸很大，hextile 坐标越界。
- **清理残留进程用 `pgrep -x qbuild.bin`**：`pkill -f 'sdk/bin/qbuild.bin'` 会匹配到自己 shell 的命令行导致自杀。
