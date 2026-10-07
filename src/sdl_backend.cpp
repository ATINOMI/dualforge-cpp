#include "backend.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <exception>
#include <set>

namespace dualforge::detail {
Status sdl_error(const char* operation) { return {ErrorCode::SdlError,std::string(operation)+": "+SDL_GetError()}; }
struct Runtime {
    Status status;
    bool initialized=false;
    std::set<std::uint32_t> open_ids;
    Runtime() {
        if(!SDL_IsMainThread()) { status={ErrorCode::WrongThread,"Context must be created on the main thread"}; return; }
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5,"1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS,"1");
        initialized=SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if(!initialized) status=sdl_error("SDL_InitSubSystem");
    }
    ~Runtime() { if(initialized) SDL_QuitSubSystem(SDL_INIT_GAMEPAD); }
};
namespace {
std::string safe(const char* s) { return s?s:""; }
Availability available(bool value) { return value?Availability::Available:Availability::Unavailable; }
float stick(Sint16 v) { return v<0?float(v)/32768.0f:float(v)/32767.0f; }
class SdlBackend final:public Backend {
    std::shared_ptr<Runtime> runtime_;
    SDL_Gamepad* pad_;
    DeviceInfo info_;
    Capabilities caps_;
    bool improved_=false;
public:
    SdlBackend(SDL_Gamepad* pad,std::shared_ptr<Runtime> runtime):runtime_(std::move(runtime)),pad_(pad) {
        info_={SDL_GetGamepadID(pad),safe(SDL_GetGamepadName(pad)),safe(SDL_GetGamepadPath(pad)),
            SDL_GetGamepadVendor(pad),SDL_GetGamepadProduct(pad),
            SDL_GetGamepadConnectionState(pad)==SDL_JOYSTICK_CONNECTION_WIRED};
        const auto props=SDL_GetGamepadProperties(pad);
        caps_.rgb=available(SDL_GetBooleanProperty(props,SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN,false));
        caps_.rumble=available(SDL_GetBooleanProperty(props,SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN,false));
        caps_.touchpad=available(SDL_GetNumGamepadTouchpads(pad)>0);
        caps_.gyro=available(SDL_GamepadHasSensor(pad,SDL_SENSOR_GYRO) && SDL_SetGamepadSensorEnabled(pad,SDL_SENSOR_GYRO,true));
        caps_.accelerometer=available(SDL_GamepadHasSensor(pad,SDL_SENSOR_ACCEL) && SDL_SetGamepadSensorEnabled(pad,SDL_SENSOR_ACCEL,true));
        // This driver signature is verified against the pinned SDL source. No public
        // API exposes the backend name. Never send PS5 packets through evdev/virtual.
        const auto guid=SDL_GetJoystickGUID(SDL_GetGamepadJoystick(pad));
        const bool native_ps5=info_.vendor==0x054c && info_.product==0x0ce6 && guid.data[14]=='h' && info_.usb;
        caps_.trigger_effects=native_ps5?Availability::Unverified:Availability::Unavailable;
        caps_.light_extensions=native_ps5?Availability::Unverified:Availability::Unavailable;
        improved_=SDL_GetGamepadFirmwareVersion(pad)>=0x0224;
    }
    ~SdlBackend() override { runtime_->open_ids.erase(info_.id); SDL_CloseGamepad(pad_); }
    DeviceInfo info() const override { return info_; }
    bool connected() const override { return SDL_GamepadConnected(pad_); }
    Capabilities capabilities() const override { return caps_; }
    bool improved_rumble() const override { return improved_; }
    InputState read() override {
        InputState s;
        s.connected=connected(); if(!s.connected) return s;
        s.left_stick={stick(SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_LEFTX)),stick(SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_LEFTY))};
        s.right_stick={stick(SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_RIGHTX)),stick(SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_RIGHTY))};
        s.left_trigger=std::max(0.0f,float(SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_LEFT_TRIGGER))/32767.0f);
        s.right_trigger=std::max(0.0f,float(SDL_GetGamepadAxis(pad_,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))/32767.0f);
        auto button=[&](SDL_GamepadButton b){return SDL_GetGamepadButton(pad_,b);};
        auto& b=s.buttons;
        b.cross=button(SDL_GAMEPAD_BUTTON_SOUTH); b.circle=button(SDL_GAMEPAD_BUTTON_EAST);
        b.square=button(SDL_GAMEPAD_BUTTON_WEST); b.triangle=button(SDL_GAMEPAD_BUTTON_NORTH);
        b.l1=button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER); b.r1=button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        b.l3=button(SDL_GAMEPAD_BUTTON_LEFT_STICK); b.r3=button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        b.create=button(SDL_GAMEPAD_BUTTON_BACK); b.options=button(SDL_GAMEPAD_BUTTON_START);
        b.home=button(SDL_GAMEPAD_BUTTON_GUIDE); b.pad=button(SDL_GAMEPAD_BUTTON_TOUCHPAD); b.mute=button(SDL_GAMEPAD_BUTTON_MISC1);
        b.up=button(SDL_GAMEPAD_BUTTON_DPAD_UP); b.right=button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        b.down=button(SDL_GAMEPAD_BUTTON_DPAD_DOWN); b.left=button(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        if(caps_.touchpad==Availability::Available) {
            const int fingers=std::min(2,SDL_GetNumGamepadTouchpadFingers(pad_,0));
            for(int i=0;i<fingers;++i) {
                auto& f=s.fingers[static_cast<std::size_t>(i)];
                if(!SDL_GetGamepadTouchpadFinger(pad_,0,i,&f.touching,&f.x,&f.y,&f.pressure)) f={};
            }
        }
        float data[3]{};
        if(caps_.gyro==Availability::Available && SDL_GetGamepadSensorData(pad_,SDL_SENSOR_GYRO,data,3)) s.gyro={data[0],data[1],data[2]};
        if(caps_.accelerometer==Availability::Available && SDL_GetGamepadSensorData(pad_,SDL_SENSOR_ACCEL,data,3)) s.accelerometer={data[0],data[1],data[2]};
        int percent=-1;
        const auto power=SDL_GetGamepadPowerInfo(pad_,&percent);
        s.battery_percent=percent;
        switch(power) {
        case SDL_POWERSTATE_ON_BATTERY:s.power=InputState::Power::OnBattery;break;
        case SDL_POWERSTATE_CHARGING:s.power=InputState::Power::Charging;break;
        case SDL_POWERSTATE_CHARGED:s.power=InputState::Power::Charged;break;
        case SDL_POWERSTATE_NO_BATTERY:s.power=InputState::Power::NoBattery;break;
        default:break;
        }
        return s;
    }
    Status send(const Report& r) override {
        if(caps_.trigger_effects==Availability::Unavailable) return {ErrorCode::Unsupported,"PS5 HIDAPI USB backend required"};
        return SDL_SendGamepadEffect(pad_,r.data(),static_cast<int>(r.size()))?Status{}:sdl_error("SDL_SendGamepadEffect");
    }
    Status led(std::uint8_t r,std::uint8_t g,std::uint8_t b) override {
        return SDL_SetGamepadLED(pad_,r,g,b)?Status{}:sdl_error("SDL_SetGamepadLED");
    }
    Status rumble(std::uint16_t left,std::uint16_t right,std::uint32_t ms) override {
        return SDL_RumbleGamepad(pad_,left,right,ms)?Status{}:sdl_error("SDL_RumbleGamepad");
    }
};
}
std::unique_ptr<Backend> open_sdl(std::uint32_t id,std::shared_ptr<Runtime> runtime,Status& status) {
    if(runtime->open_ids.count(id)) { status={ErrorCode::AlreadyOpen,"Device already open in another Context"}; return {}; }
    auto* pad=SDL_OpenGamepad(id);
    if(!pad) { status=sdl_error("SDL_OpenGamepad"); return {}; }
    if(SDL_GetGamepadVendor(pad)!=0x054c || SDL_GetGamepadProduct(pad)!=0x0ce6) {
        SDL_CloseGamepad(pad); status={ErrorCode::Unsupported,"Only ordinary Sony DualSense is supported"}; return {};
    }
    if(SDL_GetGamepadConnectionState(pad)==SDL_JOYSTICK_CONNECTION_WIRELESS) {
        SDL_CloseGamepad(pad); status={ErrorCode::Unsupported,"v1 requires USB"}; return {};
    }
    auto backend=std::make_unique<SdlBackend>(pad,runtime);
    runtime->open_ids.insert(id);
    return backend;
}
}

namespace dualforge {
struct Context::Impl {
    std::shared_ptr<detail::Runtime> runtime;
    EventMode mode;
    std::thread::id thread=std::this_thread::get_id();
    std::vector<std::weak_ptr<Controller>> controllers;
    std::set<std::uint32_t> dirty;
    bool refreshing=false, polling=false;
    Status check() const {
        if(thread!=std::this_thread::get_id()) return {ErrorCode::WrongThread,"Context requires main thread"};
        return runtime->status;
    }
    void mark(const SDL_Event& event) {
        switch(event.type) {
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: dirty.insert(event.gaxis.which); break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: case SDL_EVENT_GAMEPAD_BUTTON_UP: dirty.insert(event.gbutton.which); break;
        case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN: case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION: case SDL_EVENT_GAMEPAD_TOUCHPAD_UP: dirty.insert(event.gtouchpad.which); break;
        case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: dirty.insert(event.gsensor.which); break;
        case SDL_EVENT_GAMEPAD_REMOVED: case SDL_EVENT_GAMEPAD_ADDED: case SDL_EVENT_GAMEPAD_REMAPPED: dirty.insert(event.gdevice.which); break;
        default:break;
        }
    }
};
Context::Context(EventMode mode):impl_(std::make_unique<Impl>()) {
    // A backend retains this runtime, so Controllers can safely outlive Context.
    static std::weak_ptr<detail::Runtime> weak_runtime;
    impl_->mode=mode;
    if(SDL_IsMainThread()) impl_->runtime=weak_runtime.lock();
    if(!impl_->runtime) {
        impl_->runtime=std::make_shared<detail::Runtime>();
        if(impl_->runtime->status) weak_runtime=impl_->runtime;
    }
}
Context::~Context()=default;
Status Context::status() const { return impl_->check(); }
std::vector<DeviceInfo> Context::enumerate() const {
    std::vector<DeviceInfo> result;
    if(!impl_->check()) return result;
    int count=0; auto* ids=SDL_GetGamepads(&count);
    for(int i=0;ids && i<count;++i) {
        const auto id=ids[i];
        if(SDL_GetGamepadVendorForID(id)!=0x054c || SDL_GetGamepadProductForID(id)!=0x0ce6) continue;
        const auto name=SDL_GetGamepadNameForID(id), path=SDL_GetGamepadPathForID(id);
        // Connection state is only available after open; USB is filled in Controller info.
        result.push_back({id,name?name:"",path?path:"",0x054c,0x0ce6,false});
    }
    SDL_free(ids); return result;
}
OpenResult Context::open(std::uint32_t id) {
    if(auto s=impl_->check();!s) return {{},s};
    for(auto& weak:impl_->controllers) if(auto c=weak.lock();c && c->device_info().id==id && c->is_connected())
        return {{},{ErrorCode::AlreadyOpen,"Device already open in this Context"}};
    Status status;
    auto backend=detail::open_sdl(id,impl_->runtime,status);
    if(!backend) return {{},status};
    auto c=detail::ControllerAccess::make(std::move(backend));
    impl_->controllers.push_back(c); impl_->dirty.insert(id);
    return {std::move(c),{}};
}
Status Context::poll(const std::function<void(const SDL_Event&)>& other) {
    auto& p=*impl_;
    if(auto s=p.check();!s) return s;
    if(p.mode!=EventMode::Standalone) return {ErrorCode::WrongEventMode,"External mode does not poll SDL"};
    if(p.polling || p.refreshing) return {ErrorCode::WrongEventMode,"recursive poll is not supported"};
    p.polling=true;
    struct Guard { bool& flag; ~Guard(){flag=false;} } guard{p.polling};
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        p.mark(event);
        const bool gamepad_event=(event.type>=SDL_EVENT_GAMEPAD_AXIS_MOTION && event.type<=SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED);
        bool owned=false;
        if(gamepad_event) {
            // These SDL gamepad event structs share the common prefix through which.
            for(const auto& weak:p.controllers) {
                if(auto c=weak.lock();c && c->device_info().id==event.gdevice.which) { owned=true; break; }
            }
        }
        if(!owned && other) {
            try { other(event); }
            catch(const std::exception& e) { return {ErrorCode::SdlError,std::string("event callback: ")+e.what()}; }
            catch(...) { return {ErrorCode::SdlError,"event callback threw"}; }
        }
    }
    return refresh();
}
Status Context::handle_event(const SDL_Event& event) {
    if(auto s=impl_->check();!s) return s;
    if(impl_->mode!=EventMode::External) return {ErrorCode::WrongEventMode,"handle_event requires External mode"};
    impl_->mark(event); return {};
}
Status Context::refresh() {
    auto& p=*impl_;
    if(auto s=p.check();!s) return s;
    if(p.refreshing) return {ErrorCode::WrongEventMode,"recursive refresh is not supported"};
    p.refreshing=true;
    struct Guard { bool& flag; ~Guard(){flag=false;} } guard{p.refreshing};
    auto dirty=std::move(p.dirty); p.dirty.clear();
    // Hold strong references while callbacks run; callbacks may open other devices.
    std::vector<std::shared_ptr<Controller>> live;
    for(auto it=p.controllers.begin();it!=p.controllers.end();) {
        if(auto c=it->lock()) { live.push_back(std::move(c)); ++it; }
        else it=p.controllers.erase(it);
    }
    Status result;
    for(auto& c:live) {
        auto s=c->refresh(dirty.count(c->device_info().id)>0);
        if(!s && result) result=s;
    }
    return result;
}
}
