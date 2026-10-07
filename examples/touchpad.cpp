#include "common.hpp"
#include "mouse_output.hpp"
#include <SDL3/SDL_main.h>
using namespace dualforge;
int main(int argc,char** argv) try {
    const auto options=demo::parse(argc,argv,true);
    if(options.help) {
        demo::usage(argv[0],"触摸板鼠标：单指移动/轻触/双击，双指滚动/轻触右键，实体键单指拖动/双指右键。\nLinux 使用 uinput；Windows 使用 SendInput；macOS 使用 CoreGraphics。\n--dry-run 只打印鼠标动作，不操作桌面。",true);return 0;
    }
    demo::Session session(options);auto& ds=*session.controller;
    if(ds.capabilities().touchpad==Availability::Unavailable)throw std::runtime_error("Touchpad unavailable on this backend");
    demo::MouseOutput mouse(options.dry_run);demo::TouchpadGestures gestures;
    demo::require(ds.set_led(0,200,255));
    std::cout<<"触摸板示例已启动"<<(options.dry_run?"（dry-run，不操作鼠标）":"（原生鼠标输出）")<<"；PS / Ctrl+C 退出\n";
    while(session.next()) {
        for(const auto& action:gestures.update(ds.state(),session.now()))mouse.send(action);
        session.sleep();
    }
    for(const auto& action:gestures.release())mouse.send(action);
    std::cout<<"退出\n";
} catch(const std::exception& e){std::cerr<<"触摸板示例: "<<e.what()<<'\n';return 1;}
