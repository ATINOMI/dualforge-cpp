// Manual test only: sends brief light/motor commands to the first USB DualSense.
// Not registered with CTest. Physical feedback still requires a human observer.
#include <dualforge/dualforge.hpp>
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>
using namespace dualforge;
void check(Status s,const char* name) {
    if(!s) throw std::runtime_error(std::string(name)+": "+s.message);
    std::cout<<name<<": accepted\n";
}
int main() try {
    Context context;check(context.status(),"context");check(context.poll(),"poll");
    auto devices=context.enumerate();if(devices.empty())throw std::runtime_error("No DualSense");
    auto opened=context.open(devices.front().id);check(opened.status,"open");auto c=opened.controller;
    auto* pad=SDL_GetGamepadFromID(c->device_info().id);
    std::cout<<"SDL="<<SDL_GetVersion()<<" VID="<<std::hex<<c->device_info().vendor<<" PID="<<c->device_info().product
      <<" firmware=0x"<<SDL_GetGamepadFirmwareVersion(pad)<<std::dec<<" USB="<<c->device_info().usb<<'\n';
    const auto caps=c->capabilities();
    std::cout<<"caps rgb="<<int(caps.rgb)<<" rumble="<<int(caps.rumble)<<" touch="<<int(caps.touchpad)
      <<" gyro="<<int(caps.gyro)<<" accel="<<int(caps.accelerometer)<<" triggers="<<int(caps.trigger_effects)<<'\n';
    auto wait=[&]{for(int i=0;i<25;++i){auto s=context.poll();if(!s)throw std::runtime_error(s.message);SDL_Delay(4);}};
    check(c->set_led(0,0,64),"RGB");
    check(c->set_player_lights(4),"player lights");
    check(c->set_light_brightness(LightBrightness::Mid),"brightness");
    check(c->set_light_fade_animation(LightFadeAnimation::Nothing),"animation nothing");
    check(c->set_rumble(10000,10000,300),"timed rumble");
    check(c->set_trigger_effect(TriggerTarget::Both,effects::feedback(3,2)),"both feedback + active rumble");
    wait();check(c->set_trigger_effect(TriggerTarget::Right,effects::weapon(2,6,2)),"right weapon");
    wait();check(c->set_trigger_effect(TriggerTarget::Left,effects::vibration(5,2,10)),"left vibration");
    wait();check(c->set_trigger_effect(TriggerTarget::Both,effects::off()),"both off");
    check(c->stop_rumble(),"stop rumble");check(c->set_led(0,0,0),"RGB off");
    check(c->set_player_lights(0),"player lights off");
    check(c->set_light_brightness(LightBrightness::Bright),"restore brightness");
    check(c->close(),"close");
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
