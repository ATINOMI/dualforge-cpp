#include "check.hpp"
#include "backend.hpp"
#include <thread>
using namespace dualforge;
struct Record { bool connected=true, fail=false, destroyed=false, improved=false; int writes=0, stops=0; detail::Report last{}; };
class Mock:public detail::Backend {
    std::shared_ptr<Record> r_;
public:
    explicit Mock(std::shared_ptr<Record> r):r_(std::move(r)){}
    ~Mock() override { r_->destroyed=true; }
    DeviceInfo info() const override { return {123,"mock","mock",0x054c,0x0ce6,true}; }
    bool connected() const override { return r_->connected; }
    InputState read() override { InputState s; s.connected=r_->connected; s.left_stick.x=0.5f; return s; }
    Capabilities capabilities() const override {
        Capabilities c; c.rgb=c.rumble=Availability::Available; c.trigger_effects=c.light_extensions=Availability::Unverified; return c;
    }
    bool improved_rumble() const override { return r_->improved; }
    Status send(const detail::Report& data) override {
        ++r_->writes; r_->last=data;
        return r_->fail?Status{ErrorCode::SdlError,"simulated write failure"}:Status{};
    }
    Status led(std::uint8_t,std::uint8_t,std::uint8_t) override { return r_->fail?Status{ErrorCode::SdlError,"simulated LED failure"}:Status{}; }
    Status rumble(std::uint16_t l,std::uint16_t rr,std::uint32_t) override { if(!l && !rr) ++r_->stops; return {}; }
};
auto make(std::shared_ptr<Record> r) { return detail::ControllerAccess::make(std::make_unique<Mock>(std::move(r))); }
int main() try {
    auto record=std::make_shared<Record>(); auto c=make(record);
    CHECK(c->is_connected()); CHECK(c->state().left_stick.x==0.5f);
    CHECK(!c->set_led(-1,0,0)); CHECK(!c->set_player_lights(32));
    CHECK(!c->set_light_brightness(static_cast<LightBrightness>(3)));
    CHECK(!c->set_light_fade_animation(static_cast<LightFadeAnimation>(3)));
    CHECK(!c->set_rumble(65536,0)); CHECK(!c->set_rumble(1,1,0));
    CHECK(!c->set_trigger_effect(static_cast<TriggerTarget>(42),effects::off()));
    TriggerEffect bad{}; bad[0]=0xfc; CHECK(!c->set_trigger_effect(TriggerTarget::Both,bad));
    CHECK(record->writes==0);
    CHECK(c->set_light_brightness(LightBrightness::Dim));
    CHECK(c->set_light_fade_animation(LightFadeAnimation::FadeIn));
    CHECK(record->last[38]==3 && record->last[41]==1 && record->last[42]==2);
    CHECK(c->set_light_brightness(LightBrightness::Mid));
    CHECK(record->last[38]==3 && record->last[42]==1);
    CHECK(c->set_rumble(65535,32768,1000));
    CHECK(c->set_trigger_effect(TriggerTarget::Right,effects::feedback(3,5)));
    CHECK(record->last[0]==7 && record->last[2]==64 && record->last[3]==127);
    CHECK(c->stop_rumble()); CHECK(c->set_player_lights(4)); CHECK(record->last[0]==0);
    record->fail=true; CHECK(c->set_player_lights(4).code==ErrorCode::SdlError); record->fail=false;
    Status worker;
    std::thread t([&]{worker=c->set_led(0,0,0);}); t.join(); CHECK(worker.code==ErrorCode::WrongThread);
    int inputs=0, disconnects=0;
    c->on_state([&](bool connected){CHECK(!connected);++disconnects;});
    c->on_input([&](const InputState& s){CHECK(s.connected);++inputs;CHECK(c->close());CHECK(!record->destroyed);});
    CHECK(detail::ControllerAccess::refresh(*c));
    CHECK(inputs==1 && disconnects==1 && record->destroyed && !c->is_connected());
    CHECK(!c->state().connected && c->state().left_stick.x==0);
    CHECK(c->close()); CHECK(disconnects==1); CHECK(c->set_led(0,0,0).code==ErrorCode::NotConnected);
    auto unplug=std::make_shared<Record>(); auto u=make(unplug); int notifications=0;
    u->on_state([&](bool state){CHECK(!state); ++notifications;}); unplug->connected=false;
    CHECK(detail::ControllerAccess::refresh(*u)); CHECK(notifications==1 && unplug->writes==0 && unplug->destroyed);
    CHECK(detail::ControllerAccess::refresh(*u)); CHECK(notifications==1);
    auto exception=make(std::make_shared<Record>());
    exception->on_input([](const InputState&){throw std::runtime_error("callback failure");});
    CHECK(detail::ControllerAccess::refresh(*exception).code==ErrorCode::SdlError);
    exception->on_input([&](const InputState&){CHECK(exception->close());throw std::runtime_error("close then throw");});
    CHECK(!detail::ControllerAccess::refresh(*exception)); CHECK(!exception->is_connected());
    auto enhanced=std::make_shared<Record>(); enhanced->improved=true; auto enhanced_controller=make(enhanced);
    CHECK(enhanced_controller->set_rumble(65535,32768,1000));
    CHECK(enhanced_controller->set_trigger_effect(TriggerTarget::Right,effects::off()));
    CHECK(enhanced->last[0]==6 && enhanced->last[38]==4 && enhanced->last[2]==128 && enhanced->last[3]==255);
    CHECK(enhanced_controller->close());
    auto failclose=std::make_shared<Record>(); auto f=make(failclose); failclose->fail=true;
    CHECK(!f->close()); CHECK(failclose->destroyed);
    std::cout<<"Lifecycle, callback, disconnect and failure cases passed\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
