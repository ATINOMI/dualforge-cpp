#include "backend.hpp"
#include <chrono>
#include <exception>

namespace dualforge {
namespace {
Status invalid(const char* message) { return {ErrorCode::InvalidArgument,message}; }
}
struct Controller::Impl {
    std::unique_ptr<detail::Backend> backend;
    DeviceInfo info;
    InputState state;
    std::thread::id thread=std::this_thread::get_id();
    std::function<void(const InputState&)> input_callback;
    std::function<void(bool)> state_callback;
    bool dispatching=false, close_requested=false;
    std::uint16_t rumble_left=0, rumble_right=0;
    std::chrono::steady_clock::time_point rumble_until{};
    detail::Report light_settings{};
    Status check() const {
        if(thread!=std::this_thread::get_id()) return {ErrorCode::WrongThread,"Controller requires its main thread"};
        if(!backend || close_requested || !backend->connected()) return {ErrorCode::NotConnected,"Controller is disconnected"};
        return {};
    }
    Status lights(detail::Report update) {
        // SDL coalesces pending effects by the first two flag bytes, ignoring
        // byte 38. Carry both configured light settings so back-to-back calls
        // cannot lose brightness or animation in that queue.
        auto next=light_settings;
        next[38]|=update[38];
        if(update[38]&1) next[42]=update[42];
        if(update[38]&2) next[41]=update[41];
        auto status=send(next);
        if(status) light_settings=next;
        return status;
    }
    Status send(detail::Report r) {
        // Raw effects must preserve SDL's currently active rumble emulation.
        if(std::chrono::steady_clock::now()<rumble_until && (rumble_left || rumble_right)) {
            r[0]|=0x02;
            if(backend->improved_rumble()) r[38]|=0x04;
            else r[0]|=0x01;
            const int shift=backend->improved_rumble()?8:9;
            r[2]=static_cast<std::uint8_t>(rumble_right>>shift);
            r[3]=static_cast<std::uint8_t>(rumble_left>>shift);
        }
        return backend->send(r);
    }
};
Controller::Controller(std::unique_ptr<detail::Backend> b):impl_(std::make_unique<Impl>()) {
    impl_->info=b->info(); impl_->state=b->read(); impl_->backend=std::move(b);
}
Controller::~Controller() {
    // No callbacks during destruction. Handle cleanup is best effort and never throws.
    impl_->input_callback={}; impl_->state_callback={};
    try { (void)close(); } catch(...) {}
}
const DeviceInfo& Controller::device_info() const noexcept { return impl_->info; }
InputState Controller::state() const { return impl_->state; }
Capabilities Controller::capabilities() const { return impl_->backend?impl_->backend->capabilities():Capabilities{}; }
bool Controller::is_connected() const { return impl_->backend && !impl_->close_requested && impl_->backend->connected(); }
void Controller::on_input(std::function<void(const InputState&)> cb) { impl_->input_callback=std::move(cb); }
void Controller::on_state(std::function<void(bool)> cb) { impl_->state_callback=std::move(cb); }
Status Controller::close() {
    auto& p=*impl_;
    if(p.thread!=std::this_thread::get_id()) return {ErrorCode::WrongThread,"close requires main thread"};
    if(p.dispatching) { p.close_requested=true; return {}; }
    Status result;
    if(p.backend && p.backend->connected()) {
        const auto caps=p.backend->capabilities();
        if(caps.rumble==Availability::Available) result=p.backend->rumble(0,0,0);
        if(caps.trigger_effects!=Availability::Unavailable) {
            auto stop=p.backend->send(detail::trigger_report(TriggerTarget::Both,effects::off()));
            if(!stop && result) result=stop;
        }
    }
    p.backend.reset(); p.rumble_left=p.rumble_right=0; p.close_requested=false;
    const bool notify=p.state.connected; p.state={};
    if(notify && p.state_callback) {
        auto callback=p.state_callback;
        try { callback(false); }
        catch(const std::exception& e) { return {ErrorCode::SdlError,std::string("state callback: ")+e.what()}; }
        catch(...) { return {ErrorCode::SdlError,"state callback threw"}; }
    }
    return result;
}
Status Controller::refresh(bool notify_input) {
    auto& p=*impl_;
    if(p.thread!=std::this_thread::get_id()) return {ErrorCode::WrongThread,"refresh requires main thread"};
    if(p.dispatching) return {ErrorCode::InvalidArgument,"recursive refresh is not supported"};
    if(!p.backend) return {};
    if(!p.backend->connected()) return close();
    p.state=p.backend->read();
    if(!notify_input || !p.input_callback) return {};
    auto callback=p.input_callback;
    const auto snapshot=p.state;
    p.dispatching=true;
    Status result;
    try { callback(snapshot); }
    catch(const std::exception& e) { result={ErrorCode::SdlError,std::string("input callback: ")+e.what()}; }
    catch(...) { result={ErrorCode::SdlError,"input callback threw"}; }
    p.dispatching=false;
    if(p.close_requested) { auto closed=close(); if(!closed && result) result=closed; }
    return result;
}
Status Controller::set_led(int r,int g,int b) {
    if(auto s=impl_->check();!s) return s;
    if(r<0 || r>255 || g<0 || g>255 || b<0 || b>255) return invalid("RGB must be 0..255");
    if(capabilities().rgb==Availability::Unavailable) return {ErrorCode::Unsupported,"RGB unavailable"};
    return impl_->backend->led(static_cast<std::uint8_t>(r),static_cast<std::uint8_t>(g),static_cast<std::uint8_t>(b));
}
Status Controller::set_player_lights(int mask) {
    if(auto s=impl_->check();!s) return s;
    if(mask<0 || mask>31) return invalid("player mask must be 0..31");
    if(capabilities().light_extensions==Availability::Unavailable) return {ErrorCode::Unsupported,"light extensions unavailable"};
    return impl_->send(detail::player_report(static_cast<std::uint8_t>(mask)));
}
Status Controller::set_light_brightness(LightBrightness v) {
    if(auto s=impl_->check();!s) return s;
    if(static_cast<unsigned>(v)>2) return invalid("invalid brightness");
    if(capabilities().light_extensions==Availability::Unavailable) return {ErrorCode::Unsupported,"light extensions unavailable"};
    return impl_->lights(detail::brightness_report(v));
}
Status Controller::set_light_fade_animation(LightFadeAnimation v) {
    if(auto s=impl_->check();!s) return s;
    if(static_cast<unsigned>(v)>2) return invalid("invalid animation");
    if(capabilities().light_extensions==Availability::Unavailable) return {ErrorCode::Unsupported,"light extensions unavailable"};
    return impl_->lights(detail::animation_report(v));
}
Status Controller::set_rumble(int left,int right,std::uint32_t duration) {
    if(auto s=impl_->check();!s) return s;
    if(left<0 || left>65535 || right<0 || right>65535) return invalid("rumble must be 0..65535");
    if((left || right) && duration==0) return invalid("nonzero rumble needs positive duration");
    if(capabilities().rumble==Availability::Unavailable) return {ErrorCode::Unsupported,"rumble unavailable"};
    auto s=impl_->backend->rumble(static_cast<std::uint16_t>(left),static_cast<std::uint16_t>(right),duration);
    if(s) {
        impl_->rumble_left=static_cast<std::uint16_t>(left); impl_->rumble_right=static_cast<std::uint16_t>(right);
        impl_->rumble_until=std::chrono::steady_clock::now()+std::chrono::milliseconds(duration);
    }
    return s;
}
Status Controller::stop_rumble() { return set_rumble(0,0,0); }
Status Controller::set_trigger_effect(TriggerTarget target,const TriggerEffect& effect) {
    if(auto s=impl_->check();!s) return s;
    if(target!=TriggerTarget::Left && target!=TriggerTarget::Right && target!=TriggerTarget::Both) return invalid("invalid trigger target");
    if(!effects::valid_type(effect[0])) return invalid("unknown/debug effect type");
    if(capabilities().trigger_effects==Availability::Unavailable) return {ErrorCode::Unsupported,"trigger effects unavailable"};
    return impl_->send(detail::trigger_report(target,effect));
}
}
