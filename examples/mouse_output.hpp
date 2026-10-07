#pragma once
#include "touchpad_gestures.hpp"
#include <memory>
namespace demo {
// 原生桌面鼠标输出仅属于示例，不进入 DualForge 公共库。
class MouseOutput {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit MouseOutput(bool dry_run);
    ~MouseOutput();
    MouseOutput(const MouseOutput&)=delete;
    MouseOutput& operator=(const MouseOutput&)=delete;
    void send(const MouseAction& action);
};
}
