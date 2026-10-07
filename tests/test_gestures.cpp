#include "check.hpp"
#include "touchpad_gestures.hpp"
#include "common.hpp"
using namespace dualforge;
using namespace demo;
InputState sample(int fingers,bool pad=false) {
    InputState s;s.connected=true;s.buttons.pad=pad;
    s.fingers[0]={fingers>=1,0.5f,0.5f,1};s.fingers[1]={fingers==2,0.7f,0.5f,1};return s;
}
void only(const std::vector<MouseAction>& actions,MouseActionKind kind) {
    CHECK(actions.size()==1 && actions.front().kind==kind);
}
int main() try {
    TouchpadGestures taps;
    CHECK(taps.update(sample(1),0).empty());
    auto click=taps.update(sample(0),0.1);only(click,MouseActionKind::LeftClick);CHECK(click[0].click_count==1);
    CHECK(taps.update(sample(1),0.2).empty());
    auto second=taps.update(sample(0),0.25);only(second,MouseActionKind::LeftClick);CHECK(second[0].click_count==2);
    // Two taps produce exactly two click actions, not three actual clicks.
    TouchpadGestures slot_one;auto one=sample(0);one.fingers[1]={true,0.5f,0.5f,1};
    CHECK(slot_one.update(one,0).empty());only(slot_one.update(sample(0),0.1),MouseActionKind::LeftClick);
    TouchpadGestures movement;CHECK(movement.update(sample(1),0).empty());
    auto moved=sample(1);moved.fingers[0].x=0.6f;
    const auto motion=movement.update(moved,0.2);only(motion,MouseActionKind::Move);CHECK(motion[0].x>100);
    CHECK(movement.update(sample(0),0.22).empty());
    TouchpadGestures hold;CHECK(hold.update(sample(1),0).empty());CHECK(hold.update(sample(1),0.2).empty());
    CHECK(hold.update(sample(0),0.25).empty());
    TouchpadGestures dual;
    CHECK(dual.update(sample(2),0).empty());
    only(dual.update(sample(1),0.1),MouseActionKind::RightClick);
    moved=sample(1);moved.fingers[0].x=0.9f;
    CHECK(dual.update(moved,0.12).empty());CHECK(dual.update(sample(0),0.15).empty());
    TouchpadGestures scroll;
    CHECK(scroll.update(sample(2),0).empty());
    moved=sample(2);moved.fingers[0].y+=0.01f;moved.fingers[1].y+=0.01f;
    CHECK(scroll.update(moved,0.1).empty());
    moved.fingers[0].y+=0.01f;moved.fingers[1].y+=0.01f;
    auto wheel=scroll.update(moved,0.2);only(wheel,MouseActionKind::Scroll);CHECK(wheel[0].y>0);
    CHECK(scroll.update(sample(0),0.23).empty());
    TouchpadGestures fast_swipe;CHECK(fast_swipe.update(sample(2),0).empty());
    moved=sample(2);moved.fingers[0].x+=0.1f;moved.fingers[1].x+=0.1f;
    CHECK(fast_swipe.update(moved,0.05).empty());CHECK(fast_swipe.update(sample(0),0.1).empty());
    TouchpadGestures drag;
    only(drag.update(sample(1,true),0),MouseActionKind::LeftDown);
    moved=sample(1,true);moved.fingers[0].x+=0.01f;
    only(drag.update(moved,0.01),MouseActionKind::Move);
    CHECK(drag.update(sample(0,true),0.02).empty());
    only(drag.update(sample(0),0.03),MouseActionKind::LeftUp);CHECK(drag.release().empty());
    TouchpadGestures physical_right;
    only(physical_right.update(sample(2,true),0),MouseActionKind::RightClick);
    CHECK(physical_right.update(sample(0),0.1).empty());
    TouchpadGestures disconnect;
    only(disconnect.update(sample(0,true),0),MouseActionKind::LeftDown);
    InputState offline;
    only(disconnect.update(offline,0.1),MouseActionKind::LeftUp);CHECK(disconnect.update(offline,0.2).empty());
    TouchpadGestures cleanup;
    only(cleanup.update(sample(0,true),0),MouseActionKind::LeftDown);
    only(cleanup.release(),MouseActionKind::LeftUp);CHECK(cleanup.release().empty());
    TouchpadGestures replacement;CHECK(replacement.update(sample(1),0).empty());
    CHECK(replacement.update(one,0.1).empty());CHECK(replacement.update(sample(0),0.15).empty());
    // CLI parsing rejects errors before devices or desktop mouse APIs are opened.
    char program[]="example",seconds[]="--seconds",negative[]="-1",unknown[]="--unknown";
    char* invalid_time[]={program,seconds,negative};rejects([&]{parse(3,invalid_time);});
    char* invalid_option[]={program,unknown};rejects([&]{parse(2,invalid_option);});
    std::cout<<"Gesture tap/scroll/drag transitions, slot changes and cleanup passed\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
