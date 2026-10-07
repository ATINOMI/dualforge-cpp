#include <dualforge/dualforge.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

using namespace dualforge;
namespace {
volatile std::sig_atomic_t interrupted=0;
void signal_handler(int) { interrupted=1; }
void require(Status s) { if(!s) throw std::runtime_error(s.message); }
int number(const char* text) {
    std::size_t used=0;const std::string value(text);int n=std::stoi(value,&used);
    if(used!=value.size()) throw std::invalid_argument("invalid number: "+value);
    return n;
}
void run(Context& ctx,const std::shared_ptr<Controller>& c,int seconds) {
    if(seconds<1 || seconds>300) throw std::invalid_argument("duration must be 1..300 seconds");
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    while(!interrupted && c->is_connected() && std::chrono::steady_clock::now()<until) {
        require(ctx.poll()); SDL_Delay(4);
    }
    if(!c->is_connected()) throw std::runtime_error("controller disconnected");
}
std::map<std::string,TriggerEffect> presets() {
    using namespace effects;
    return {{"off",off()},{"feedback",feedback(3,5)},{"weapon",weapon(2,6,8)},
      {"vibration",vibration(0,5,20)},{"bow",bow(0,7,7,6)},
      {"machine",machine(1,9,5,3,10,1)},{"galloping",galloping(0,9,2,5,10)},
      {"normal",normal()},{"very_soft",very_soft()},{"soft",soft()},{"medium",medium()},
      {"hard",hard()},{"very_hard",very_hard()},{"hardest",hardest()},{"rigid",rigid()},
      {"game_cube",game_cube()},{"semi_automatic_gun",semi_automatic_gun()},
      {"choppy",choppy()},{"automatic_gun",automatic_gun()},
      {"vibrate_trigger",vibrate_trigger()},{"vibrate_trigger_pulse",vibrate_trigger_pulse()},
      {"vibrate_trigger_custom",vibrate_trigger_custom(0,5,20)}};
}
}
int main(int argc,char** argv) try {
    std::signal(SIGINT,signal_handler);std::signal(SIGTERM,signal_handler);
    if(argc<2 || std::string(argv[1])=="--help") {
        std::cout<<"dualforge_cli list\n"
          "dualforge_cli [--device ID] monitor [seconds]\n"
          "dualforge_cli [--device ID] led [seconds]\n"
          "dualforge_cli [--device ID] rumble [seconds]\n"
          "dualforge_cli [--device ID] trigger [name [seconds]]\n"
          "dualforge_cli [--device ID] bow [seconds]\n"
          "Default device: first ordinary DualSense; USB only. Ctrl+C exits.\n";
        return 0;
    }
    int arg=1;std::uint32_t selected=0;
    if(std::string(argv[arg])=="--device") {
        if(argc<4) throw std::invalid_argument("--device needs ID and command");
        const int id=number(argv[++arg]);if(id<1) throw std::invalid_argument("ID must be positive");
        selected=static_cast<std::uint32_t>(id);++arg;
    }
    const std::string command=argv[arg++];
    if(command!="list" && command!="monitor" && command!="led" && command!="rumble" && command!="trigger" && command!="bow")
        throw std::invalid_argument("unknown command");
    Context ctx; require(ctx.status()); require(ctx.poll());
    const auto devices=ctx.enumerate();
    if(command=="list") {
        for(const auto& d:devices) std::cout<<d.id<<"\t"<<d.name<<"\t"<<d.path<<'\n';
        if(devices.empty()) std::cout<<"No ordinary DualSense detected\n";
        return 0;
    }
    if(devices.empty()) throw std::runtime_error("No DualSense found. Check USB and HID permissions.");
    auto opened=ctx.open(selected?selected:devices.front().id); require(opened.status);auto c=opened.controller;
    c->on_state([](bool connected){if(!connected) std::cout<<"Disconnected\n";});
    if(command=="trigger") {
        const auto choices=presets();std::string name;
        if(arg<argc) name=argv[arg++];
        else {
            for(auto& item:choices) std::cout<<item.first<<' ';
            std::cout<<"\nEffect name: "<<std::flush;
            if(!(std::cin>>name)) throw std::invalid_argument("no effect selected");
        }
        const auto it=choices.find(name);if(it==choices.end()) throw std::invalid_argument("unknown effect name");
        const int seconds=arg<argc?number(argv[arg]):3;
        require(c->set_trigger_effect(TriggerTarget::Both,it->second));run(ctx,c,seconds);
    } else {
        const int seconds=arg<argc?number(argv[arg]):5;
        if(command=="monitor") {
            auto next=std::chrono::steady_clock::now();
            c->on_input([&](const InputState& s){
                const auto now=std::chrono::steady_clock::now();if(now<next)return;next=now+std::chrono::milliseconds(100);
                std::cout<<std::fixed<<std::setprecision(2)<<"L("<<s.left_stick.x<<','<<s.left_stick.y<<") R("<<s.right_stick.x<<','<<s.right_stick.y
                  <<") triggers "<<s.left_trigger<<','<<s.right_trigger<<" cross="<<s.buttons.cross
                  <<" touch="<<s.fingers[0].touching<<','<<s.fingers[1].touching
                  <<" gyro="<<s.gyro.x<<','<<s.gyro.y<<','<<s.gyro.z
                  <<" accel="<<s.accelerometer.x<<','<<s.accelerometer.y<<','<<s.accelerometer.z
                  <<" battery="<<s.battery_percent<<"%\n";
            });
        } else if(command=="led") {
            require(c->set_led(0,0,255));require(c->set_player_lights(4));require(c->set_light_brightness(LightBrightness::Mid));
        } else if(command=="rumble") {
            if(seconds<1 || seconds>300) throw std::invalid_argument("duration must be 1..300 seconds");
            require(c->set_rumble(20000,20000,static_cast<std::uint32_t>(seconds)*1000));
        } else if(command=="bow") {
            require(c->set_trigger_effect(TriggerTarget::Right,effects::bow(0,7,7,6)));
            int previous=-1;
            c->on_input([&](const InputState& s){
                const int color=static_cast<int>(s.right_trigger*255);
                if(color!=previous) { require(c->set_led(color,0,255-color));previous=color; }
            });
        }
        run(ctx,c,seconds);
    }
    if(command=="led" || command=="bow") require(c->set_led(0,0,0));
    require(c->close());return 0;
} catch(const std::exception& e) {std::cerr<<"DualForge: "<<e.what()<<'\n';return 1;}
