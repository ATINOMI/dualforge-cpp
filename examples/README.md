# Python 示例的 C++ 对应版本

四个程序沿用原 `examples/*.py` 的手柄操作方式，使用 DualForge 库在主线程处理输入和计时。构建时默认启用 `DUALFORGE_BUILD_EXAMPLES`。

| 原示例 | C++ 源文件 | 可执行文件 |
|---|---|---|
| `test_led.py` | [test_led.cpp](test_led.cpp) | `dualforge_test_led` |
| `test_trigger_effects.py` | [test_trigger_effects.cpp](test_trigger_effects.cpp) | `dualforge_test_trigger_effects` |
| `bow_simulation.py` | [bow_simulation.cpp](bow_simulation.cpp) | `dualforge_bow_simulation` |
| `touchpad.py` | [touchpad.cpp](touchpad.cpp) | `dualforge_touchpad` |

从独立项目根目录构建：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
```

以下命令假设位于构建目录；Windows Visual Studio 生成器的程序位于 `Release/`，文件名以 `.exe` 结尾。

```bash
./dualforge_test_led
./dualforge_test_trigger_effects
./dualforge_bow_simulation
./dualforge_touchpad --dry-run
```

所有示例支持 `--help`、`--device ID` 和 `--seconds 1..3600`。不指定时长则持续运行；默认选择第一个能打开的 USB DualSense。PS 键或 Ctrl+C 退出，弓弦还支持 OPTIONS 退出。退出、拔出或异常时尽力关闭 LED、震动和扳机；鼠标示例同时释放自己按住的鼠标键。强制终止进程不能保证清理。

`--help` 不初始化设备、不发送手柄或鼠标输出。`--dry-run` 是触摸板程序专有选项：仍连接手柄并亮起青色灯，但只打印鼠标动作，不操作桌面。设置有限运行时间可用于启动与退出检查：

```bash
./dualforge_test_led --seconds 10
./dualforge_test_trigger_effects --device 1 --seconds 30
./dualforge_touchpad --dry-run --seconds 10
```

## 灯光测试

- OPTIONS 打开/关闭菜单；方向键上下选择模式；○ 确认；PS 退出。
- 预设模式：上下切换原版的全部 33 个颜色预设。
- 自定义模式：上下选 R/G/B，左右微调 ±1，右摇杆持续调节，× 应用。连续调节固定为 20ms 一次，避免速度随输入频率改变。
- 彩虹模式：上下选择慢/中/快；进入菜单暂停彩虹，关闭菜单后恢复。彩虹在主线程定时更新，不创建后台写设备线程。

## 扳机效果测试

OPTIONS 进入/退出设置；L1/R1 选择左/右扳机；方向键上下循环原版的 21 项效果，左右分别记忆当前选择。启动时两侧效果关闭，设置模式显示蓝灯，正常模式显示绿灯。

Bow / Machine / Galloping 为实验固件效果。`choppy` 使用 C++ 库已记录的修正编码，见 [协议差异](../docs/protocol.md)，不承诺与旧 Python 触感相同。

## 弓弦模拟

连接后仅设置一次右扳机 Bow（起点 0、终点 8、阻力/回弹均为 6）。拉弓深度控制橙色亮度；累计扳机释放量决定离弦震动强度，左侧为右侧的一半，持续 150ms。震动期间暂停灯光更新，结束后恢复。

归一化输入按原版 `20/255` 开始拉弓、`8/255` 判定松开；震动强度换算为 C++ 的 `0..65535`。计时使用单调时钟，即使暂时没有新输入事件也能停止震动。常量可在源文件开头调整。

## 触摸板鼠标

- 单指滑动移动鼠标，轻触左键单击，连续两次轻触形成双击。
- 双指稳定 150ms 后上下滑动滚动；短时间、低位移的双指轻触为右键单击。
- 实体键：单指/无指按住左键进行拖动，松开实体键释放；双指按下实体键为右键单击。
- PS / Ctrl+C 退出。可以单独使用第二个触点槽位，不要求手指总在槽位 0。

保留旧版 1920×1080 触摸坐标尺度、移动倍率 1、滚动倍率 0.8、轻触 300ms/40 像素阈值。修正两处交互：第二次轻触只追加一次点击；双指结束后必须全部抬起才恢复单指识别，避免额外单击或鼠标跳跃。实体键点击不再同时触发轻触点击。

鼠标注入放在示例层，不改变公共控制器库：

| 平台 | 接口 | 运行条件 |
|---|---|---|
| Linux | [uinput](https://docs.kernel.org/input/uinput.html) | 当前用户能写 `/dev/uinput`；适用于由系统识别虚拟鼠标的 X11/Wayland 会话 |
| Windows | [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput) | 受 Windows 输入权限/UIPI 限制 |
| macOS | [CoreGraphics CGEvent](https://developer.apple.com/documentation/coregraphics/cgevent/post%28tap%3A%29?language=objc) | 系统辅助功能授权，使用 ApplicationServices 框架 |

程序不修改系统权限。Linux 无 uinput 访问权限或 macOS 未授权时会明确报错，可先用 `--dry-run`。如果桌面已经把 DualSense 的原生触摸板作为鼠标处理，需在系统设备设置中避免原生指针和此示例重复输出；dry-run 只保证本程序不注入鼠标事件。

自动化测试覆盖手势逻辑和退出时释放，不触发桌面输入。Windows/macOS 原生后端尚未在当前 Linux 环境中编译或实测；具体交互、滚动方向/速度和触感仍需人工验收。
