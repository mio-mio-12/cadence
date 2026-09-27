#pragma once
namespace gameplay::exo {
inline bool available(bool charge,bool infinite,bool firstAirborne,bool primed){
    return charge||infinite||(firstAirborne&&!primed);
}
inline void queue(bool& pending,bool enabled,bool grounded,bool pressed,bool eligible){
    if(!enabled||grounded||!eligible)pending=false;
    else if(pressed)pending=true;
}
inline bool ready(bool pending,bool grounded,float grace,bool firstAirborne){
    return pending&&!grounded&&(firstAirborne||grace<=0.f);
}
}
