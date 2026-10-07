#include "check.hpp"
#include "backend.hpp"
#include <algorithm>
using namespace dualforge;
int main() try {
    using namespace effects;
    CHECK(off()==TriggerEffect({5,0,0,0,0,0,0,0,0,0,0}));
    CHECK(feedback(0,8)==TriggerEffect({0x21,0xff,3,0xff,0xff,0xff,0x3f,0,0,0,0}));
    CHECK(feedback(9,1)==TriggerEffect({0x21,0,2,0,0,0,0,0,0,0,0}));
    CHECK(weapon(2,6,8)==TriggerEffect({0x25,0x44,0,7,0,0,0,0,0,0,0}));
    CHECK(vibration(0,8,20)==TriggerEffect({0x26,0xff,3,0xff,0xff,0xff,0x3f,0,0,20,0}));
    CHECK(bow(0,7,7,6)==TriggerEffect({0x22,0x81,0,46,0,0,0,0,0,0,0}));
    CHECK(machine(1,9,5,3,10,1)==TriggerEffect({0x27,2,2,29,10,1,0,0,0,0,0}));
    CHECK(galloping(0,9,2,5,10)==TriggerEffect({0x23,1,2,21,10,0,0,0,0,0,0}));
    CHECK(choppy()==TriggerEffect({0x21,0x62,2,0x18,0,0,0x20,0,0,0,0}));
    for(int p=0;p<10;++p) for(int strength=1;strength<=8;++strength) {
        const auto r=feedback(p,strength);
        const unsigned mask=r[1]|(unsigned(r[2])<<8);
        const std::uint32_t f=r[3]|(std::uint32_t(r[4])<<8)|(std::uint32_t(r[5])<<16)|(std::uint32_t(r[6])<<24);
        for(int i=0;i<10;++i) {
            CHECK(bool(mask&(1u<<i))==(i>=p));
            CHECK(((f>>(3*i))&7u)==unsigned(i>=p?strength-1:0));
        }
    }
    const TriggerEffect presets[]={normal(),very_soft(),soft(),medium(),hard(),very_hard(),hardest(),rigid(),game_cube(),semi_automatic_gun(),choppy(),automatic_gun(),vibrate_trigger(),vibrate_trigger_pulse(),vibrate_trigger_custom(9,1,255)};
    for(auto& e:presets) CHECK(valid_type(e[0]));
    CHECK(normal()==off()); CHECK(rigid()==hardest());
    CHECK(game_cube()==weapon(4,7,8)); CHECK(vibrate_trigger_pulse()==vibration(0,8,5));
    rejects([]{feedback(-1,1);}); rejects([]{feedback(10,1);}); rejects([]{feedback(0,0);}); rejects([]{feedback(0,9);});
    rejects([]{weapon(1,6,8);}); rejects([]{weapon(7,7,1);}); rejects([]{weapon(2,9,1);}); rejects([]{weapon(2,6,9);});
    rejects([]{vibration(0,0,1);}); rejects([]{vibration(0,1,0);}); rejects([]{vibration(0,1,256);});
    rejects([]{bow(8,8,1,1);}); rejects([]{bow(0,9,1,1);}); rejects([]{bow(0,8,0,1);}); rejects([]{bow(0,8,1,9);});
    rejects([]{machine(0,9,1,1,1,0);}); rejects([]{machine(1,9,8,1,1,0);}); rejects([]{machine(1,9,1,-1,1,0);});
    rejects([]{machine(1,9,1,1,0,0);}); rejects([]{machine(1,9,1,1,1,256);});
    rejects([]{galloping(0,9,6,6,1);}); rejects([]{galloping(0,10,0,1,1);}); rejects([]{galloping(0,9,-1,1,1);});
    rejects([]{custom(static_cast<EffectType>(0xfc),{});});
    CHECK(custom(EffectType::Off,{})==off());
    const auto e=weapon(2,6,8);
    auto right=detail::trigger_report(TriggerTarget::Right,e);
    CHECK(right.size()==47 && right[0]==4 && right[10]==0x25 && right[21]==0);
    auto left=detail::trigger_report(TriggerTarget::Left,e);
    CHECK(left[0]==8 && left[10]==0 && left[21]==0x25);
    auto both=detail::trigger_report(TriggerTarget::Both,e);
    CHECK(both[0]==12 && std::equal(e.begin(),e.end(),both.begin()+10) && std::equal(e.begin(),e.end(),both.begin()+21));
    auto players=detail::player_report(0x15); CHECK(players[1]==0x10 && players[43]==0x15 && players[0]==0);
    auto bright=detail::brightness_report(LightBrightness::Dim); CHECK(bright[38]==1 && bright[42]==2);
    auto anim=detail::animation_report(LightFadeAnimation::FadeIn); CHECK(anim[38]==2 && anim[41]==1);
    std::cout<<"Protocol vectors and parameter boundaries passed\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
