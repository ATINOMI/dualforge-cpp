#pragma once
#include <dualforge/dualforge.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace demo {
enum class MouseActionKind { Move, Scroll, LeftDown, LeftUp, LeftClick, RightClick };
struct MouseAction { MouseActionKind kind; int x=0,y=0,click_count=1; };
// 纯手势逻辑，不操作桌面。归一化触点换算到旧示例的 1920×1080 坐标。
class TouchpadGestures {
    enum class Mode { Idle, Single, Dual, Suppressed } mode_=Mode::Idle;
    bool previous_pad_=false, held_=false, cancelled_=false, moved_=false;
    int primary_=0;
    double started_=0,last_tap_=-1,origin_x_=0,origin_y_=0,previous_x_=0,previous_y_=0;
    double remainder_x_=0,remainder_y_=0,scroll_remainder_=0,max_movement_=0;
    static constexpr double tap_time=0.3,tap_distance=40,stabilize=0.15;
    static double x(const dualforge::Finger& f) {return f.x*1920.0;}
    static double y(const dualforge::Finger& f) {return f.y*1080.0;}
public:
    std::vector<MouseAction> release() {
        std::vector<MouseAction> actions;
        if(held_)actions.push_back({MouseActionKind::LeftUp});
        held_=false;previous_pad_=false;mode_=Mode::Idle;cancelled_=false;last_tap_=-1;
        return actions;
    }
    std::vector<MouseAction> update(const dualforge::InputState& s,double now) {
        std::vector<MouseAction> actions;
        if(!s.connected)return release();
        const int count=int(s.fingers[0].touching)+int(s.fingers[1].touching);
        const bool pad=s.buttons.pad;
        if(pad && !previous_pad_) {
            cancelled_=true;last_tap_=-1;
            if(count==2)actions.push_back({MouseActionKind::RightClick});
            else {actions.push_back({MouseActionKind::LeftDown});held_=true;}
        }
        if(!pad && previous_pad_ && held_) {actions.push_back({MouseActionKind::LeftUp});held_=false;}
        previous_pad_=pad;
        if(mode_==Mode::Suppressed) {
            if(count==0){mode_=Mode::Idle;cancelled_=pad;}
            return actions;
        }
        if(count==2) {
            const double cx=(x(s.fingers[0])+x(s.fingers[1]))/2;
            const double cy=(y(s.fingers[0])+y(s.fingers[1]))/2;
            if(mode_!=Mode::Dual) {
                mode_=Mode::Dual;started_=now;origin_x_=cx;origin_y_=cy;
                previous_x_=cx;previous_y_=cy;max_movement_=0;scroll_remainder_=0;
                last_tap_=-1;
            } else {
                max_movement_=std::max(max_movement_,std::abs(cx-origin_x_)+std::abs(cy-origin_y_));
                if(now-started_>stabilize) {
                    scroll_remainder_+=(cy-previous_y_)*0.8;
                    const int steps=int(scroll_remainder_);
                    if(steps){actions.push_back({MouseActionKind::Scroll,0,steps});scroll_remainder_-=steps;cancelled_=true;}
                }
                previous_x_=cx;previous_y_=cy;
            }
            return actions;
        }
        if(mode_==Mode::Dual) {
            if(!cancelled_ && now-started_<tap_time && max_movement_<=tap_distance)
                actions.push_back({MouseActionKind::RightClick});
            // 双指变为单指后，必须全部抬起才恢复识别，避免跳跃和误单击。
            mode_=count?Mode::Suppressed:Mode::Idle;cancelled_=pad;return actions;
        }
        if(count==1) {
            const int index=s.fingers[0].touching?0:1;
            const auto& f=s.fingers[static_cast<std::size_t>(index)];const double cx=x(f),cy=y(f);
            if(mode_!=Mode::Single || primary_!=index) {
                if(mode_==Mode::Single)cancelled_=true;
                mode_=Mode::Single;primary_=index;started_=now;
                origin_x_=previous_x_=cx;origin_y_=previous_y_=cy;
                moved_=false;remainder_x_=remainder_y_=0;
            } else {
                if(std::abs(cx-origin_x_)+std::abs(cy-origin_y_)>tap_distance || now-started_>stabilize)moved_=true;
                if(moved_ || held_) {
                    remainder_x_+=cx-previous_x_;remainder_y_+=cy-previous_y_;
                    const int dx=int(remainder_x_),dy=int(remainder_y_);
                    if(dx || dy){actions.push_back({MouseActionKind::Move,dx,dy});remainder_x_-=dx;remainder_y_-=dy;}
                }
                previous_x_=cx;previous_y_=cy;
            }
        } else {
            if(mode_==Mode::Single && !moved_ && !cancelled_ && now-started_<tap_time) {
                const bool twice=last_tap_>=0 && now-last_tap_<tap_time;
                actions.push_back({MouseActionKind::LeftClick,0,0,twice?2:1});
                // 第二次轻触只追加一次点击，不重复发送两次。
                last_tap_=twice?-1:now;
            }
            mode_=Mode::Idle;cancelled_=pad;
        }
        return actions;
    }
};
}
