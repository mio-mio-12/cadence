#pragma once
#include "scene/ImportedBody.h"

namespace scene::imported {
using BodyFingerChains=std::array<std::array<std::array<int,3>,5>,2>;
// Read existing anatomical aliases only; never publish them to native binders.
inline int semanticBodyBone(const Skeleton& rig,const std::string& name){
    const int native=nativeBodyBone(rig,name);if(native>=0)return native;
    const auto found=rig.boneByCanonicalName.find(name);
    return found!=rig.boneByCanonicalName.end()&&found->second<rig.bones.size()?static_cast<int>(found->second):-1;
}
inline BodyFingerChains bodyFingerChains(const Skeleton& rig){
    BodyFingerChains result;for(auto& hand:result)for(auto& digit:hand)digit.fill(-1);
    const auto family=bodyLayout(rig).family;
    const auto ancestor=[&](int child,int parent){for(std::size_t n=0;child>=0&&std::size_t(child)<rig.bones.size()&&n<rig.bones.size();++n){if(child==parent)return true;child=rig.bones[child].parent;}return false;};
    for(int side=0;side<2;++side){
        const std::string lr=side?"r_":"l_",cod=side?"ri":"le",cap=side?"R":"L";
        const int wrist=family==BodyFamily::Halo?nativeBodyBone(rig,lr+"hand"):(bipedBodyFamily(family)||family==BodyFamily::Source2)?bodyLayout(rig).hand[side]:semanticBodyBone(rig,"j_wrist_"+cod);
        if(wrist<0)continue;
        const std::array<const char*,5> names{"thumb","index","middle","ring","pinky"};
        for(int digit=0;digit<5;++digit){int parent=wrist;bool valid=true;
            const auto codStem="j_"+std::string(digit==2?"mid":names[digit])+"_"+cod+"_";
            const bool oneBased=semanticBodyBone(rig,codStem+"0")<0&&semanticBodyBone(rig,codStem+"1")>=0;
            for(int joint=0;joint<3;++joint){std::string name;
                if(family==BodyFamily::Halo)name=lr+names[digit]+"_"+(joint==0?"low":joint==1?"mid":"tip");
                else if(family==BodyFamily::Source2)name="finger_"+std::string(names[digit])+"_"+std::to_string(joint)+"_"+cap;
                else if(bipedBodyFamily(family)){const auto prefix=bodyFingerPrefix(family);name=std::string(prefix)+cap+bodyFingerSeparator(family)+"Finger"+std::to_string(digit)+(joint?std::to_string(joint):"");}
                else name=codStem+std::to_string(joint+(oneBased?1:0));
                int found=family==BodyFamily::None?semanticBodyBone(rig,name):nativeBodyBone(rig,name);
                if(found<0&&bipedBodyFamily(family))for(std::size_t b=0;b<rig.bones.size();++b)if(rig.bones[b].name.starts_with(name+"_")&&ancestor(static_cast<int>(b),parent)){if(found>=0){found=-1;break;}found=static_cast<int>(b);}
                // A shorter exported digit is valid; do not discard its real
                // joints merely because it has no third phalanx. Broken parent
                // relationships remain invalid, not partial-chain fallbacks.
                if(found<0)break;
                if(found==parent||!ancestor(found,parent)){valid=false;break;}
                result[side][digit][joint]=found;parent=found;
            }
            if(!valid)result[side][digit].fill(-1);
        }
    }
    return result;
}
}
