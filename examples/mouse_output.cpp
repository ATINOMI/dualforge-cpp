#include "mouse_output.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace demo {
struct MouseOutput::Impl {
    bool dry=false,held=false,right_held=false;
#if defined(__linux__)
    int fd=-1;
    void emit(unsigned short type,unsigned short code,int value) {
        input_event event{};event.type=type;event.code=code;event.value=value;
        ssize_t written;
        do {written=::write(fd,&event,sizeof(event));}while(written<0 && errno==EINTR);
        if(written!=static_cast<ssize_t>(sizeof(event)))throw std::runtime_error(std::string("uinput write: ")+std::strerror(errno));
    }
    void sync() {emit(EV_SYN,SYN_REPORT,0);}
#elif defined(_WIN32)
    void inject(DWORD flags,int x=0,int y=0,DWORD data=0) {
        INPUT input{};input.type=INPUT_MOUSE;input.mi.dx=x;input.mi.dy=y;input.mi.dwFlags=flags;input.mi.mouseData=data;
        if(SendInput(1,&input,sizeof(INPUT))!=1)throw std::runtime_error("SendInput failed (permissions/UIPI may block input)");
    }
#elif defined(__APPLE__)
    CGPoint position() {
        auto event=CGEventCreate(nullptr);if(!event)throw std::runtime_error("CGEventCreate failed");
        const auto point=CGEventGetLocation(event);CFRelease(event);return point;
    }
    void mouse(CGEventType type,CGMouseButton button,CGPoint point,int clicks=1) {
        auto event=CGEventCreateMouseEvent(nullptr,type,point,button);
        if(!event)throw std::runtime_error("CGEventCreateMouseEvent failed");
        CGEventSetIntegerValueField(event,kCGMouseEventClickState,clicks);
        CGEventPost(kCGHIDEventTap,event);CFRelease(event);
    }
#endif
    void button(bool left,bool down,int clicks=1) {
#if defined(__linux__)
        (void)clicks;emit(EV_KEY,left?BTN_LEFT:BTN_RIGHT,down?1:0);sync();
#elif defined(_WIN32)
        (void)clicks;inject(left?(down?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP):(down?MOUSEEVENTF_RIGHTDOWN:MOUSEEVENTF_RIGHTUP));
#elif defined(__APPLE__)
        mouse(left?(down?kCGEventLeftMouseDown:kCGEventLeftMouseUp):(down?kCGEventRightMouseDown:kCGEventRightMouseUp),
            left?kCGMouseButtonLeft:kCGMouseButtonRight,position(),clicks);
#else
        (void)left;(void)down;(void)clicks;
#endif
    }
    void move(int x,int y) {
#if defined(__linux__)
        emit(EV_REL,REL_X,x);emit(EV_REL,REL_Y,y);sync();
#elif defined(_WIN32)
        inject(MOUSEEVENTF_MOVE,x,y);
#elif defined(__APPLE__)
        auto point=position();point.x+=x;point.y+=y;
        mouse(held?kCGEventLeftMouseDragged:kCGEventMouseMoved,kCGMouseButtonLeft,point);
#else
        (void)x;(void)y;
#endif
    }
    void scroll(int steps) {
#if defined(__linux__)
        emit(EV_REL,REL_WHEEL,steps);sync();
#elif defined(_WIN32)
        inject(MOUSEEVENTF_WHEEL,0,0,static_cast<DWORD>(steps*WHEEL_DELTA));
#elif defined(__APPLE__)
        auto event=CGEventCreateScrollWheelEvent(nullptr,kCGScrollEventUnitLine,1,steps);
        if(!event)throw std::runtime_error("CGEventCreateScrollWheelEvent failed");
        CGEventPost(kCGHIDEventTap,event);CFRelease(event);
#else
        (void)steps;
#endif
    }
    ~Impl() {
        if(!dry && held) {try {button(true,false);}catch(...) {}}
        if(!dry && right_held) {try {button(false,false);}catch(...) {}}
#if defined(__linux__)
        if(fd>=0){ioctl(fd,UI_DEV_DESTROY);::close(fd);}
#endif
    }
};
MouseOutput::MouseOutput(bool dry_run):impl_(std::make_unique<Impl>()) {
    impl_->dry=dry_run;if(dry_run)return;
#if defined(__linux__)
    impl_->fd=::open("/dev/uinput",O_WRONLY|O_NONBLOCK|O_CLOEXEC);
    if(impl_->fd<0)throw std::runtime_error(std::string("Cannot open /dev/uinput: ")+std::strerror(errno)+". Use --dry-run or configure uinput access for your user.");
    auto configure=[&](unsigned long request,int value){if(ioctl(impl_->fd,request,value)<0)throw std::runtime_error(std::string("uinput setup: ")+std::strerror(errno));};
    configure(UI_SET_EVBIT,EV_KEY);configure(UI_SET_KEYBIT,BTN_LEFT);configure(UI_SET_KEYBIT,BTN_RIGHT);
    configure(UI_SET_EVBIT,EV_REL);configure(UI_SET_RELBIT,REL_X);configure(UI_SET_RELBIT,REL_Y);configure(UI_SET_RELBIT,REL_WHEEL);
    uinput_setup setup{};setup.id.bustype=BUS_USB;setup.id.vendor=0x0001;setup.id.product=0x0001;
    std::strncpy(setup.name,"DualForge touchpad mouse",sizeof(setup.name)-1);
    if(ioctl(impl_->fd,UI_DEV_SETUP,&setup)<0 || ioctl(impl_->fd,UI_DEV_CREATE)<0)throw std::runtime_error(std::string("uinput create: ")+std::strerror(errno));
#elif defined(__APPLE__)
    if(__builtin_available(macOS 10.15,*)) {
        if(!CGPreflightPostEventAccess())throw std::runtime_error("Allow this terminal/app in macOS Accessibility settings, or use --dry-run.");
    }
#elif !defined(_WIN32)
    throw std::runtime_error("Native mouse output is unavailable on this platform; use --dry-run.");
#endif
}
MouseOutput::~MouseOutput()=default;
void MouseOutput::send(const MouseAction& action) {
    auto& p=*impl_;
    if(p.dry) {
        const char* name="";
        switch(action.kind) {
        case MouseActionKind::Move:name="move";break;case MouseActionKind::Scroll:name="scroll";break;
        case MouseActionKind::LeftDown:name="left-down";break;case MouseActionKind::LeftUp:name="left-up";break;
        case MouseActionKind::LeftClick:name="left-click";break;case MouseActionKind::RightClick:name="right-click";break;
        }
        std::cout<<name<<" x="<<action.x<<" y="<<action.y<<" click-count="<<action.click_count<<'\n';return;
    }
    switch(action.kind) {
    case MouseActionKind::Move:p.move(action.x,action.y);break;
    case MouseActionKind::Scroll:p.scroll(action.y);break;
    case MouseActionKind::LeftDown:p.button(true,true);p.held=true;break;
    case MouseActionKind::LeftUp:p.button(true,false);p.held=false;break;
    case MouseActionKind::LeftClick:
    case MouseActionKind::RightClick: {
        const bool left=action.kind==MouseActionKind::LeftClick;
        // Track a successful press so exception cleanup can release it too.
        p.button(left,true,action.click_count);
        if(left)p.held=true;else p.right_held=true;
        p.button(left,false,action.click_count);
        if(left)p.held=false;else p.right_held=false;
        break;
    }
    }
}
}
