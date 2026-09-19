#include "app/ReplayVisibility.h"
#include <iostream>
int main(){
    for(int flags=0;flags<256;++flags){
        const bool first=flags&1,hidden=flags&2,actor=flags&4,shoulder=flags&8,world=flags&16,retain=flags&32,nav=flags&64,vm=flags&128;
        if(cadence::showFirstPersonRig(true,first,hidden,actor,shoulder,world,retain,nav)!=(first&&!hidden))return 4;
        if(cadence::useViewmodelCamera(true,first,vm,actor,shoulder,retain)!=first)return 5;
        if(cadence::showFirstPersonRig(false,first,hidden,actor,shoulder,world,retain,nav)!=(!hidden&&!(actor&&shoulder)&&(!world||retain)&&!nav))return 6;
        if(cadence::useViewmodelCamera(false,first,vm,actor,shoulder,retain)!=((vm||actor)&&!(actor&&shoulder)&&!retain))return 7;
    }
    for(int editor=0;editor<2;++editor)for(int actor=0;actor<2;++actor)for(int shoulder=0;shoulder<2;++shoulder)for(int freecam=0;freecam<2;++freecam){
        if(cadence::showPlayerWorldProxy(true,true,editor,actor,shoulder,freecam))return 1;
        if(!cadence::showPlayerWorldProxy(true,false,editor,actor,shoulder,freecam))return 2;
        if(cadence::showPlayerWorldProxy(false,false,editor,actor,shoulder,freecam)!=(editor||(actor&&(shoulder||freecam))))return 3;
    }
    std::cout<<"Recorded first/third-person visibility overrides every live-camera flag combination\n";
}
