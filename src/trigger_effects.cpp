// Effect encoding adapted from John "Nielk1" Klein's MIT-licensed generators.
// See THIRD_PARTY_NOTICES.md for the full notice and source provenance.
#include <dualforge/trigger_effects.hpp>
#include <algorithm>
#include <stdexcept>

namespace dualforge::effects {
namespace {
void range(int value, int low, int high, const char* name) {
    if (value < low || value > high) throw std::invalid_argument(name);
}
TriggerEffect zones(EffectType type, int start, int end) {
    TriggerEffect r{};
    r[0] = static_cast<std::uint8_t>(type);
    const auto mask = (1u << start) | (1u << end);
    r[1] = static_cast<std::uint8_t>(mask);
    r[2] = static_cast<std::uint8_t>(mask >> 8);
    return r;
}
TriggerEffect patterned(EffectType type, int position, int strength) {
    range(position, 0, 9, "position must be 0..9");
    range(strength, 1, 8, "strength/amplitude must be 1..8");
    TriggerEffect r{};
    r[0] = static_cast<std::uint8_t>(type);
    std::uint32_t forces = 0;
    unsigned active = 0;
    for (int i = position; i < 10; ++i) {
        active |= 1u << i;
        forces |= static_cast<std::uint32_t>(strength - 1) << (3 * i);
    }
    r[1] = static_cast<std::uint8_t>(active);
    r[2] = static_cast<std::uint8_t>(active >> 8);
    for (int i = 0; i < 4; ++i) r[3+i] = static_cast<std::uint8_t>(forces >> (8*i));
    return r;
}
}
bool valid_type(std::uint8_t t) noexcept {
    switch (static_cast<EffectType>(t)) {
    case EffectType::Off: case EffectType::Feedback: case EffectType::Weapon:
    case EffectType::Vibration: case EffectType::Bow: case EffectType::Galloping:
    case EffectType::Machine: return true;
    }
    return false;
}
TriggerEffect off() { TriggerEffect r{}; r[0] = 0x05; return r; }
TriggerEffect feedback(int p, int s) { return patterned(EffectType::Feedback, p, s); }
TriggerEffect weapon(int start, int end, int strength) {
    range(start, 2, 7, "weapon start must be 2..7");
    range(end, start+1, 8, "weapon end must be start+1..8");
    range(strength, 1, 8, "weapon strength must be 1..8");
    auto r = zones(EffectType::Weapon, start, end); r[3] = static_cast<std::uint8_t>(strength-1); return r;
}
TriggerEffect vibration(int p, int a, int f) {
    range(f, 1, 255, "frequency must be 1..255");
    auto r = patterned(EffectType::Vibration, p, a); r[9] = static_cast<std::uint8_t>(f); return r;
}
TriggerEffect bow(int start, int end, int strength, int snap) {
    range(start, 0, 7, "bow start must be 0..7");
    range(end, start+1, 8, "bow end must be start+1..8");
    range(strength, 1, 8, "bow strength must be 1..8");
    range(snap, 1, 8, "snap force must be 1..8");
    auto r = zones(EffectType::Bow, start, end);
    r[3] = static_cast<std::uint8_t>((strength-1) | ((snap-1) << 3)); return r;
}
TriggerEffect machine(int start, int end, int a, int b, int f, int period) {
    range(start, 1, 8, "machine start must be 1..8");
    range(end, start+1, 9, "machine end must be start+1..9");
    range(a, 0, 7, "amplitude A must be 0..7"); range(b, 0, 7, "amplitude B must be 0..7");
    range(f, 1, 255, "frequency must be 1..255"); range(period, 0, 255, "period must be 0..255");
    auto r = zones(EffectType::Machine, start, end);
    r[3] = static_cast<std::uint8_t>(a | (b << 3)); r[4] = static_cast<std::uint8_t>(f); r[5] = static_cast<std::uint8_t>(period); return r;
}
TriggerEffect galloping(int start, int end, int first, int second, int f) {
    range(start, 0, 8, "galloping start must be 0..8"); range(end, start+1, 9, "galloping end must be start+1..9");
    range(first, 0, 6, "first foot must be 0..6"); range(second, first+1, 7, "second foot must be first+1..7");
    range(f, 1, 255, "frequency must be 1..255");
    auto r = zones(EffectType::Galloping, start, end); r[3] = static_cast<std::uint8_t>(second | (first << 3)); r[4] = static_cast<std::uint8_t>(f); return r;
}
TriggerEffect normal() { return off(); }
TriggerEffect very_soft() { return feedback(0,1); }
TriggerEffect soft() { return feedback(0,2); }
TriggerEffect medium() { return feedback(0,4); }
TriggerEffect hard() { return feedback(0,6); }
TriggerEffect very_hard() { return feedback(0,7); }
TriggerEffect hardest() { return feedback(0,8); }
TriggerEffect rigid() { return hardest(); }
TriggerEffect game_cube() { return weapon(4,7,8); }
TriggerEffect semi_automatic_gun(int a,int b,int c) { return weapon(a,b,c); }
TriggerEffect choppy() {
    // Deliberately replace the legacy 0x2702 mask (bits outside ten zones).
    // Explicitly enable zones 1,5,6,9 at forces 4,1,1,5 (encoded 3,0,0,4).
    TriggerEffect r{}; r[0]=0x21; r[1]=0x62; r[2]=0x02;
    const std::uint32_t force = (3u << 3) | (4u << 27);
    for(int i=0;i<4;++i) r[3+i]=static_cast<std::uint8_t>(force >> (8*i));
    return r;
}
TriggerEffect automatic_gun(int p,int a,int f) { return vibration(p,a,f); }
TriggerEffect vibrate_trigger(int f) { return vibration(0,8,f); }
TriggerEffect vibrate_trigger_pulse() { return vibration(0,8,5); }
TriggerEffect vibrate_trigger_custom(int p,int a,int f) { return vibration(p,a,f); }
TriggerEffect custom(EffectType type, const std::array<std::uint8_t,10>& params) {
    const auto t=static_cast<std::uint8_t>(type);
    if(!valid_type(t)) throw std::invalid_argument("unknown/debug effect type");
    TriggerEffect r{}; r[0]=t; std::copy(params.begin(),params.end(),r.begin()+1); return r;
}
}
