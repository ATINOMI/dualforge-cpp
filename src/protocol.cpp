#include "backend.hpp"
#include <algorithm>
namespace dualforge::detail {
Report trigger_report(TriggerTarget target,const TriggerEffect& e) {
    Report r{};
    if(target==TriggerTarget::Right || target==TriggerTarget::Both) {
        r[0]|=0x04; std::copy(e.begin(),e.end(),r.begin()+10);
    }
    if(target==TriggerTarget::Left || target==TriggerTarget::Both) {
        r[0]|=0x08; std::copy(e.begin(),e.end(),r.begin()+21);
    }
    return r;
}
Report player_report(std::uint8_t mask) { Report r{}; r[1]=0x10; r[43]=mask; return r; }
Report brightness_report(LightBrightness v) { Report r{}; r[38]=0x01; r[42]=static_cast<std::uint8_t>(v); return r; }
Report animation_report(LightFadeAnimation v) { Report r{}; r[38]=0x02; r[41]=static_cast<std::uint8_t>(v); return r; }
}
