#pragma once
#include <dualforge/trigger_effects.hpp>
#include <SDL3/SDL_events.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dualforge {
enum class ErrorCode { None, InvalidArgument, NotConnected, Unsupported, SdlError, WrongThread, AlreadyOpen, WrongEventMode };
struct Status {
    ErrorCode code = ErrorCode::None;
    std::string message;
    explicit operator bool() const noexcept { return code == ErrorCode::None; }
};
enum class Availability { Unavailable, Available, Unverified };
struct Capabilities {
    Availability rgb = Availability::Unavailable, rumble = Availability::Unavailable;
    Availability touchpad = Availability::Unavailable, gyro = Availability::Unavailable;
    Availability accelerometer = Availability::Unavailable;
    Availability trigger_effects = Availability::Unavailable, light_extensions = Availability::Unavailable;
};
struct Vec2 { float x=0, y=0; };
struct Vec3 { float x=0, y=0, z=0; };
struct Finger { bool touching=false; float x=0, y=0, pressure=0; };
struct Buttons {
    bool cross=false, square=false, circle=false, triangle=false;
    bool l1=false, r1=false, l3=false, r3=false, create=false, options=false;
    bool home=false, pad=false, mute=false;
    bool up=false, right=false, down=false, left=false;
};
struct InputState {
    bool connected=false;
    Vec2 left_stick, right_stick; // [-1,1], SDL axes, positive Y points down
    float left_trigger=0, right_trigger=0; // [0,1]
    Buttons buttons;
    std::array<Finger,2> fingers{}; // normalized coordinates
    Vec3 gyro, accelerometer; // SDL axes; rad/s and m/s^2
    int battery_percent=-1; // -1 = unknown
    enum class Power { Unknown, OnBattery, Charging, Charged, NoBattery } power=Power::Unknown;
};
struct DeviceInfo {
    std::uint32_t id=0;
    std::string name, path;
    std::uint16_t vendor=0, product=0;
    bool usb=false; // false also covers unknown connection type
};
enum class TriggerTarget { Left, Right, Both };
enum class LightBrightness : std::uint8_t { Bright=0, Mid=1, Dim=2 };
enum class LightFadeAnimation : std::uint8_t { Nothing=0, FadeIn=1, FadeOut=2 };
enum class EventMode { Standalone, External };
namespace detail { class Backend; struct ControllerAccess; struct Runtime; }

// All public operations and destruction belong to the main thread.
class Controller {
public:
    ~Controller();
    Controller(const Controller&)=delete;
    Controller& operator=(const Controller&)=delete;
    const DeviceInfo& device_info() const noexcept;
    InputState state() const;
    Capabilities capabilities() const;
    bool is_connected() const;
    // Callbacks are sampled once per refresh, not once per physical USB report.
    void on_input(std::function<void(const InputState&)> callback);
    void on_state(std::function<void(bool)> callback);
    Status close(); // idempotent, also safe inside callbacks
    Status set_led(int r,int g,int b);
    Status set_player_lights(int mask);
    Status set_light_brightness(LightBrightness brightness);
    Status set_light_fade_animation(LightFadeAnimation animation);
    Status set_rumble(int left,int right,std::uint32_t duration_ms=1000); // strengths 0..65535
    Status stop_rumble();
    Status set_trigger_effect(TriggerTarget target,const TriggerEffect& effect);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit Controller(std::unique_ptr<detail::Backend> backend);
    Status refresh(bool notify_input);
    friend class Context;
    friend struct detail::ControllerAccess;
};
struct OpenResult { std::shared_ptr<Controller> controller; Status status; };

class Context {
public:
    explicit Context(EventMode mode=EventMode::Standalone);
    ~Context();
    Context(const Context&)=delete;
    Context& operator=(const Context&)=delete;
    Status status() const;
    std::vector<DeviceInfo> enumerate() const;
    OpenResult open(std::uint32_t device_id);
    // Standalone: drains SDL events, forwards unrelated events, then refreshes.
    Status poll(const std::function<void(const SDL_Event&)>& other_event={});
    // External: caller pumps SDL events; this only marks affected controllers dirty.
    Status handle_event(const SDL_Event& event);
    Status refresh();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
