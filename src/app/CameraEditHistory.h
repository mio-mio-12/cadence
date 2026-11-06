#pragma once
#include "take/Take.h"
#include "app/UiHistory.h"
namespace cadence::ui {
struct CameraEdits {
    std::vector<take::DollyCameraKeyframe> nodes;
    std::vector<take::DollyCameraPath> paths;
    static bool sameNodes(const auto& a,const auto& b){
        if(a.size()!=b.size())return false;
        for(size_t i=0;i<a.size();++i){const auto& x=a[i];const auto& y=b[i];
            if(x.tick!=y.tick||x.fov!=y.fov||x.position.x!=y.position.x||x.position.y!=y.position.y||x.position.z!=y.position.z||x.rotationDegrees.x!=y.rotationDegrees.x||x.rotationDegrees.y!=y.rotationDegrees.y||x.rotationDegrees.z!=y.rotationDegrees.z)return false;}
        return true;
    }
    bool operator==(const CameraEdits& other)const{
        if(!sameNodes(nodes,other.nodes)||paths.size()!=other.paths.size())return false;
        for(size_t i=0;i<paths.size();++i)if(paths[i].name!=other.paths[i].name||!sameNodes(paths[i].keyframes,other.paths[i].keyframes))return false;
        return true;
    }
};
}
