#pragma once
#include "scene/ImportedBody.h"

namespace scene::imported {
using BodyFingerChains=std::array<std::array<std::array<int,3>,5>,2>;
inline BodyFingerChains bodyFingerChains(const Skeleton& rig){
    BodyFingerChains result;for(auto& hand:result)for(auto& digit:hand)digit.fill(-1);
    const auto family=bodyLayout(rig).family;
    const auto ancestor=[&](int child,int parent){for(std::size_t n=0;child>=0&&std::size_t(child)<rig.bones.size()&&n<rig.bones.size();++n){if(child==parent)return true;child=rig.bones[child].parent;}return false;};
    for(int side=0;side<2;++side){
        const std::string lr=side?"r_":"l_",cod=side?"ri":"le",cap=side?"R":"L";
        const int wrist=family==BodyFamily::Halo?nativeBodyBone(rig,lr+"hand"):sourceBodyFamily(family)?bodyLayout(rig).hand[side]:nativeBodyBone(rig,"j_wrist_"+cod);
        if(wrist<0)continue;
        const std::array<const char*,5> names{"thumb","index","middle","ring","pinky"};
        for(int digit=0;digit<5;++digit){int parent=wrist;bool valid=true;
            const auto codStem="j_"+std::string(digit==2?"mid":names[digit])+"_"+cod+"_";
            const bool oneBased=nativeBodyBone(rig,codStem+"0")<0&&nativeBodyBone(rig,codStem+"1")>=0;
            for(int joint=0;joint<3;++joint){std::string name;
                if(family==BodyFamily::Halo)name=lr+names[digit]+"_"+(joint==0?"low":joint==1?"mid":"tip");
                else if(sourceBodyFamily(family)){const auto prefix=family==BodyFamily::Source?"ValveBiped.Bip01_":family==BodyFamily::Cso2?"CSO2_BipM ":"Bip01 ";name=std::string(prefix)+cap+(family==BodyFamily::Source?"_":" ")+"Finger"+std::to_string(digit)+(joint?std::to_string(joint):"");}
                else name=codStem+std::to_string(joint+(oneBased?1:0));
                int found=nativeBodyBone(rig,name);
                if(found<0&&sourceBodyFamily(family))for(std::size_t b=0;b<rig.bones.size();++b)if(rig.bones[b].name.starts_with(name+"_")&&ancestor(static_cast<int>(b),parent)){if(found>=0){found=-1;break;}found=static_cast<int>(b);}
                if(found<0||found==parent||!ancestor(found,parent)){valid=false;break;}
                result[side][digit][joint]=found;parent=found;
            }
            if(!valid)result[side][digit].fill(-1);
        }
    }
    return result;
}
}
