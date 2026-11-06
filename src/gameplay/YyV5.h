#pragma once
#include <algorithm>
#include <cmath>

namespace gameplay::yyv5 {
struct Sample { float frame{}, velocity{}; };
// Integral of smoothstep. Ramping velocity, rather than blending a frozen
// pose, keeps the authored putaway moving through its actual frames.
inline float integral(float u) { return u*u*u*(1.f-.5f*u); }
inline float smooth(float u) { return u*u*(3.f-2.f*u); }
struct Return {
    float start{}, incoming{}, duration{}, turn{}, stop{}, rate{};
    void begin(float frame,float velocity,float lastFrame,float seconds) {
        start=std::clamp(frame,0.f,std::max(0.f,lastFrame));
        incoming=velocity;
        duration=std::max(.001f,seconds);
        turn=std::min(.075f,duration*.25f);
        // Never sample past the authored clip when cancelling near its end.
        if(incoming>0)turn=std::min(turn,std::max(0.f,lastFrame-start)/incoming);
        if(incoming<0)turn=std::min(turn,start/-incoming);
        if(turn<.000001f){turn=std::min(.075f,duration*.25f);incoming=0;}
        stop=std::min(.060f,duration*.2f);
        rate=(start+incoming*turn*.5f)/(duration-(turn+stop)*.5f);
    }
    Sample at(float time) const {
        if(time>=duration)return {};
        const float t=std::max(0.f,time);
        if(turn>0&&t<turn){const float u=t/turn;return {start+incoming*t+(-rate-incoming)*turn*integral(u),incoming+(-rate-incoming)*smooth(u)};}
        const float afterTurn=start+(incoming-rate)*turn*.5f;
        if(t<duration-stop)return {afterTurn-rate*(t-turn),-rate};
        const float u=(t-(duration-stop))/stop;
        return {std::max(0.f,rate*stop*(.5f-u+integral(u))),-rate*(1.f-smooth(u))};
    }
};
// Used when another Y arrives during the return: retain the current frame
// and velocity instead of restarting the clip and crossfading from a snapshot.
struct Resume {
    float start{}, incoming{}, target{}, duration{};
    void begin(float frame,float velocity,float forwardRate) {
        start=frame;incoming=velocity;target=std::max(0.f,forwardRate);
        duration=.075f;
        if(incoming<0)duration=std::min(duration,std::max(0.f,frame)/-incoming);
        if(duration<.000001f){duration=0;incoming=0;}
    }
    Sample at(float time) const {
        const float t=std::max(0.f,time);
        if(duration>0&&t<duration){const float u=t/duration;return {start+incoming*t+(target-incoming)*duration*integral(u),incoming+(target-incoming)*smooth(u)};}
        return {start+(incoming+target)*duration*.5f+target*(t-duration),target};
    }
};
}
