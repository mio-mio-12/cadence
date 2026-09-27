#pragma once
#include "scene/CastScene.h"
#include "scene/ImportedViewRetarget.h"
#include "assets/ImportedGamePolicy.h"
#include <optional>
#include <span>

namespace scene::imported::world_grip {
// This adapter never guesses a grip from a bounding box or rescales a gun to
// fit a character. The donor is a native body with an authored weapon socket;
// source and destination bodies/poses must already use centimetres.
struct Mount { std::size_t bone{}; Mat4 transform{Mat4::identity()}; };
inline int bone(const Skeleton& s,std::initializer_list<const char*> names){
    for(const auto* name:names){const auto it=s.boneByName.find(name);if(it!=s.boneByName.end()&&it->second<s.bones.size())return static_cast<int>(it->second);}
    return -1;
}
struct Palm {int wrist{-1},index{-1},middle{-1},pinky{-1},thumb{-1};};
inline Palm rightPalm(const Skeleton& s){
    Palm p;
    p.wrist=bone(s,{"j_wrist_ri","r_hand","ValveBiped.Bip01_R_Hand","CSO2_BipM R Hand","Bip01 R Hand","R Hand","b_RightHand"});
    p.index=bone(s,{"j_index_ri_0","j_index_ri_1","r_index_low","ValveBiped.Bip01_R_Finger1","CSO2_BipM R Finger1","Bip01 R Finger1","R Index1","b_RightIndex1"});
    p.middle=bone(s,{"j_mid_ri_0","j_mid_ri_1","r_middle_low","ValveBiped.Bip01_R_Finger2","CSO2_BipM R Finger2","R Middle1","b_RightMiddle1"});
    p.pinky=bone(s,{"j_pinky_ri_0","j_pinky_ri_1","r_pinky_low","ValveBiped.Bip01_R_Finger4","CSO2_BipM R Finger4","R Little1","b_RightPinky1"});
    p.thumb=bone(s,{"j_thumb_ri_0","j_thumb_ri_1","r_thumb_low","ValveBiped.Bip01_R_Finger0","CSO2_BipM R Finger0","Bip01 R Finger0","R Thumb1","b_RightThumb1"});
    // Some GoldSrc characters suffix finger names with their model variant.
    // Only accept a base finger directly parented to the matched wrist, never
    // Finger11/12 knuckles or a similarly named accessory skeleton.
    if(p.wrist>=0&&s.bones[p.wrist].name=="Bip01 R Hand"){
        const auto variant=[&](int digit){const auto stem=std::string("Bip01 R Finger")+char('0'+digit);for(std::size_t i=0;i<s.bones.size();++i){const auto& b=s.bones[i];if(b.parent==p.wrist&&(b.name==stem||b.name.starts_with(stem+"_")))return static_cast<int>(i);}return -1;};
        if(p.index<0)p.index=variant(1);if(p.middle<0)p.middle=variant(2);
        if(p.pinky<0)p.pinky=variant(4);if(p.thumb<0)p.thumb=variant(0);
    }
    return p;
}
inline bool finite(const Mat4& m){for(auto x:m.v)if(!std::isfinite(x))return false;return true;}
inline std::optional<Mat4> rigid(const Mat4& m){
    if(!finite(m))return {};
    Vec3 p,s;Quat q;decomposeAffine(m,p,q,s);
    if(std::min({s.x,s.y,s.z})<1e-6f)return {};
    return trs(p,q,{1,1,1});
}
inline std::optional<Mat4> palmFrame(const Skeleton& s,std::span<const Mat4> pose){
    const auto p=rightPalm(s);
    if(p.wrist<0||p.index<0||pose.size()!=s.bones.size())return {};
    const auto pos=[&](int i){return transformPoint(pose[static_cast<std::size_t>(i)],{});};
    const auto origin=pos(p.wrist),forward=pos(p.middle>=0?p.middle:p.index)-origin;
    // Full rigs use index-to-pinky handedness. Two-finger legacy rigs use
    // their authored thumb/index plane, not an unrelated foreign bind axis.
    const auto normal=p.pinky>=0?cross(pos(p.index)-origin,pos(p.pinky)-origin):
        p.thumb>=0?cross(pos(p.thumb)-origin,pos(p.index)-origin):Vec3{};
    if(length(forward)<1e-5f||length(normal)<1e-5f)return {};
    const auto x=normalize(forward),y=normalize(cross(normal,x)),z=normalize(cross(x,y));
    if(length(y)<.99f||length(z)<.99f)return {};
    auto frame=Mat4::identity();
    frame.v[0]=x.x;frame.v[1]=x.y;frame.v[2]=x.z;
    frame.v[4]=y.x;frame.v[5]=y.y;frame.v[6]=y.z;
    frame.v[8]=z.x;frame.v[9]=z.y;frame.v[10]=z.z;
    frame.v[12]=origin.x;frame.v[13]=origin.y;frame.v[14]=origin.z;
    return finite(frame)?std::optional<Mat4>(frame):std::nullopt;
}
inline std::vector<Mat4> restPose(const Skeleton& s){std::vector<Mat4> p;p.reserve(s.bones.size());for(const auto& b:s.bones)p.push_back(b.restGlobal);return p;}
inline std::optional<Mat4> gripSpanFrame(Vec3 trigger,Vec3 support,Vec3 up={0,0,1}){
    if(up.z<0)up=up*-1.f;
    const auto x=normalize(support-trigger),y=normalize(cross(up,x));
    if(length(support-trigger)<1e-3f||length(y)<.9f)return {};
    const auto z=normalize(cross(x,y));auto m=translation(trigger);
    m.v[0]=x.x;m.v[1]=x.y;m.v[2]=x.z;
    m.v[4]=y.x;m.v[5]=y.y;m.v[6]=y.z;
    m.v[8]=z.x;m.v[9]=z.y;m.v[10]=z.z;return m;
}
// Freeze a native first-person hold, retaining gun geometry only. Match palm
// frames rather than the unrelated origins of view and world weapon exports.
struct ViewHoldConvention {
    Mat4 camera{Mat4::identity()};
    std::array<Mat4,2> span{},socket{}; // wrist origin, then finger-contact origin
    bool spanValid{},socketValid{},contacts{};
};
// Derived affine calibration only: no model, vertices, curves, or bone names
// need to be retained in the resulting game-independent convention.
inline std::optional<ViewHoldConvention> deriveViewHoldConvention(const Skeleton& donor,std::span<const Mat4> pose){
    if(pose.size()!=donor.bones.size())return {};
    const auto right=rightPalm(donor);const auto palm=palmFrame(donor,restPose(donor));
    if(right.wrist<0||!palm||!rigid(pose[right.wrist]))return {};
    const auto c=inverseAffine(*palm)*donor.bones[right.wrist].restGlobal*inverseAffine(pose[right.wrist]);
    ViewHoldConvention result;const auto wrist=transformPoint(pose[right.wrist],{});result.camera=c*translation(wrist);
    const int left=bone(donor,{"j_wrist_le","l_hand","ValveBiped.Bip01_L_Hand","CSO2_BipM L Hand","Bip01 L Hand"});
    if(left>=0){
        const int middle=bone(donor,{"j_mid_le_0","j_mid_le_1","l_middle_low","ValveBiped.Bip01_L_Finger2","CSO2_BipM L Finger2"});
        result.contacts=right.middle>=0&&middle>=0;
        const auto span=gripSpanFrame(wrist,transformPoint(pose[left],{}));
        const int socket=bone(donor,{"tag_weapon_right"});const auto socketFrame=socket>=0?rigid(pose[socket]):std::optional<Mat4>{};
        result.spanValid=bool(span);result.socketValid=bool(socketFrame);
        for(int contact=0;contact<2;++contact){const auto origin=contact&&result.contacts?(wrist+transformPoint(pose[right.middle],{}))*.5f:wrist;
            const auto putOrigin=[&](Mat4 m){m.v[12]=origin.x;m.v[13]=origin.y;m.v[14]=origin.z;return c*m;};
            if(span)result.span[contact]=putOrigin(*span);if(socketFrame)result.socket[contact]=putOrigin(*socketFrame);
        }
    }
    if(!finite(result.camera))return {};
    for(int i=0;i<2;++i)if(result.spanValid&&!finite(result.span[i])||result.socketValid&&!finite(result.socket[i]))return {};
    return result;
}
inline std::optional<Mount> fromViewHold(CastScene& view,std::span<const Mat4> pose,
                                       const Skeleton& target,float units,const Skeleton* holdSkeleton=nullptr,std::span<const Mat4> holdPose={},bool rearGrip=false,const ViewHoldConvention* convention=nullptr){
    if(pose.size()!=view.skeleton.bones.size())return {};
    const auto arms=viewLayout(view);
    auto source=view.skeleton;
    // Normalized Source rifle exports can label the forward support palm R.
    // For a two-handed long gun, the rear authored grip is the trigger hand.
    // Do not apply this to pistols, dual weapons, melee, or native world sockets.
    int grip=1;
    if(rearGrip&&arms.arms[0].wrist>=0&&arms.arms[1].wrist>=0&&
       source.bones[arms.arms[0].wrist].name=="cs_hand_L.palm"&&
       source.bones[arms.arms[1].wrist].name=="cs_hand_R.palm"&&
       transformPoint(pose[arms.arms[0].wrist],{}).x+1.f<transformPoint(pose[arms.arms[1].wrist],{}).x)
        grip=0;
    const auto& right=arms.arms[grip];
    if(right.wrist<0)return {};
    const auto alias=[&](const char* name,int index){if(index>=0)source.boneByName[name]=static_cast<std::size_t>(index);};
    alias("j_wrist_ri",right.wrist);alias("j_index_ri_0",right.fingers[1][0]);
    alias("j_mid_ri_0",right.fingers[2][0]);alias("j_pinky_ri_0",right.fingers[4][0]);alias("j_thumb_ri_0",right.fingers[0][0]);
    auto native=palmFrame(source,restPose(source));
    if(native)native=pose[right.wrist]*inverseAffine(source.bones[right.wrist].restGlobal)*(*native);
    if(!native)native=rigid(pose[right.wrist]);
    const auto targetHand=rightPalm(target);
    if(targetHand.wrist<0)return {};
    const auto targetPose=restPose(target);auto destination=palmFrame(target,targetPose);
    if(!destination)destination=rigid(targetPose[targetHand.wrist]);
    if(!native||!destination)return {};
    std::vector<bool> armBones(source.bones.size());
    for(const auto& arm:arms.arms){
        for(int b:{arm.shoulder,arm.elbow,arm.wrist})if(b>=0)armBones[b]=true;
        for(const auto& finger:arm.fingers)for(int b:finger)if(b>=0)armBones[b]=true;
    }
    std::erase_if(view.meshes,[&](const auto& mesh){
        if(explicitHandMesh(mesh.name))return true;
        if(mesh.vertices.empty())return true;
        return std::all_of(mesh.vertices.begin(),mesh.vertices.end(),[&](const auto& v){
            float total{},arm{};for(int k=0;k<4;++k){total+=v.weights[k];if(v.bones[k]<armBones.size()&&armBones[v.bones[k]])arm+=v.weights[k];}
            return total>0&&arm>=total*.999f;
        });
    });
    if(view.meshes.empty())return {};
    for(auto& mesh:view.meshes){
        if(mesh.skinned)for(auto& v:mesh.vertices){Vec3 p{},n{};float weight{};
            for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){
                const auto m=pose[v.bones[k]]*view.skeleton.bones[v.bones[k]].inverseBind;
                p=p+transformPoint(m,v.position)*v.weights[k];
                n=n+(transformPoint(m,v.normal)-transformPoint(m,{}))*v.weights[k];weight+=v.weights[k];
            }
            if(weight>0){v.position=p/weight;v.normal=normalize(n);}
        }
        mesh.skinned=false;mesh.viewmodelWeapon=false;
    }
    // Scaling after removal of the source palm translation keeps the grip
    // fixed and converts Source inches once, without resizing to hand size.
    auto transform=inverseAffine(targetPose[targetHand.wrist])*(*destination)*scale({units,units,units})*inverseAffine(*native);
    if(holdSkeleton&&holdPose.size()==holdSkeleton->bones.size()){
        const auto donor=rightPalm(*holdSkeleton);const auto donorPalm=palmFrame(*holdSkeleton,restPose(*holdSkeleton));
        if(donor.wrist>=0&&donorPalm){
            // Imported first-person exports are camera-aligned (+X forward,
            // +Z up). Register that orientation against an authored body hold,
            // not finger curl axes, which differ between universal hand rigs.
            transform=inverseAffine(targetPose[targetHand.wrist])*(*destination)*inverseAffine(*donorPalm)*holdSkeleton->bones[donor.wrist].restGlobal*
                inverseAffine(holdPose[donor.wrist])*translation(transformPoint(holdPose[donor.wrist],{}))*scale({units,units,units})*translation(transformPoint(pose[right.wrist],{})*-1.f);
            const auto support=arms.arms[1-grip].wrist;
            const int donorSupport=bone(*holdSkeleton,{"j_wrist_le","l_hand","ValveBiped.Bip01_L_Hand","CSO2_BipM L Hand","Bip01 L Hand"});
            if(rearGrip&&support>=0&&donorSupport>=0){
                const auto contact=[](std::span<const Mat4> p,int wrist,int middle){
                    const auto origin=transformPoint(p[wrist],{});
                    return middle>=0?(origin+transformPoint(p[middle],{}))*.5f:origin;
                };
                const int leftMiddle=bone(*holdSkeleton,{"j_mid_le_0","j_mid_le_1","l_middle_low","ValveBiped.Bip01_L_Finger2","CSO2_BipM L Finger2"});
                const bool contacts=right.fingers[2][0]>=0&&arms.arms[1-grip].fingers[2][0]>=0&&donor.middle>=0&&leftMiddle>=0;
                const auto sourceTrigger=contact(pose,right.wrist,contacts?right.fingers[2][0]:-1);
                const auto heldTrigger=contact(holdPose,donor.wrist,contacts?donor.middle:-1);
                auto sourceFrame=gripSpanFrame(transformPoint(pose[right.wrist],{}),transformPoint(pose[support],{}));
                auto heldFrame=gripSpanFrame(transformPoint(holdPose[donor.wrist],{}),transformPoint(holdPose[donorSupport],{}));
                // Native barrel and body weapon-socket axes preserve the
                // authored firing angle; camera-forward and palm spans do not.
                const auto socket=bone(*holdSkeleton,{"tag_weapon_right"});
                if(socket>=0&&!view.muzzleAnchors.empty()){
                    const auto barrel=view.muzzleAnchors.front().bone;
                    if(barrel<pose.size()){
                        const auto motion=pose[barrel]*source.bones[barrel].inverseBind;
                        const auto o=transformPoint(motion,{});
                        sourceFrame=gripSpanFrame(sourceTrigger,sourceTrigger+transformPoint(motion,{0,-1,0})-o,transformPoint(motion,{0,0,1})-o);
                        heldFrame=rigid(holdPose[socket]);
                    }
                }
                if(sourceFrame&&heldFrame){
                    sourceFrame->v[12]=sourceTrigger.x;sourceFrame->v[13]=sourceTrigger.y;sourceFrame->v[14]=sourceTrigger.z;
                    heldFrame->v[12]=heldTrigger.x;heldFrame->v[13]=heldTrigger.y;heldFrame->v[14]=heldTrigger.z;
                }
                if(sourceFrame&&heldFrame)
                    transform=inverseAffine(targetPose[targetHand.wrist])*(*destination)*inverseAffine(*donorPalm)*holdSkeleton->bones[donor.wrist].restGlobal*
                        inverseAffine(holdPose[donor.wrist])*(*heldFrame)*scale({units,units,units})*inverseAffine(*sourceFrame);
            }
        }
    }
    if(convention){
        const auto targetFrame=inverseAffine(targetPose[targetHand.wrist])*(*destination);
        transform=targetFrame*convention->camera*scale({units,units,units})*translation(transformPoint(pose[right.wrist],{})*-1.f);
        const int support=arms.arms[1-grip].wrist;
        if(rearGrip&&support>=0){
            const bool contacts=convention->contacts&&right.fingers[2][0]>=0&&arms.arms[1-grip].fingers[2][0]>=0;
            const auto wrist=transformPoint(pose[right.wrist],{});
            const auto trigger=contacts?(wrist+transformPoint(pose[right.fingers[2][0]],{}))*.5f:wrist;
            auto sourceFrame=gripSpanFrame(wrist,transformPoint(pose[support],{}));
            bool heldValid=convention->spanValid;auto heldFrame=convention->span[contacts?1:0];
            if(convention->socketValid&&!view.muzzleAnchors.empty()){
                const auto barrel=view.muzzleAnchors.front().bone;
                if(barrel<pose.size()){const auto motion=pose[barrel]*source.bones[barrel].inverseBind;const auto o=transformPoint(motion,{});
                    sourceFrame=gripSpanFrame(trigger,trigger+transformPoint(motion,{0,-1,0})-o,transformPoint(motion,{0,0,1})-o);
                    heldFrame=convention->socket[contacts?1:0];heldValid=true;
                }
            }
            if(sourceFrame&&heldValid){sourceFrame->v[12]=trigger.x;sourceFrame->v[13]=trigger.y;sourceFrame->v[14]=trigger.z;transform=targetFrame*heldFrame*scale({units,units,units})*inverseAffine(*sourceFrame);}
        }
    }
    if(!finite(transform))return {};
    // Static world placeholders keep sockets in the same frozen pose as the
    // baked barrel geometry. No source animation or skeleton file is changed.
    for(std::size_t i=0;i<view.skeleton.bones.size();++i)view.skeleton.bones[i].restGlobal=pose[i];
    return Mount{static_cast<std::size_t>(targetHand.wrist),transform};
}
struct NativeSocket {int donor{-1},weapon{-1};float weaponUnits{1};};
inline int explicitGunAnchor(const Skeleton& skeleton){
    // dew2cast separates first-person weapon-only rigs from shared hands and
    // namespaces their authored gun root. This is not an arbitrary suffix or
    // root-bone heuristic; unknown namespaces and similarly named helpers fail.
    return bone(skeleton,{"gun","dew2cast_weapon__gun"});
}
// Last-resort visible placeholder, not an anatomical calibration. Both ends
// must be explicitly authored weapon anchors; an arbitrary scene root or
// muzzle cannot establish a grip. Work transactionally so rejection cannot
// leave partially baked geometry for another attachment path.
inline std::optional<Mount> fromAuthoredAnchor(CastScene& view,std::span<const Mat4> pose,
                                              const Skeleton& target,float units,std::string& error){
    error.clear();
    const auto reject=[&](const char* message)->std::optional<Mount>{error=message;return {};};
    if(pose.size()!=view.skeleton.bones.size()||!std::isfinite(units)||units<=0)return reject("Invalid placeholder pose or units");
    int source=bone(view.skeleton,{"tag_weapon","j_gun","ValveBiped.weapon_bone","CSO2_W_bone"});
    if(source<0)source=explicitGunAnchor(view.skeleton);
    const int destination=bone(target,{"tag_weapon_right","b_RightWeapon","dew2cast_socket__right_hand__0","ValveBiped.weapon_bone","CSO2_W_bone"});
    if(source<0||destination<0)return reject("No authored source/target weapon anchor for a safe placeholder");
    if(!rigid(pose[source])||!rigid(target.bones[destination].restGlobal))return reject("Invalid authored weapon anchor transform");
    const auto layout=viewLayout(view);std::vector<bool> arms(view.skeleton.bones.size());
    for(std::size_t i=0;i<arms.size();++i)arms[i]=assets::imported::lower(view.skeleton.bones[i].name).starts_with("cs_hand_");
    for(const auto& arm:layout.arms){for(const int b:{arm.shoulder,arm.elbow,arm.wrist})if(b>=0)arms[b]=true;for(const auto& finger:arm.fingers)for(const int b:finger)if(b>=0)arms[b]=true;}
    // Sleeves and twist helpers may have arbitrary names. Follow ancestry,
    // stopping at the explicit weapon anchor: a gun may itself be parented
    // to the wrist, but that does not make its receiver an arm mesh.
    const auto armRoots=arms;
    for(std::size_t i=0;i<arms.size();++i){std::size_t b=i;for(std::size_t depth=0;b<arms.size()&&depth<arms.size();++depth){if(b==std::size_t(source))break;if(armRoots[b]){arms[i]=true;break;}const auto parent=view.skeleton.bones[b].parent;if(parent<0)break;b=std::size_t(parent);}}
    const auto belowSource=[&](std::size_t b){for(std::size_t depth=0;b<view.skeleton.bones.size()&&depth<view.skeleton.bones.size();++depth){if(b==std::size_t(source))return true;const auto parent=view.skeleton.bones[b].parent;if(parent<0)return false;b=std::size_t(parent);}return false;};
    auto prepared=view;prepared.meshes.clear();
    for(const auto& mesh:view.meshes){
        if(explicitHandMesh(mesh.name)||mesh.vertices.empty())continue;
        // Unskinned vertices carry default bone-zero weights; those are not
        // evidence that an otherwise unclassified mesh belongs to the gun.
        bool armMesh=false,owned=mesh.skinned;
        for(const auto& vertex:mesh.vertices){
            if(!std::isfinite(vertex.position.x)||!std::isfinite(vertex.position.y)||!std::isfinite(vertex.position.z)||!std::isfinite(vertex.normal.x)||!std::isfinite(vertex.normal.y)||!std::isfinite(vertex.normal.z)||!finite(mesh.modelTransform))return reject("Nonfinite placeholder geometry");
            float total{};
            if(mesh.skinned)for(int k=0;k<4;++k){const float weight=vertex.weights[k];if(!std::isfinite(weight)||weight<0)return reject("Invalid placeholder skin weights");if(weight==0)continue;
                const auto b=vertex.bones[k];if(b>=pose.size()||!finite(pose[b])||!finite(view.skeleton.bones[b].inverseBind))return reject("Invalid placeholder skin transform");
                armMesh=armMesh||arms[b];owned=owned&&belowSource(b);total+=weight;
            }
            if(mesh.skinned&&total<=0)return reject("Missing placeholder skin weights");
            if(total<=0)owned=false;
        }
        if(armMesh||(!mesh.viewmodelWeapon&&!owned))continue;
        for(const auto index:mesh.indices)if(index>=mesh.vertices.size())return reject("Invalid placeholder triangle index");
        auto baked=mesh;
        if(baked.skinned)for(auto& vertex:baked.vertices){Vec3 p{},n{};float total{};for(int k=0;k<4;++k)if(vertex.weights[k]>0){const auto b=vertex.bones[k];const auto m=pose[b]*view.skeleton.bones[b].inverseBind;p=p+transformPoint(m,vertex.position)*vertex.weights[k];n=n+(transformPoint(m,vertex.normal)-transformPoint(m,{}))*vertex.weights[k];total+=vertex.weights[k];}vertex.position=p/total;vertex.normal=normalize(n);}
        baked.skinned=false;baked.viewmodelWeapon=false;prepared.meshes.push_back(std::move(baked));
    }
    if(prepared.meshes.empty())return reject("No safely identified weapon geometry for placeholder");
    const auto transform=scale({units,units,units})*inverseAffine(pose[source]);
    if(!finite(transform))return reject("Invalid placeholder attachment transform");
    for(std::size_t i=0;i<pose.size();++i)prepared.skeleton.bones[i].restGlobal=pose[i];
    view=std::move(prepared);return Mount{static_cast<std::size_t>(destination),transform};
}
inline int authoredEldRightHand(const Skeleton& skeleton,int gun){
    const int marker=bone(skeleton,{"dew2cast_socket__right_hand__0"});
    if(gun<0||marker<0||gun==marker)return -1;
    const auto& m=skeleton.bones[marker].restGlobal;
    if(!finite(m))return -1;
    const Vec3 x{m.v[0],m.v[1],m.v[2]},y{m.v[4],m.v[5],m.v[6]},z{m.v[8],m.v[9],m.v[10]};
    // This explicit socket is a proper rigid frame, not reflected art or a
    // scale/shear correction. Preserve the established gun fallback otherwise.
    if(std::abs(dot(x,x)-1)>1e-3f||std::abs(dot(y,y)-1)>1e-3f||std::abs(dot(z,z)-1)>1e-3f||
       std::abs(dot(x,y))>1e-3f||std::abs(dot(x,z))>1e-3f||std::abs(dot(y,z))>1e-3f||
       std::abs(dot(cross(x,y),z)-1)>1e-3f||std::abs(m.v[3])+std::abs(m.v[7])+std::abs(m.v[11])+std::abs(m.v[15]-1)>1e-3f)return -1;
    bool descendant=false;int parent=skeleton.bones[marker].parent;
    // Continue above gun as well: a corrupt cycle through gun is not a valid
    // descendant chain. The bounded walk also handles unordered helper bones.
    for(std::size_t steps=0;steps<skeleton.bones.size()&&parent>=0;++steps){
        if(static_cast<std::size_t>(parent)>=skeleton.bones.size())return -1;
        if(parent==gun)descendant=true;
        parent=skeleton.bones[parent].parent;
    }
    return descendant&&parent==-1?marker:-1;
}
inline NativeSocket nativeSocket(std::string_view game,const CastScene& world,const Skeleton& donor){
    NativeSocket n;
    if(game=="eldewrito"){
        n.donor=bone(donor,{"dew2cast_socket__right_hand__0"});n.weapon=explicitGunAnchor(world.skeleton);
        // buildScene has already normalized embedded dew2cast documents.
        if(world.importedTranslationScale!=304.8f)return {};
        if(const int grip=authoredEldRightHand(world.skeleton,n.weapon);grip>=0)n.weapon=grip;
    }else if(game=="css"){
        n.donor=bone(donor,{"ValveBiped.weapon_bone"});n.weapon=bone(world.skeleton,{"ValveBiped.weapon_bone"});n.weaponUnits=2.54f;
    }else if(game=="cso2"){
        n.donor=bone(donor,{"CSO2_W_bone"});n.weapon=bone(world.skeleton,{"CSO2_W_bone"});n.weaponUnits=2.54f;
        if(n.weapon<0){n.donor=bone(donor,{"CSO2_BipM R Hand"});n.weapon=bone(world.skeleton,{"CSO2_BipM R Hand"});}
    }else if(game=="bo2"){
        n.donor=bone(donor,{"tag_weapon_right"});n.weapon=bone(world.skeleton,{"tag_weapon","j_gun"});
    }
    // GoldSrc w_ exports are dropped art with unrelated roots. A complete
    // native held reference is required; never invent an offset from its name.
    return n;
}
inline std::optional<Mount> fromNativeSocket(std::string_view weaponGame,const CastScene& world,
    const Skeleton& donor,std::span<const Mat4> donorPose,const Skeleton& target,std::string& error){
    error.clear();
    if(weaponGame=="cs2"){error="CS2 is outside imported world-grip conversion";return {};}
    const auto source=nativeSocket(weaponGame,world,donor);const auto targetPalm=rightPalm(target),sourcePalm=rightPalm(donor);
    if(source.donor<0||source.weapon<0){error="No verified native body/world weapon socket pair";return {};}
    if(targetPalm.wrist<0||sourcePalm.wrist<0||donorPose.size()!=donor.bones.size()){error="Missing native or target right-hand reference";return {};}
    auto nativePalm=palmFrame(donor,donorPose);
    const auto targetRest=restPose(target);
    const auto destinationPalm=palmFrame(target,targetRest);
    auto sourceSocket=rigid(donorPose[source.donor]);
    auto weaponSocket=world.skeleton.bones[source.weapon].restGlobal;
    for(int k:{12,13,14})weaponSocket.v[k]*=source.weaponUnits;
    const auto weaponFrame=rigid(weaponSocket);
    if(!nativePalm||!destinationPalm||!sourceSocket||!weaponFrame){error="Degenerate native palm/socket frame";return {};}
    if(weaponGame=="cso2"&&source.donor!=sourcePalm.wrist){
        const auto helper=bone(donor,{"CSO2_W_bone_RHand"});
        const auto rest=restPose(donor);const auto restPalm=palmFrame(donor,rest);
        const auto wristFrame=helper>=0?rigid(donor.bones[helper].restGlobal):std::optional<Mat4>{};
        const auto helperFrame=helper>=0?rigid(donorPose[helper]):std::optional<Mat4>{};
        if(helperFrame&&wristFrame&&restPalm)nativePalm=(*helperFrame)*inverseAffine(*wristFrame)*(*restPalm);
    }
    // CSS's bind weapon bone is parked at the origin; only sampled native
    // holding clips bring it to the hand. Never calibrate against that parking pose.
    if(length(transformPoint(*sourceSocket,{})-transformPoint(*nativePalm,{}))>60.f){
        // Source character rigs export an explicit right-hand IK target under
        // the weapon bone. It supplies a native hold without guessing a parked
        // weapon's offset, even when the native locomotion clips were not exported.
        const auto helper=weaponGame=="css"?bone(donor,{"ValveBiped.weapon_bone_RHand"}):-1;
        const auto rest=restPose(donor);const auto restPalm=palmFrame(donor,rest);
        const auto wristFrame=rigid(donor.bones[sourcePalm.wrist].restGlobal);
        const auto helperFrame=helper>=0?rigid(donor.bones[helper].restGlobal):std::optional<Mat4>{};
        if(!helperFrame||!wristFrame||!restPalm){error="Native weapon socket is detached; sample a native holding animation";return {};}
        nativePalm=(*helperFrame)*inverseAffine(*wristFrame)*(*restPalm);
        sourceSocket=rigid(donor.bones[source.donor].restGlobal);
    }
    const auto result=inverseAffine(target.bones[targetPalm.wrist].restGlobal)*(*destinationPalm)*inverseAffine(*nativePalm)*(*sourceSocket)*inverseAffine(*weaponFrame)*scale({source.weaponUnits,source.weaponUnits,source.weaponUnits});
    if(!finite(result)){error="Nonfinite native world-grip transform";return {};}
    return Mount{static_cast<std::size_t>(targetPalm.wrist),result};
}
}
