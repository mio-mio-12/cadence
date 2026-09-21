#include "scene/T6Magazine.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace scene {
namespace {
bool solve(double a[6][7],double x[6]) {
    for(int k=0;k<6;++k){
        int pivot=k;for(int i=k+1;i<6;++i)if(std::abs(a[i][k])>std::abs(a[pivot][k]))pivot=i;
        // Stationary or single-axis wrist motion cannot determine a socket.
        if(std::abs(a[pivot][k])<1e-4)return false;
        for(int j=k;j<7;++j)std::swap(a[k][j],a[pivot][j]);
        const double divisor=a[k][k];for(int j=k;j<7;++j)a[k][j]/=divisor;
        for(int i=0;i<6;++i)if(i!=k){const double factor=a[i][k];for(int j=k;j<7;++j)a[i][j]-=factor*a[k][j];}
    }
    for(int i=0;i<6;++i){x[i]=a[i][6];if(!std::isfinite(x[i]))return false;}return true;
}
}
bool hasSeparateT6Magazine(const CastScene& scene) {
    for(const auto& part:scene.rigParts){const auto name=canonicalName(part.name);
        if((name.starts_with("t6_attach_mag_")||name.starts_with("t6_attach_fastmag_"))&&name.find("_view")!=std::string::npos)
            for(auto root:part.rootBones)if(root<scene.skeleton.bones.size()&&canonicalName(scene.skeleton.bones[root].name)=="tag_clip")return true;
    }return false;
}
std::optional<T6MagazineCalibration> inferT6MagazineMount(const CastScene& native) {
    if(!hasSeparateT6Magazine(native))return {};
    const auto& bones=native.skeleton.boneByCanonicalName;
    const auto gi=bones.find("j_gun"),mi=bones.find("tag_clip");if(gi==bones.end()||mi==bones.end())return {};
    const auto mag=mi->second,gun=gi->second;const auto bind=native.skeleton.bones[mag].restLocal.position;
    struct Candidate {Vec3 mount;float residual;std::string source;};std::vector<Candidate> candidates;std::vector<Vec3> incoming;
    for(std::size_t ai=0;ai<native.animations.size();++ai){
        const auto& clip=native.animations[ai];const auto name=canonicalName(clip.sourceName);
        if(!name.starts_with("viewmodel_")||name.find("reload")==std::string::npos||clip.durationFrames<16||clip.durationFrames>600)continue;
        std::vector<std::vector<Mat4>> poses;poses.reserve(clip.durationFrames+1);
        for(unsigned f=0;f<=clip.durationFrames;++f){auto pose=native.samplePose(ai,float(f));const auto inv=inverseAffine(pose[gun]);for(auto& m:pose)m=inv*m;poses.push_back(std::move(pose));}
        // A parked embedded spare supplies the seated station after insertion.
        for(const auto boneName:{"tag_clip1","tag_clip_full"})if(const auto it=bones.find(boneName);it!=bones.end()){
            const auto b=it->second;if(native.skeleton.bones[b].parent!=native.skeleton.bones[mag].parent)continue;
            const auto end=transformPoint(poses.back()[b],{}),start=transformPoint(poses.front()[b],{});
            if(length(end)<80&&length(end-start)>25&&length(end-transformPoint(poses[poses.size()-5][b],{}))<.05f&&length(native.skeleton.bones[b].restLocal.position-bind)>25)incoming.push_back(end);
        }
        for(const auto boneName:{"j_wrist_le","j_wrist_ri"})if(const auto it=bones.find(boneName);it!=bones.end()){
            const auto wrist=it->second;constexpr int window=16;
            for(std::size_t begin=0;begin+window<=poses.size();++begin){
                if(length(transformPoint(poses[begin][mag],{})-transformPoint(poses[begin+window-1][mag],{}))<3)continue;
                double a[6][7]{},x[6]{};
                // magTranslation + missingMount = wristTranslation + wristRotation * fixedGrip.
                for(std::size_t f=begin;f<begin+window;++f)for(int k=0;k<3;++k){
                    double row[6]{};row[k]=1;for(int j=0;j<3;++j)row[3+j]=-poses[f][wrist].v[j*4+k];
                    const double y=poses[f][wrist].v[12+k]-poses[f][mag].v[12+k];
                    for(int i=0;i<6;++i){for(int j=0;j<6;++j)a[i][j]+=row[i]*row[j];a[i][6]+=row[i]*y;}
                }
                if(!solve(a,x))continue;
                const Vec3 mount=bind+Vec3{float(x[0]),float(x[1]),float(x[2])},grip{float(x[3]),float(x[4]),float(x[5])};
                if(length(mount)>100||length(grip)>35)continue;
                double error=0;for(std::size_t f=begin;f<begin+window;++f)for(int k=0;k<3;++k){double e=poses[f][mag].v[12+k]+x[k]-poses[f][wrist].v[12+k];for(int j=0;j<3;++j)e-=poses[f][wrist].v[j*4+k]*x[3+j];error+=e*e;}
                const float rms=float(std::sqrt(error/window));if(rms<=.25f)candidates.push_back({mount,rms,name});
            }
        }
    }
    if(!candidates.empty()){
        std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.residual<b.residual;});
        const auto best=candidates.front();Vec3 sum{};float weight=0,residual=0;std::size_t support=0;
        for(const auto& c:candidates)if(length(c.mount-best.mount)<.5f){const float w=1/std::max(.005f,c.residual);sum+=c.mount*w;weight+=w;residual+=c.residual*w;++support;}
        // Two different reload sources agreeing tightly are stronger evidence
        // than overlapping windows from one clip. Keep the original bounds.
        std::set<std::string> independent;
        for(const auto& c:candidates)if(length(c.mount-best.mount)<.1f)independent.insert(c.source);
        if(support>=3||independent.size()>=2)return T6MagazineCalibration{sum/weight,residual/weight,support,false};
    }
    // A single endpoint could be a hidden spare; require independent reloads.
    if(incoming.size()>=2){Vec3 sum{};for(const auto& p:incoming){if(length(p-incoming.front())>.1f)return {};sum+=p;}return T6MagazineCalibration{sum/float(incoming.size()),0,incoming.size(),true};}
    return {};
}
bool applyT6MagazineMount(CastScene& target,const T6MagazineCalibration& mount){
    if(!hasSeparateT6Magazine(target)||!std::isfinite(length(mount.position)))return false;
    auto& bone=target.skeleton.bones[target.skeleton.boneByCanonicalName.at("tag_clip")];
    bone.restLocal.position=mount.position;bone.absoluteTranslationOffset=mount.position;
    // Preserve the attachment's zero-space inverse bind so the recovered
    // socket moves its geometry rather than cancelling out during skinning.
    return true;
}
}
