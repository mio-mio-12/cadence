#pragma once
#include "gameplay/IwPresentation.h"
#include <bit>
#include <unordered_map>

namespace gameplay::collision {
inline float stanceHeight(float standing,scene::Stance stance,float legacyStanding=72.f,float legacyCrouch=40.f,float legacyProne=20.f){
    if(!std::isfinite(standing)||standing<=0)standing=iw::kStandingActorHeight;
    const float selected=stance==scene::Stance::Prone?legacyProne:stance==scene::Stance::Crouch?legacyCrouch:legacyStanding;
    if(!std::isfinite(legacyStanding)||legacyStanding<=0||!std::isfinite(selected)||selected<=0)return standing;
    return standing*std::min(1.f,selected/legacyStanding);
}
// Assembly-time rest measurements, never current animation bounds. Storage and
// mesh descriptors detect ordinary replacement; explicit invalidate() is required
// after in-place vertex/bind edits that retain the same storage and descriptors.
class CharacterHeightCache {
    const scene::CastScene* owner_{};
    size_t signature_{};
    std::unordered_map<int,float> tops_;
    std::unordered_map<int,float> bottoms_;
    float commonTop_{},commonBottom_{},floor_{};
    bool commonValid_{};
    size_t rebuilds_{};
    static bool finite(scene::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    static size_t signature(const scene::CastScene& actor){
        size_t h=0;const auto add=[&](size_t v){h^=v+size_t(0x9e3779b9)+(h<<6)+(h>>2);};
        add(reinterpret_cast<size_t>(actor.skeleton.bones.data()));add(actor.skeleton.bones.size());
        add(actor.nativePoseFollowers.size());add(actor.bounds.valid);add(std::bit_cast<uint32_t>(actor.bounds.minimum.z));
        for(const auto& a:actor.attachments){add(a.boneIndex);for(float f:a.localMatrix().v)add(std::bit_cast<uint32_t>(f));if(a.boneIndex<actor.skeleton.bones.size())add(std::hash<std::string>{}(actor.skeleton.bones[a.boneIndex].name));}
        for(const auto& m:actor.meshes){add(reinterpret_cast<size_t>(m.vertices.data()));add(m.vertices.size());add(reinterpret_cast<size_t>(m.indices.data()));add(m.indices.size());add(m.actorVariant);add(m.attachmentIndex);add(m.viewmodelWeapon);add(m.skinned);for(float f:m.modelTransform.v)add(std::bit_cast<uint32_t>(f));}
        return h;
    }
    void rebuild(const scene::CastScene& actor){
        ++rebuilds_;tops_.clear();bottoms_.clear();commonValid_=false;
        floor_=actor.bounds.valid&&std::isfinite(actor.bounds.minimum.z)?actor.bounds.minimum.z:0.f;
        std::vector<scene::Transform> locals;for(const auto& b:actor.skeleton.bones)locals.push_back(b.restLocal);
        std::vector<scene::Mat4> rest;actor.globalPoseInto(locals,rest);
        const auto globals=rest;
        for(size_t i=0;i<rest.size();++i)rest[i]=rest[i]*actor.skeleton.bones[i].inverseBind;
        for(const auto& m:actor.meshes){if(m.viewmodelWeapon)continue;
            auto model=m.modelTransform;
            if(m.attachmentIndex>=0){
                if(m.skinned||static_cast<size_t>(m.attachmentIndex)>=actor.attachments.size())continue;
                const auto& a=actor.attachments[m.attachmentIndex];if(a.boneIndex>=globals.size())continue;
                const auto name=scene::canonicalName(actor.skeleton.bones[a.boneIndex].name);
                const bool head=name=="j_head"||name=="j_neck"||name=="j_helmet"||name=="tag_head"||name=="tag_helmet"||name=="head"||name=="neck"||name=="neck1"||name=="b_head"||name=="b_neck"||name=="bip01 head"||name=="bip01 neck"||name=="valvebiped.bip01_head1"||name=="valvebiped.bip01_neck1";
                if(!head)continue;
                model=globals[a.boneIndex]*a.localMatrix()*model;
            }
            for(const auto index:m.indices){if(index>=m.vertices.size())continue;const auto& v=m.vertices[index];auto p=v.position;
                if(m.skinned){p={};float sum=0;bool valid=true;for(int k=0;k<4;++k){const float w=v.weights[k];if(!std::isfinite(w)||w<0){valid=false;break;}if(w==0)continue;if(v.bones[k]>=rest.size()){valid=false;break;}p+=scene::transformPoint(rest[v.bones[k]],v.position)*w;sum+=w;}if(!valid||std::abs(sum-1.f)>.01f)continue;}
                p=scene::transformPoint(model,p);if(!finite(p))continue;
                if(m.actorVariant<0){if(!commonValid_||p.z>commonTop_)commonTop_=p.z;if(!commonValid_||p.z<commonBottom_)commonBottom_=p.z;commonValid_=true;}
                else{const auto it=tops_.find(m.actorVariant);if(it==tops_.end()){tops_.emplace(m.actorVariant,p.z);bottoms_.emplace(m.actorVariant,p.z);}else{it->second=std::max(it->second,p.z);bottoms_[m.actorVariant]=std::min(bottoms_[m.actorVariant],p.z);}}
            }
        }
    }
public:
    void invalidate(){owner_=nullptr;}
    size_t rebuildCount()const{return rebuilds_;}
    float standingHeight(const scene::CastScene& actor,int variant=-1,float scale=1.f,float extraGroundLift=0.f,float fallback=iw::kStandingActorHeight){
        if(!std::isfinite(fallback)||fallback<=0)fallback=iw::kStandingActorHeight;
        if(!std::isfinite(scale)||scale<=0||!std::isfinite(extraGroundLift))return fallback;
        const auto key=signature(actor);if(owner_!=&actor||key!=signature_){rebuild(actor);owner_=&actor;signature_=key;}
        bool valid=commonValid_;float top=commonTop_,bottom=commonBottom_;
        for(const auto& [id,value]:tops_)if(variant<0||id==variant){if(!valid||value>top)top=value;if(!valid||bottoms_.at(id)<bottom)bottom=bottoms_.at(id);valid=true;}
        const float height=(top-floor_)*scale+extraGroundLift;
        return valid&&top-bottom>1e-4f&&std::isfinite(height)&&height>0?height:fallback;
    }
};
}
