#include "gameplay/YyV5.h"
#include "app/UiHistory.h"
#include <iostream>
#include <cmath>
#include <vector>
int main(){
    int failures=0;
    auto check=[&](bool ok,const char* what){if(!ok){if(failures<20)std::cerr<<what<<'\n';++failures;}};
    for(float frame:{0.f,.01f,1.f,10.f,29.99f,30.f})
    for(float velocity:{-90.f,0.f,30.f,120.f})
    for(float seconds:{.001f,.05f,.15f,.444f,3.2f}){
        gameplay::yyv5::Return curve;curve.begin(frame,velocity,30,seconds);
        check(std::abs(curve.at(0).frame-frame)<.0001f,"start frame");
        check(std::abs(curve.at(0).velocity-curve.incoming)<.0001f,"incoming velocity");
        for(int i=0;i<=1000;++i){
            const auto s=curve.at(seconds*i/1000.f);
            check(std::isfinite(s.frame)&&std::isfinite(s.velocity),"finite");
            check(s.frame>=-.0001f&&s.frame<=30.0001f,"clip bounds");
        }
        check(curve.at(seconds).frame==0&&curve.at(seconds).velocity==0,"rest endpoint");
        for(float join:{curve.turn,seconds-curve.stop}){
            if(join<.00001f)continue;
            const float eps=std::min(seconds*.000001f,join*.00001f);
            check(std::abs(curve.at(join-eps).frame-curve.at(join+eps).frame)<.01f,"position continuity");
            check(std::abs(curve.at(join-eps).velocity-curve.at(join+eps).velocity)<.001f+curve.rate*.0001f,"velocity continuity");
        }
    }
    gameplay::yyv5::Return returning;returning.begin(12,75,30,.444f);
    check(returning.at(.001f).frame>12,"no instant reversal");
    check(returning.at(.15f).velocity<0,"returns along clip");
    for(float t:{.005f,.04f,.15f,.40f}){
        const auto p=returning.at(t);gameplay::yyv5::Resume resume;resume.begin(p.frame,p.velocity,75);
        check(std::abs(resume.at(0).frame-p.frame)<.0001f,"chain retains frame");
        check(std::abs(resume.at(0).velocity-p.velocity)<.0001f,"chain retains velocity");
        for(float dt:{1.f/30,1.f/60,1.f/165,1.f/300}){
            float elapsed=0;while(elapsed<.1f)elapsed=std::min(.1f,elapsed+dt);
            check(std::abs(resume.at(elapsed).frame-resume.at(.1f).frame)<.0001f,"frame rate independence");
        }
        const auto p2=resume.at(.005f);returning.begin(p2.frame,p2.velocity,30,.444f);
        check(std::abs(returning.at(0).velocity-p2.velocity)<.0001f,"rapid repeat preserves signed velocity");
    }
    cadence::ui::History<int> history;history.reset(0);
    history.observe(1,true);history.observe(2,true);history.observe(3);
    int value=3;check(history.undo(value)&&value==0,"coalesced undo");
    check(history.redo(value)&&value==3,"redo");
    history.undo(value);history.observe(5);check(history.next.empty(),"edit invalidates redo");
    for(int i=6;i<100;++i)history.observe(i);
    check(history.previous.size()==32,"bounded history");
    std::cout<<"YY v5 / UI history failures: "<<failures<<'\n';return failures?1:0;
}
