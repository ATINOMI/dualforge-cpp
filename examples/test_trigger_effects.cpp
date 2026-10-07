#include "common.hpp"
#include <SDL3/SDL_main.h>
#include <array>

using namespace dualforge;
struct EffectChoice { const char* name; TriggerEffect effect; };
int main(int argc,char** argv) try {
    const auto options=demo::parse(argc,argv);
    if(options.help) { demo::usage(argv[0],"扳机效果测试：OPTIONS 设置模式；L1/R1 选择左/右扳机；方向键上下切换。\nBow/Machine/Galloping 为实验固件效果。");return 0; }
    using namespace effects;
    const std::array<EffectChoice,21> choices{{
        {"off",off()},{"normal",normal()},{"very_soft",very_soft()},{"soft",soft()},
        {"medium",medium()},{"hard",hard()},{"very_hard",very_hard()},{"hardest",hardest()},
        {"rigid",rigid()},{"feedback(3,5)",feedback(3,5)},{"weapon(2,6,8)",weapon(2,6,8)},
        {"game_cube",game_cube()},{"semi_automatic_gun",semi_automatic_gun()},{"choppy",choppy()},
        {"bow(0,8,7,6) [实验]",bow(0,8,7,6)},{"vibration(0,5,20)",vibration(0,5,20)},
        {"automatic_gun",automatic_gun()},{"vibrate_trigger(10)",vibrate_trigger(10)},
        {"vibrate_trigger_pulse",vibrate_trigger_pulse()},
        {"machine(1,9,5,3,10,1) [实验]",machine(1,9,5,3,10,1)},
        {"galloping(0,9,2,5,10) [实验]",galloping(0,9,2,5,10)}
    }};
    demo::Session session(options);auto& ds=*session.controller;
    demo::require(ds.set_trigger_effect(TriggerTarget::Both,off()));
    bool setting=false;TriggerTarget active=TriggerTarget::Right;int left=0,right=0;Buttons previous;
    auto display=[&] {
        demo::require(ds.set_led(0,setting?100:255,setting?255:0));
        std::cout<<"\n扳机效果测试 | OPTIONS 设置 | L1/R1 选侧 | 上下切换 | PS 退出\n"
          <<(setting?"【设置模式】":"【正常模式】")
          <<"\n左 "<<(active==TriggerTarget::Left?"> ":"  ")<<'['<<left+1<<"/21] "<<choices[left].name
          <<"\n右 "<<(active==TriggerTarget::Right?"> ":"  ")<<'['<<right+1<<"/21] "<<choices[right].name<<'\n';
    };
    display();
    while(session.next()) {
        const auto b=ds.state().buttons;bool changed=false;
        if(demo::rising(b.options,previous.options)) {setting=!setting;changed=true;}
        if(setting) {
            if(demo::rising(b.l1,previous.l1)) {active=TriggerTarget::Left;changed=true;}
            if(demo::rising(b.r1,previous.r1)) {active=TriggerTarget::Right;changed=true;}
            const int delta=int(demo::rising(b.down,previous.down))-int(demo::rising(b.up,previous.up));
            if(delta) {
                auto& index=active==TriggerTarget::Left?left:right;index=demo::wrap(index+delta,21);
                demo::require(ds.set_trigger_effect(active,choices[index].effect));changed=true;
            }
        }
        if(changed) display();
        previous=b;
        session.sleep();
    }
    demo::require(ds.set_trigger_effect(TriggerTarget::Both,off()));
    std::cout<<"退出\n";
} catch(const std::exception& e) {std::cerr<<"扳机示例: "<<e.what()<<'\n';return 1;}
