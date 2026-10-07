#pragma once
#include <dualforge/dualforge.hpp>
#include <SDL3/SDL.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace demo {
inline volatile std::sig_atomic_t interrupted=0;
inline void signal_handler(int) { interrupted=1; }
inline void require(dualforge::Status status) {
    if(!status) throw std::runtime_error(status.message);
}
struct Options { std::uint32_t device=0; int seconds=0; bool help=false, dry_run=false; };
inline Options parse(int argc,char** argv,bool allow_dry_run=false) {
    Options result;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--help") { result.help=true; continue; }
        if(arg=="--dry-run" && allow_dry_run) { result.dry_run=true; continue; }
        if(arg!="--device" && arg!="--seconds") throw std::invalid_argument("Unknown option: "+arg);
        if(++i>=argc) throw std::invalid_argument("Missing value for "+arg);
        const std::string value=argv[i];std::size_t used=0;
        const auto n=std::stoull(value,&used);
        if(used!=value.size() || value.empty() || value[0]=='-' || n==0) throw std::invalid_argument("Expected positive integer for "+arg);
        if(arg=="--device") {
            if(n>std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("Device ID too large");
            result.device=static_cast<std::uint32_t>(n);
        } else {
            if(n>3600) throw std::invalid_argument("--seconds must be 1..3600");
            result.seconds=static_cast<int>(n);
        }
    }
    return result;
}
inline void usage(const char* name,const char* description,bool touchpad=false) {
    std::cout<<description<<"\n\n"<<name<<" [--device ID] [--seconds 1..3600]"<<(touchpad?" [--dry-run]":"")
        <<"\n不指定时长则持续运行；PS 键或 Ctrl+C 退出。\n";
}
inline int wrap(int value,int count) { return (value%count+count)%count; }
inline bool rising(bool value,bool previous) { return value && !previous; }
// 主线程循环：即使手柄暂时没有新事件，也继续处理彩虹和震动计时。
class Session {
    Options options_;
    std::chrono::steady_clock::time_point start_=std::chrono::steady_clock::now();
public:
    dualforge::Context context;
    std::shared_ptr<dualforge::Controller> controller;
    explicit Session(Options options):options_(options) {
        std::signal(SIGINT,signal_handler);std::signal(SIGTERM,signal_handler);
        require(context.status());require(context.poll());
        if(options.device) {
            auto opened=context.open(options.device);require(opened.status);controller=std::move(opened.controller);
        } else {
            auto devices=context.enumerate();
            dualforge::Status last{dualforge::ErrorCode::NotConnected,"No USB DualSense found"};
            for(const auto& device:devices) {
                auto opened=context.open(device.id);
                if(opened.status) { controller=std::move(opened.controller);break; }
                last=opened.status;
            }
            if(!controller) require(last);
        }
        std::cout<<"已连接: "<<controller->device_info().name<<" (ID "<<controller->device_info().id<<")\n";
    }
    ~Session() {
        if(controller) {
            // 失败也继续释放句柄；析构不向外抛异常。
            try {
                if(controller->is_connected()) (void)controller->set_led(0,0,0);
                (void)controller->close();
            } catch(...) {}
        }
    }
    double now() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now()-start_).count();
    }
    bool next() {
        if(interrupted || (options_.seconds && now()>=options_.seconds)) return false;
        require(context.poll());
        if(!controller->is_connected()) { std::cout<<"设备已断开\n";return false; }
        if(controller->state().buttons.home) return false;
        return true;
    }
    void sleep() const { SDL_Delay(4); }
};
}
