#include "common.hpp"
#include "led_presets.hpp"
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <cmath>
using namespace dualforge;
namespace {
std::array<int,3> rainbow(double hue) {
    hue-=std::floor(hue);const int i=int(hue*6);const double f=hue*6-i;
    const int up=int(f*255),down=int((1-f)*255);
    switch(i%6) {
    case 0:return {255,up,0};case 1:return {down,255,0};case 2:return {0,255,up};
    case 3:return {0,down,255};case 4:return {up,0,255};default:return {255,0,down};
    }
}
}
int main(int argc,char** argv) try {
    const auto options=demo::parse(argc,argv);
    if(options.help) {demo::usage(argv[0],"灯光测试：OPTIONS 菜单；方向键上下选择；○ 确认。\n自定义：上下选 RGB，左右 ±1，右摇杆持续调节，× 应用。\n彩虹：上下选择慢/中/快。");return 0;}
    demo::Session session(options);auto& ds=*session.controller;
    enum Mode { Standby=-1,Preset,Custom,Rainbow };
    int mode=Standby,menu_index=0,preset=0,channel=0,speed=1;
    bool menu=false;Buttons previous;std::array<int,3> rgb{{128,128,128}};
    constexpr std::array<double,3> intervals{{0.05,0.02,0.005}};
    constexpr std::array<const char*,3> modes{{"预设模式","自定义模式","彩虹模式"}};
    constexpr std::array<const char*,3> speeds{{"慢","中","快"}};
    double hue=0,next_rainbow=0,next_adjust=0;
    auto apply=[&](std::array<int,3> c){demo::require(ds.set_led(c[0],c[1],c[2]));};
    auto display=[&] {
        std::cout<<"\nLED 测试 | OPTIONS 菜单 | PS 退出\n";
        if(menu) {
            for(int i=0;i<3;++i)std::cout<<(menu_index==i?"> ":"  ")<<modes[i]<<'\n';
            std::cout<<"上下选择，○ 确认\n";
        } else if(mode==Preset) {
            std::cout<<"【预设模式】上下切换\n";
            for(std::size_t i=0;i<demo::color_presets.size();++i)std::cout<<(preset==int(i)?"> ":"  ")<<demo::color_presets[i].name<<'\n';
        } else if(mode==Custom) {
            std::cout<<"【自定义模式】上下选通道；左右 ±1；右摇杆连续调节；× 应用\n";
            for(int i=0;i<3;++i)std::cout<<(channel==i?"> ":"  ")<<"RGB"[i]<<" = "<<rgb[i]<<'\n';
        } else if(mode==Rainbow)std::cout<<"【彩虹模式】上下切换速度，当前："<<speeds[speed]<<'\n';
        else std::cout<<"待机（绿色），OPTIONS 打开菜单\n";
    };
    apply({0,255,0});display();
    while(session.next()) {
        const auto state=ds.state();const auto b=state.buttons;const auto now=session.now();bool changed=false;
        const int delta=int(demo::rising(b.down,previous.down))-int(demo::rising(b.up,previous.up));
        if(demo::rising(b.options,previous.options)) {menu=!menu;changed=true;next_rainbow=now;}
        else if(menu) {
            if(delta){menu_index=demo::wrap(menu_index+delta,3);changed=true;}
            if(demo::rising(b.circle,previous.circle)) {
                mode=menu_index;menu=false;changed=true;next_rainbow=now;
                if(mode==Preset)apply(demo::color_presets[preset].rgb);
            }
        } else if(mode==Preset && delta) {
            preset=demo::wrap(preset+delta,int(demo::color_presets.size()));apply(demo::color_presets[preset].rgb);changed=true;
        } else if(mode==Custom) {
            if(delta){channel=demo::wrap(channel+delta,3);changed=true;}
            int step=int(demo::rising(b.right,previous.right))-int(demo::rising(b.left,previous.left));
            const float axis=state.right_stick.x;constexpr float dead_zone=20.0f/127;
            // 固定 20ms 调节间隔，避免速度随 poll 频率改变。
            if(std::abs(axis)>dead_zone && now>=next_adjust) {
                const int amount=int((std::abs(axis)-dead_zone)/(1-dead_zone)*5)+1;
                step+=axis>0?amount:-amount;next_adjust=now+0.02;
            }
            if(step){const int value=std::clamp(rgb[channel]+step,0,255);changed|=value!=rgb[channel];rgb[channel]=value;}
            if(demo::rising(b.cross,previous.cross)){apply(rgb);changed=true;std::cout<<"已应用 RGB\n";}
        } else if(mode==Rainbow && delta) {speed=demo::wrap(speed+delta,3);changed=true;}
        if(!menu && mode==Rainbow && now>=next_rainbow) {
            apply(rainbow(hue));hue=std::fmod(hue+0.005,1.0);next_rainbow=now+intervals[speed];
        }
        if(changed) display();
        previous=b;
        session.sleep();
    }
    std::cout<<"退出\n";
} catch(const std::exception& e){std::cerr<<"LED 示例: "<<e.what()<<'\n';return 1;}
