#pragma once
#include <dualforge/dualforge.hpp>
#include <thread>
namespace dualforge::detail {
using Report = std::array<std::uint8_t,47>;
class Backend {
public:
    virtual ~Backend()=default;
    virtual DeviceInfo info() const=0;
    virtual bool connected() const=0;
    virtual InputState read()=0;
    virtual Capabilities capabilities() const=0;
    virtual Status send(const Report&)=0;
    virtual Status led(std::uint8_t,std::uint8_t,std::uint8_t)=0;
    virtual Status rumble(std::uint16_t,std::uint16_t,std::uint32_t)=0;
    virtual bool improved_rumble() const { return false; }
};
struct ControllerAccess {
    static std::shared_ptr<Controller> make(std::unique_ptr<Backend> b) {
        return std::shared_ptr<Controller>(new Controller(std::move(b)));
    }
    static Status refresh(Controller& c,bool input=true) { return c.refresh(input); }
};
struct Runtime;
std::unique_ptr<Backend> open_sdl(std::uint32_t id,std::shared_ptr<Runtime> runtime,Status& status);
Status sdl_error(const char* operation);
Report trigger_report(TriggerTarget target,const TriggerEffect& effect);
Report player_report(std::uint8_t mask);
Report brightness_report(LightBrightness value);
Report animation_report(LightFadeAnimation value);
}
