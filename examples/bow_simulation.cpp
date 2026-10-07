#include "common.hpp"
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>

using namespace dualforge;
int main(int argc,char** argv) try {
    const auto options=demo::parse(argc,argv);
    if(options.help) {demo::usage(argv[0],"弓弦模拟：右扳机拉弓，松开时短震；OPTIONS / PS 退出。Bow 为实验固件效果。");return 0;}
    constexpr int strength=6,snap_force=6,start_pos=0,end_pos=8;
    constexpr float draw_threshold=20.0f/255,release_threshold=8.0f/255;
    constexpr double impact_duration=0.15;
    demo::Session session(options);auto& ds=*session.controller;
    demo::require(ds.set_trigger_effect(TriggerTarget::Right,effects::bow(start_pos,end_pos,strength,snap_force)));
    std::cout<<"弓弦模拟启动，阻力=6 回弹=6；OPTIONS / PS / Ctrl+C 退出\n";
    bool drawing=false,impact=false;float previous=0,peak=0,release_integral=0;double impact_end=0;
    std::array<int,3> last_color{{-1,-1,-1}};
    auto led=[&](std::array<int,3> color) {
        if(color!=last_color) {demo::require(ds.set_led(color[0],color[1],color[2]));last_color=color;}
    };
    while(session.next()) {
        const auto s=ds.state();if(s.buttons.options)break;
        const auto now=session.now();const float trigger=s.right_trigger;
        if(impact && now>=impact_end) {demo::require(ds.stop_rumble());impact=false;std::cout<<"[弓弦] 待机\n";}
        if(!impact) led({int(255*trigger),int(80*trigger),0});
        if(!drawing && trigger>draw_threshold) {
            drawing=true;peak=trigger;release_integral=0;std::cout<<"[弓弦] 开始拉弓\n";
        }
        if(drawing) {
            peak=std::max(peak,trigger);
            if(trigger<previous) release_integral+=previous-trigger;
            if(trigger<=release_threshold && !impact) {
                drawing=false;const int rumble=int(std::clamp(release_integral,0.0f,1.0f)*65535);
                std::cout<<"[弓弦] 离弦！峰值="<<int(peak*255)<<" 积分="<<release_integral*255<<" 震动="<<rumble<<'\n';
                peak=0;release_integral=0;
                if(rumble) {
                    led({0,0,0});demo::require(ds.set_rumble(rumble/2,rumble,150));
                    impact=true;impact_end=now+impact_duration;
                }
            }
        }
        previous=trigger;session.sleep();
    }
    std::cout<<"退出\n";
} catch(const std::exception& e) {std::cerr<<"弓弦示例: "<<e.what()<<'\n';return 1;}
