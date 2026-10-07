#include "check.hpp"
#include <dualforge/dualforge.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
using namespace dualforge;
int main() try {
    // Host owns an independent reference. DualForge must not shut it down.
    CHECK(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
    std::shared_ptr<Controller> outliving;
    {
        Context ctx; CHECK(ctx.status());
        CHECK(!ctx.open(0).status);
        SDL_VirtualJoystickTouchpadDesc touch{}; touch.nfingers=2;
        SDL_VirtualJoystickSensorDesc sensors[2]{};
        sensors[0].type=SDL_SENSOR_GYRO; sensors[0].rate=250;
        sensors[1].type=SDL_SENSOR_ACCEL; sensors[1].rate=250;
        SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc);
        desc.type=SDL_JOYSTICK_TYPE_GAMEPAD; desc.vendor_id=0x054c; desc.product_id=0x0ce6;
        desc.naxes=SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
        desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
        desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
        desc.ntouchpads=1;desc.touchpads=&touch;desc.nsensors=2;desc.sensors=sensors;
        desc.SetSensorsEnabled=[](void*,bool){return true;};
        desc.name="DualForge virtual DualSense";
        const auto id=SDL_AttachVirtualJoystick(&desc); CHECK(id!=0);
        CHECK(ctx.poll());
        auto devices=ctx.enumerate(); CHECK(std::any_of(devices.begin(),devices.end(),[&](auto& d){return d.id==id;}));
        auto opened=ctx.open(id); CHECK(opened.status && opened.controller); auto c=opened.controller;
        CHECK(ctx.open(id).status.code==ErrorCode::AlreadyOpen);
        Context another(EventMode::External);CHECK(another.status());
        CHECK(another.open(id).status.code==ErrorCode::AlreadyOpen);
        CHECK(c->capabilities().trigger_effects==Availability::Unavailable);
        CHECK(c->set_trigger_effect(TriggerTarget::Both,effects::off()).code==ErrorCode::Unsupported);
        auto* joystick=SDL_GetJoystickFromID(id); CHECK(joystick);
        CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFTX,-32768));
        CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHTY,32767));
        CHECK(SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767));
        CHECK(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,true));
        CHECK(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,0.25f,0.75f,1));
        CHECK(SDL_SetJoystickVirtualTouchpad(joystick,0,1,true,0.8f,0.2f,1));
        const float gyro[3]={1,2,3}, accel[3]={4,5,6};
        CHECK(SDL_SendJoystickVirtualSensorData(joystick,SDL_SENSOR_GYRO,SDL_GetTicksNS(),gyro,3));
        CHECK(SDL_SendJoystickVirtualSensorData(joystick,SDL_SENSOR_ACCEL,SDL_GetTicksNS(),accel,3));
        int inputs=0;c->on_input([&](const InputState&){++inputs;});CHECK(ctx.poll());
        const auto s=c->state(); CHECK(s.connected && s.buttons.cross);
        CHECK(s.left_stick.x==-1 && s.right_stick.y==1 && s.right_trigger==1);
        CHECK(s.fingers[0].touching && s.fingers[1].touching && s.fingers[0].x==0.25f);
        CHECK(s.gyro.y==2 && s.accelerometer.z==6 && inputs>0);
        bool forwarded=false; SDL_Event user{};user.type=SDL_EVENT_USER;CHECK(SDL_PushEvent(&user));
        CHECK(ctx.poll([&](const SDL_Event& e){if(e.type==SDL_EVENT_USER) forwarded=true;})); CHECK(forwarded);
        SDL_Event unrelated{}; unrelated.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; unrelated.gbutton.which=0xf0000000u;
        CHECK(SDL_PushEvent(&unrelated)); forwarded=false;
        CHECK(ctx.poll([&](const SDL_Event& e){if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN && e.gbutton.which==0xf0000000u) forwarded=true;}));
        CHECK(forwarded);
        CHECK(ctx.handle_event(user).code==ErrorCode::WrongEventMode);
        CHECK(SDL_PushEvent(&user));
        CHECK(ctx.poll([](const SDL_Event& e){if(e.type==SDL_EVENT_USER) throw std::runtime_error("host callback failure");}).code==ErrorCode::SdlError);
        CHECK(ctx.poll());
        int disconnected=0;c->on_state([&](bool connected){CHECK(!connected);++disconnected;});
        CHECK(SDL_DetachVirtualJoystick(id));CHECK(ctx.poll());CHECK(disconnected==1 && !c->state().connected);
        CHECK(ctx.poll());CHECK(disconnected==1);
        // Reinsert as a new instance, handled by the caller-owned SDL event loop.
        Context external(EventMode::External);CHECK(external.status());
        const auto second=SDL_AttachVirtualJoystick(&desc);CHECK(second && second!=id);
        SDL_Event event;while(SDL_PollEvent(&event)) CHECK(external.handle_event(event));
        auto reopened=external.open(second);CHECK(reopened.status);outliving=reopened.controller;
        CHECK(external.poll().code==ErrorCode::WrongEventMode);
        joystick=SDL_GetJoystickFromID(second);CHECK(joystick);
        CHECK(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_EAST,true));
        while(SDL_PollEvent(&event)) CHECK(external.handle_event(event));
        CHECK(external.refresh());CHECK(outliving->state().buttons.circle);
        CHECK(SDL_DetachVirtualJoystick(second));
    }
    CHECK(outliving->close());outliving.reset();
    CHECK(SDL_WasInit(SDL_INIT_GAMEPAD)&SDL_INIT_GAMEPAD);
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    std::cout<<"SDL virtual input, sensors, event modes and host ownership passed\n";
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
