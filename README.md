# DualForge C++

基于 **C++17 + SDL 3.4.18** 的 DualSense USB 控制库。独立 CMake 项目，可从仓库根目录直接构建，不依赖 Python 项目。

[English quick start](README_EN.md) · [原示例的 C++ 版本](examples/README.md) · [协议与迁移说明](docs/protocol.md) · [验收记录](docs/validation.md)

## 功能与边界

- 普通 Sony DualSense（VID `054c` / PID `0ce6`），设备枚举、按 ID 选择、多设备、断开检测与重新打开。
- 按键、方向键、摇杆、模拟扳机、电池、双指触摸板、陀螺仪和加速度计。
- RGB、普通震动、玩家灯掩码、亮度、淡入淡出及自适应扳机。
- 七个底层效果工厂、全部旧版预设名称及限定类型的自定义效果。Bow / Galloping / Machine 属于实验固件效果。
- 固件详情、MAC、原始校准、音频、蓝牙、Edge 专属按键与 Python 绑定不在首版范围内。

三平台构建工作流已提供，真实硬件支持以验收记录为准。没有测量或宣称比 Python 版更低的延迟。

## 构建

需要 CMake 3.24+、C++17 编译器和平台 SDK。默认下载固定 SDL 源码并校验 SHA256，无需先安装 SDL。Ubuntu 可以使用 GCC，Windows 使用 Visual Studio 2022，macOS 使用 Xcode Command Line Tools。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

离线构建：先解压官方 `SDL3-3.4.18.tar.gz`，然后增加：

```bash
cmake -S . -B build -DDUALFORGE_SDL_SOURCE_DIR=/absolute/path/to/SDL3-3.4.18
```

选项：`DUALFORGE_BUILD_TESTS`、`DUALFORGE_BUILD_EXAMPLES`，默认开启；`DUALFORGE_SANITIZERS=ON` 开启 GCC/Clang 的 ASan/UBSan。库为静态库，SDL 为共享库；Windows 示例和测试自动复制 SDL DLL 到可执行文件旁。

Linux 要使用原生 HIDAPI 功能，需要当前用户有对应 `/dev/hidraw*` 的访问权限。可参考 SDL 源码 `src/joystick/hidapi/README.txt` 中的 udev 说明。权限不足时可能只能读取 evdev 输入，专有输出会明确返回 `Unsupported`；库不会修改系统权限。

## 命令行示例

以下命令在构建目录运行；Visual Studio 多配置生成器的程序位于 `Release/`。`list` 只列设备；其余命令打开手柄，灯光/震动/扳机命令会产生相应输出。

```bash
./dualforge_cli list
./dualforge_cli monitor 10
./dualforge_cli led 5
./dualforge_cli rumble 3
./dualforge_cli trigger feedback 3
./dualforge_cli trigger                  # 从列出的效果名称交互选择
./dualforge_cli bow 10                  # 实验弓弦效果 + 按压程度颜色显示
./dualforge_cli --device 1 monitor 5
```

持续时间为 1..300 秒，默认 5 秒（trigger 默认 3 秒）。Ctrl+C 退出；正常退出或异常展开时停止震动及两侧扳机。断开后旧对象保持关闭，重新枚举并打开新 ID，不自动恢复旧输出。灯光测试正常结束时关闭 RGB；通用析构保留灯光设置。

四个原 Python 示例另有独立 C++ 程序，保留手柄菜单和交互：

```bash
./dualforge_test_led
./dualforge_test_trigger_effects
./dualforge_bow_simulation
./dualforge_touchpad --dry-run
```

详见 [示例说明](examples/README.md)。触摸板原生鼠标输出属于示例层，需要平台权限；`--dry-run` 仅打印动作。全部示例支持 `--help`、`--device ID` 和 `--seconds N`。

## 库调用

```cpp
#include <dualforge/dualforge.hpp>
#include <SDL3/SDL.h>
#include <iostream>

int main() {
    dualforge::Context context;
    if (!context.status()) return 1;
    context.poll();
    const auto devices = context.enumerate();
    if (devices.empty()) return 1;
    auto opened = context.open(devices.front().id);
    if (!opened.status) {
        std::cerr << opened.status.message << '\n';
        return 1;
    }
    auto controller = opened.controller;
    controller->on_input([](const dualforge::InputState& state) {
        std::cout << state.right_trigger << '\n';
    });
    auto result = controller->set_trigger_effect(
        dualforge::TriggerTarget::Right, dualforge::effects::feedback(3, 5));
    if (!result) std::cerr << result.message << '\n';
    for (int i = 0; i < 250 && controller->is_connected(); ++i) {
        if (!context.poll()) return 1;
        SDL_Delay(4);
    }
    return controller->close() ? 0 : 1;
}
```

所有操作和析构在主线程调用。`Context` 持有 SDL 子系统引用；控制器可以比 Context 活得更久，但不再自动接收事件，需要主动关闭。不会调用全局 `SDL_Quit()`。同一设备不可在多个 Context 重复打开。

输入回调每轮刷新最多一次，仅有相关输入事件时通知，不保证每个 USB 报告都触发。状态回调在断开或主动关闭时通知一次；注册时不立即发送初始状态，请先查询 `is_connected()`。允许在输入回调内设置输出、替换回调或调用 `close()`；关闭延迟到回调结束后执行。禁止递归 `poll()` / `refresh()`。

`Status` 的 bool 转换表示成功；包含 `ErrorCode` 和说明。效果工厂非法参数抛出 `std::invalid_argument`。回调异常转为失败 Status，由应用处理，不静默吞掉。设备状态快照缺失字段保留默认值，能力以 `capabilities()` 判断。

### 已有 SDL 应用

宿主选择 `EventMode::External`，保持原有 SDL 事件循环：

```cpp
dualforge::Context context(dualforge::EventMode::External);
SDL_Event event;
while (SDL_PollEvent(&event)) {
    auto status = context.handle_event(event); // 不消费或泵送事件
    // 宿主照常处理窗口、键盘和其他事件。
}
auto status = context.refresh();
```

独立模式调用 `poll(other_event_handler)` 时，非本 Context 所持有手柄的事件会交给传入函数。应用应只有一个事件队列拥有者；多个 Context 时由宿主向各 External Context 分发事件，避免彼此消费事件。

### 数值与单位

- 摇杆 `[-1,1]`，Y 正方向向下；模拟扳机 `[0,1]`，不额外制造 L2/R2 数字按钮。
- 两指坐标 `[0,1]`，IMU 使用 SDL 坐标系，角速度 rad/s，加速度 m/s²。SDL 负责校准，不提供旧版原始值。
- 电池未知为 `-1`，状态为 `Unknown`。设备枚举时 USB 类型未知，打开后查看 `device_info().usb`。
- 亮度与动画 setter 会一起发送最近配置，避免 SDL 队列合并时丢字段；改变亮度可能重触发已配置动画，可先设 `Nothing`。
- RGB `0..255`，玩家灯掩码 `0..31`，震动 `0..65535` 并显式提供毫秒持续时间；非法值返回错误，不隐式裁剪。
- 专有功能仅对确认的普通 DualSense + HIDAPI + USB 后端开放。`Unverified` 表示允许尝试但无法运行时确认物理表现；发送成功不等于效果已被感知验证。

## 安装与链接

```bash
cmake --install build --config Release --prefix /your/prefix
cmake -S tests/consumer -B consumer-build -DCMAKE_PREFIX_PATH=/your/prefix
cmake --build consumer-build --config Release
```

库、头文件、CMake 包与 SDL 依赖一同安装。消费方：

```cmake
find_package(DualForge 0.1 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE DualForge::DualForge)
```

运行安装后的程序时需确保 SDL 共享库可发现（Windows 放置 DLL，Linux 配置运行路径或 `LD_LIBRARY_PATH`，macOS 配置加载路径）。

## 测试与发布

CTest 覆盖协议向量、参数边界、模拟生命周期、SDL 虚拟设备、触摸板手势转换和四个独立示例的帮助入口。测试不向物理手柄发送效果。专用报告和实际触感仍需硬件验收。

可选 `-DDUALFORGE_BUILD_HARDWARE_CHECK=ON` 构建 `dualforge_hardware_check`，仅手动运行。它会对首个 USB DualSense 发送短时、低强度灯光/震动/正式扳机命令，打印 SDL 版本与固件字段并清理输出；不加入 CTest，也不能代替人工观察。

`.github/workflows/ci.yml` 包含三平台构建、CTest、安装消费和 Linux sanitizer 作业。仓库根目录已放置该工作流，推送后可由 GitHub Actions 自动运行。

MIT 许可；效果编码保留 John “Nielk1” Klein 的 MIT 声明。详见 `THIRD_PARTY_NOTICES.md`。
