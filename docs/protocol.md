# 协议依据与迁移差异

读取日期：2026-10-07。

- 社区报告表：https://controllers.fandom.com/wiki/Sony_DualSense/Report_Summaries
- 社区数据结构与效果生成器：https://controllers.fandom.com/wiki/Sony_DualSense/Data_Structures
- 效果生成器原作者：https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db
- 固定 SDL 后端：https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/joystick/hidapi/SDL_hidapi_ps5.c
- SDL 后端 GUID 标识：https://github.com/libsdl-org/SDL/blob/release-3.4.18/src/joystick/SDL_joystick.c

社区 Wiki 包含逆向观察、未确认字段和推测，不是 Sony 的公开协议保证。这里只实现需要的已知字段，不照搬整份位域结构体。效果生成器代码有独立 MIT 许可，见第三方声明。

## 发送边界

USB 原始输出为 `0x02` Report ID + 47 字节状态载荷。`SDL_SendGamepadEffect` 接收后者；SDL 负责 HID 写入和外层封装。蓝牙封装在不同 SDL 版本间存在实现差异，首版拒绝已知无线连接，不自行实现蓝牙 CRC。

| 载荷偏移 | 含义 |
|---|---|
| 0 | 右扳机允许位 0x04、左扳机允许位 0x08；震动 emulation 标志 |
| 1 | RGB 允许位 0x04、玩家灯允许位 0x10 |
| 2 / 3 | 右 / 左普通震动强度 |
| 10..20 / 21..31 | 右 / 左 11 字节扳机效果 |
| 38 | 亮度允许位 0x01、动画允许位 0x02、增强震动位 0x04 |
| 41 / 42 / 43 | 动画、亮度、玩家灯掩码 |
| 44..46 | RGB |

RGB / 普通震动走 SDL 标准函数。专有效果载荷只启用目标字段；存在普通震动时同时保留相同的 emulation 参数。对固件 >= 0x0224 使用 SDL 相同的增强震动编码，否则按 SDL 的旧震动尺度处理。超时仍由 SDL 管理。

SDL 的待发送效果队列只比较前两个允许字节，不比较字节 38；连续调用亮度和动画可能被合并。因此库在这两种 setter 中一起发送最近配置的亮度与动画，确保合并不丢字段。先设动画后改变亮度可能再次触发该动画；需要避免时先把动画设为 Nothing。

USB 灯光初始化由 SDL 的正常初始化与 RGB API 处理，不额外写 `ResetLights`，不固定 sleep 500ms。Wiki 描述的动画等待逻辑主要针对无线，不能当作 USB 必需步骤。

为避免把原始 PS5 包发送给 evdev、虚拟手柄或其他设备，专有输出要求 VID/PID 为 054c/0ce6、连接为 wired，并使用固定 SDL 版本的 GUID 驱动标记 `data[14]=='h'`。这不是稳定的公开后端名接口，因此 SDL 升级必须重新核对并运行协议/硬件验收。其他后端允许标准输入，专有输出返回 Unsupported。

## 效果

正式认可模式：Off 0x05、Feedback 0x21、Weapon 0x25、Vibration 0x26。
实验模式：Bow 0x22、Galloping 0x23、Machine 0x27。它们依赖固件，不保证后续版本继续支持。

工厂函数严格校验参数，零强度不隐式等同 off，调用者显式使用 `off()`；Machine 保持旧库 start >= 1 的限制，补齐 period 0..255 校验。自定义工厂仅接受上述七种模式和固定十字节参数，不开放 Wiki 中的遗留、调试或校准模式。

`choppy()` 修正旧掩码 `0x2702`：它包含十区域之外的位，且与旧注释声明的区域不一致。C++ 版明确启用区域 1、5、6、9，掩码 `0x0262`；保留旧力度字在这些有效区域上的编码 3、0、0、4（力度 4、1、1、5），其他区域清零，得到力度字 `0x20000018`。这是有意的行为差异，不声称与旧版触感相同，须实测。

`normal()` 为 off；`rigid()` 与 `hardest()` 编码相同。预设名称数量不代表硬件底层模式数量。

## 输入迁移

SDL 负责输入解析和 IMU 校准。C++ 输入采用归一化坐标和 SDL 坐标系，不暴露原始字节、扳机状态半字节或传感器原始校准报告。L2/R2 以模拟轴呈现；触摸板鼠标模拟需要操作系统注入接口，不属于公共控制器库；独立 `dualforge_touchpad` 示例在示例层提供三平台鼠标注入及 dry-run。原 CLI 的 `bow` 命令仅控制手柄和显示拉弓颜色；独立 `dualforge_bow_simulation` 还提供原 Python 示例的离弦短震，不注入鼠标。
