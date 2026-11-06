#pragma once
#include "scene/ImportedNative.h"
#include "scene/VolumePreservingFit.h"
#include "scene/NativeBoneNames.h"
#include "scene/ImportedForearmRoll.h"
#include "scene/ProximalContinuation.h"
#include "scene/SleeveClearance.h"
#include <stdexcept>
#include <string_view>
#include <map>
#include <cstdint>

namespace scene::imported {
struct ViewArmLayout {
    int shoulder{-1},elbow{-1},wrist{-1};
    std::array<std::array<int,3>,5> fingers{{{{-1,-1,-1}},{{-1,-1,-1}},{{-1,-1,-1}},{{-1,-1,-1}},{{-1,-1,-1}}}};
    bool hasFinger(std::size_t digit)const{return std::all_of(fingers[digit].begin(),fingers[digit].end(),[](int b){return b>=0;});}
    bool valid()const{
        if(elbow<0||wrist<0||!hasFinger(0)||!hasFinger(1)||!hasFinger(4))return false;
        int complete{};for(const auto& finger:fingers){const auto count=std::count_if(finger.begin(),finger.end(),[](int b){return b>=0;});if(count!=0&&count!=3)return false;complete+=count==3;}
        // Named four-digit anatomy is valid; a partially exported chain is not.
        return complete>=4;
    }
};
struct ViewLayout {std::array<ViewArmLayout,2> arms;bool valid()const{return arms[0].valid()&&arms[1].valid();}bool usable()const{bool found=false;for(const auto& arm:arms){if(arm.valid())found=true;else if(arm.wrist>=0||arm.elbow>=0)return false;}return found;}};
// Only an identity check, not a retarget compatibility heuristic. A native
// assembly may append weapon bones/meshes while retaining its original skin.
inline bool nativeViewSkinUnchanged(const CastScene& assembled,const CastScene& selected){
    if(selected.skeleton.bones.empty()||selected.skeleton.bones.size()>assembled.skeleton.bones.size())return false;
    for(size_t i=0;i<selected.skeleton.bones.size();++i){const auto& a=assembled.skeleton.bones[i];const auto& b=selected.skeleton.bones[i];
        if(a.name!=b.name||a.parent!=b.parent||a.restGlobal.v!=b.restGlobal.v||a.inverseBind.v!=b.inverseBind.v)return false;
    }
    std::vector<const Mesh*> a,b;
    for(const auto& m:assembled.meshes)if(!m.viewmodelWeapon)a.push_back(&m);
    for(const auto& m:selected.meshes)if(!m.viewmodelWeapon)b.push_back(&m);
    if(a.empty()||a.size()!=b.size())return false;
    bool populated=false;
    for(size_t i=0;i<a.size();++i){const auto& x=*a[i];const auto& y=*b[i];
        if(x.name!=y.name||x.indices!=y.indices||x.vertices.size()!=y.vertices.size()||x.modelTransform.v!=y.modelTransform.v||x.skinned!=y.skinned||x.attachmentIndex!=y.attachmentIndex)return false;
        populated|=!x.vertices.empty()&&!x.indices.empty();
        for(size_t j=0;j<x.vertices.size();++j){const auto& p=x.vertices[j];const auto& q=y.vertices[j];
            if(p.position.x!=q.position.x||p.position.y!=q.position.y||p.position.z!=q.position.z||p.bones!=q.bones||p.weights!=q.weights||p.armSkinning!=q.armSkinning)return false;
        }
    }
    return populated;
}
// Diagnostic envelope ownership only; never changes animation parents/weights.
inline std::vector<bool> diagnosticArmEnvelopeOwners(const Skeleton& rig,int bone,int end){
    std::vector<bool> owned(rig.bones.size());
    if(bone<0||end<0||size_t(bone)>=owned.size()||size_t(end)>=owned.size())return owned;
    owned[bone]=true;
    for(size_t i=0;i<owned.size();++i){
        const auto name=assets::imported::lower(rig.bones[i].name);
        if(name.find("twist")==std::string::npos&&name.find("roll")==std::string::npos)continue;
        int cursor=static_cast<int>(i);
        for(size_t n=0;n<owned.size()&&cursor>=0&&size_t(cursor)<owned.size();++n){
            if(cursor==end)break;
            if(cursor==bone){owned[i]=true;break;}
            cursor=rig.bones[cursor].parent;
        }
    }
    return owned;
}
inline bool viewForearmTwist(const std::string& name,int side,bool underscoreCandidate=false){
    const std::string lr=side?"r":"l",pb=lr+" twist";
    // PB Twist1..4 is parented to UpperArm but spans the forearm in bind space.
    const bool pbChain=name.starts_with(pb)&&name.size()>pb.size()&&
        std::all_of(name.begin()+pb.size(),name.end(),[](unsigned char c){return std::isdigit(c);});
    const std::string cod=side?"ri":"le";
    const bool wristHelper=name=="j_wristtwist_"+cod||
        (name.starts_with("j_sleave_reshape_")&&name.find("_"+cod+"_")!=std::string::npos);
    // Unreal-style exports retain underscore-separated semantic helper names.
    // Only the exact side and an optional numeric chain suffix are accepted.
    const std::string skelPrefix="skelbone_"+lr+"_foretwist";
    const bool skelChain=name.starts_with(skelPrefix)&&
        std::all_of(name.begin()+skelPrefix.size(),name.end(),[](unsigned char c){return std::isdigit(c);});
    // Semantic recognition alone does not validate the resulting skin fit.
    // The underscore experiment changed donor contours without resolving the
    // cloth defect; retain the established production path until validated.
    // CODM's roll helper is parented to the upper arm but spatially belongs
    // to the forearm. Pose-only assembly must not inherit the upper-arm anchor.
    const bool codmRoll=name==(side?"b_rightforearmroll":"b_leftforearmroll");
    return pbChain||wristHelper||(underscoreCandidate&&(skelChain||codmRoll))||name.find(lr+" foretwist")!=std::string::npos||name.starts_with("arm_lower_"+lr+"_twist");
}
inline ViewLayout viewLayout(const CastScene& scene){
    auto s=scene.skeleton;ViewLayout result;
    // Recover anatomical relationships from the retained original weapon rig.
    // Shared-hand animation tracks remain absolute roots; only this local
    // discovery copy receives parents, never the animated driver skeleton.
    for(std::size_t i=0;i<s.bones.size();++i){auto& hand=s.bones[i];if(!hand.name.starts_with("cs_hand_")||hand.parent>=0)continue;const auto suffix=hand.name.substr(8);int recovered=-1;bool ambiguous=false;
        for(const auto& native:scene.skeleton.bones){if(!native.name.starts_with("cs_weapon_")||!native.name.ends_with("_"+suffix)||native.parent<0)continue;if(length(transformPoint(native.restGlobal,{})-transformPoint(hand.restGlobal,{}))>.001f)continue;const auto prefix=native.name.substr(0,native.name.size()-suffix.size());const auto& parent=scene.skeleton.bones[native.parent];if(!parent.name.starts_with(prefix))continue;const auto it=s.boneByName.find("cs_hand_"+parent.name.substr(prefix.size()));if(it==s.boneByName.end())continue;const auto candidate=static_cast<int>(it->second);if(recovered>=0&&recovered!=candidate)ambiguous=true;recovered=candidate;}
        if(!ambiguous&&recovered>=0&&recovered!=static_cast<int>(i))hand.parent=recovered;
    }
    const auto find=[&](const std::string& name){
        if(const auto it=s.boneByName.find(name);it!=s.boneByName.end())return static_cast<int>(it->second);
        // Cold War exports may retain exact hashed joint identities. Resolve
        // locally: never rename animation tracks or change the native rig.
        if(const auto it=s.boneByName.find(nativeBoneHash(name));it!=s.boneByName.end())return static_cast<int>(it->second);
        if(!name.starts_with("j_")&&!name.starts_with("Bip01 ")&&!name.starts_with("SkelBone_"))return -1;
        // Exporters preserve native capitalization and may replace spaces
        // with underscores. Resolve anatomy locally; never alias animation rigs.
        const auto key=[](std::string text){text=assets::imported::lower(text);std::erase_if(text,[](char c){return c==' '||c=='_';});return text;};
        const auto wanted=key(name);int found=-1;
        for(const auto& [n,i]:s.boneByName)if(key(n)==wanted){if(found>=0)return -1;found=static_cast<int>(i);}if(found>=0)return found;
        return -1;
    };
    const auto pos=[&](int b){return transformPoint(s.bones[b].restGlobal,{});};
    std::vector<std::vector<int>> children(s.bones.size());for(std::size_t i=0;i<s.bones.size();++i)if(s.bones[i].parent>=0&&static_cast<std::size_t>(s.bones[i].parent)<s.bones.size())children[s.bones[i].parent].push_back(static_cast<int>(i));
    const std::array<const char*,5> digits{"thumb","index","middle","ring","pinky"};
    for(int side=0;side<2;++side){
        auto& a=result.arms[side];const std::string lr=side?"r":"l",cod=side?"ri":"le",word=side?"Right":"Left",cap=side?"R":"L";
        if(find("cs_hand_"+cap+".palm")>=0){
            const auto p="cs_hand_"+cap+".";
            a.shoulder=find(p+"upperarm");a.elbow=find(p+"forearm");a.wrist=find(p+"palm");
            if(a.shoulder<0)a.shoulder=find("cs_hand_Bone_"+cap+"_Upper");
            std::vector<int> roots;
            for(int f=0;f<5;++f)roots.push_back(find(p+"finger"+std::to_string(f)+".0"));
            if(std::all_of(roots.begin(),roots.end(),[](int b){return b>=0;})){
                orderAnonymousFingerRoots(s,roots);
                for(int f=0;f<5;++f){a.fingers[f][0]=roots[f];const auto stem=s.bones[roots[f]].name.substr(0,s.bones[roots[f]].name.size()-1);for(int j=1;j<3;++j)a.fingers[f][j]=find(stem+std::to_string(j));}
            }
        }else if(find("cs_hand_"+word+"_H_Bone"+(side?"03":"21"))>=0){
            // Flattened CSNZ H_Bone arm schema. Numbering differs between
            // sides, but is shared across weapons using this exported rig.
            const auto p="cs_hand_"+word+"_H_Bone";
            a.shoulder=find(p+(side?"02":"37"));a.elbow=find(p+(side?"01":"20"));a.wrist=find(p+(side?"03":"21"));
            std::vector<int> roots;for(int n:side?std::array<int,5>{4,7,10,13,17}:std::array<int,5>{22,25,28,31,34})roots.push_back(find(p+(n<10?"0":"")+std::to_string(n)));
            if(std::all_of(roots.begin(),roots.end(),[](int b){return b>=0;})){orderAnonymousFingerRoots(s,roots);for(int f=0;f<5;++f){const int n=std::stoi(s.bones[roots[f]].name.substr(p.size()));for(int j=0;j<3;++j)a.fingers[f][j]=find(p+(n+j<10?"0":"")+std::to_string(n+j));}}
        }else if(find("cs_hand_zombieF"+cap+"ArmPalm")>=0){
            const auto p="cs_hand_zombieF"+cap+"Arm";a.shoulder=find(p+"1");a.elbow=find(p+"21");a.wrist=find(p+"Palm");
            std::vector<int> roots;for(int f=1;f<=5;++f)roots.push_back(find(p+"Digit"+std::to_string(f)+"1"));
            if(std::all_of(roots.begin(),roots.end(),[](int b){return b>=0;})){orderAnonymousFingerRoots(s,roots);for(int f=0;f<5;++f){const auto stem=s.bones[roots[f]].name.substr(0,s.bones[roots[f]].name.size()-1);for(int j=0;j<3;++j)a.fingers[f][j]=find(stem+std::to_string(j+1));}}
        }else if(find("arm_lower_"+cap)>=0&&find("hand_"+cap)>=0){
            // armUpperShoulder is the shoulder anchor, not the rotating
            // humerus. Weight a replacement sleeve to the authored upper arm.
            a.shoulder=find("arm_upper_"+cap);if(a.shoulder<0)a.shoulder=find("armUpperShoulder_"+cap);
            a.elbow=find("arm_lower_"+cap);a.wrist=find("hand_"+cap);
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)a.fingers[f][j]=find("finger_"+std::string(digits[f])+"_"+std::to_string(j)+"_"+cap);
        }else if(find("j_wrist_"+cod)>=0){
            a.shoulder=find("j_shoulder_"+cod);a.elbow=find("j_elbow_"+cod);a.wrist=find("j_wrist_"+cod);
            const bool oneBased=find("j_pinky_"+cod+"_0")<0&&find("j_pinky_"+cod+"_3")>=0;
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)a.fingers[f][j]=find("j_"+std::string(f==2?"mid":digits[f])+"_"+cod+"_"+std::to_string(j+(oneBased?1:0)));
        }else if(find(lr+"_hand")>=0){
            a.shoulder=find(lr+"_upperarm");a.elbow=find(lr+"_forearm");a.wrist=find(lr+"_hand");
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)a.fingers[f][j]=find(lr+"_"+digits[f]+"_"+(j==0?"low":j==1?"mid":"tip"));
        }else if(find("SkelBone_"+cap+"_Hand")>=0){
            const auto p="SkelBone_"+cap+"_";a.shoulder=find(p+"UpperArm");a.elbow=find(p+"Forearm");a.wrist=find(p+"Hand");
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)a.fingers[f][j]=find(p+"Finger"+std::to_string(f)+(j?std::to_string(j):""));
        }else if(find("CSO2_MHAND "+cap+" Hand")>=0||find("ValveBiped.Bip01_"+cap+"_Hand")>=0||find("Bip01 "+cap+" Hand")>=0){
            const bool cso=find("CSO2_MHAND "+cap+" Hand")>=0,source=find("ValveBiped.Bip01_"+cap+"_Hand")>=0;
            const std::string p=cso?"CSO2_MHAND "+cap+" ":source?"ValveBiped.Bip01_"+cap+"_":"Bip01 "+cap+" ";
            a.shoulder=find(p+"UpperArm");a.elbow=find(p+"Forearm");a.wrist=find(p+"Hand");
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)a.fingers[f][j]=find(p+"Finger"+std::to_string(f)+(j?std::to_string(j):""));
        }else if(find("v_weapon."+word+"_Hand")>=0){
            const std::string p="v_weapon."+word+"_";a.elbow=find(p+"Arm");a.wrist=find(p+"Hand");
            for(int f=0;f<5;++f)for(int j=0;j<3;++j){auto finger=std::string(digits[f]);finger[0]=static_cast<char>(std::toupper(finger[0]));a.fingers[f][j]=find(p+finger+"0"+std::to_string(j+1));if(a.fingers[f][j]<0)a.fingers[f][j]=find(p+finger+"_0"+std::to_string(j+1));}
        }else if(find(cap+" Hand")>=0||find("b_"+word+"Hand")>=0){
            const bool pb=find(cap+" Hand")>=0;const auto p=pb?cap+" ":"b_"+word;
            a.shoulder=find(p+(pb?"UpperArm":"Arm"));a.elbow=find(p+(pb?"Forearm":"ForeArm"));a.wrist=find(p+"Hand");
            for(int f=0;f<5;++f)for(int j=0;j<3;++j){auto finger=std::string(pb&&f==4?"Little":digits[f]);finger[0]=static_cast<char>(std::toupper(finger[0]));a.fingers[f][j]=find(p+finger+std::to_string(j+1));}
        }else{
            // weights, then find a wrist with exactly five 3-bone digit chains.
            int root=-1;const auto rootOf=[&](int b){for(std::size_t n=0;b>=0&&static_cast<std::size_t>(b)<s.bones.size()&&n<s.bones.size();++n){if(s.bones[b].parent<0)return b;b=s.bones[b].parent;}return -1;};
            const auto named=find("Bone_"+word+"hand");if(named>=0)root=rootOf(named);
            if(root<0)for(std::size_t i=0;i<s.bones.size();++i)if(s.bones[i].name.starts_with("cs_hand_"+word+"_")){root=rootOf(static_cast<int>(i));break;}
            if(root<0){std::vector<float> votes(s.bones.size());for(const auto& m:scene.meshes)if(assets::imported::lower(m.name).starts_with(side?"rhand/":"lhand/"))for(const auto& v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0){const auto r=rootOf(static_cast<int>(v.bones[k]));if(r>=0)votes[r]+=v.weights[k];}if(!votes.empty()){auto it=std::max_element(votes.begin(),votes.end());if(*it>0)root=static_cast<int>(it-votes.begin());}}
            // Named wrists disambiguate arms sharing a common scene root.
            // Searching that whole root would discover both wrists and reject
            // valid CSNZ rigs as ambiguous.
            const auto underNamed=[&](int w){for(std::size_t n=0;w>=0&&static_cast<std::size_t>(w)<s.bones.size()&&n<s.bones.size();++n){if(w==named)return true;w=s.bones[w].parent;}return false;};
            if(root>=0)for(std::size_t w=0;w<s.bones.size();++w)if(named>=0?underNamed(static_cast<int>(w)):rootOf(static_cast<int>(w))==root){
                std::vector<std::array<int,3>> fingers;
                for(const int first:children[w])if(children[first].size()==1){const auto second=children[first][0];if(children[second].size()==1){const auto third=children[second][0];if(children[third].empty())fingers.push_back({first,second,third});}}
                if(fingers.size()!=5)continue;
                if(a.wrist>=0){a=ViewArmLayout{};break;}
                a.wrist=static_cast<int>(w);a.elbow=s.bones[w].parent;
                if(a.elbow<0)continue;
                const auto grand=s.bones[a.elbow].parent;
                if(grand>=0&&length(pos(a.wrist)-pos(a.elbow))<length(pos(a.wrist)-pos(grand))*.2f)a.elbow=grand;
                a.shoulder=s.bones[a.elbow].parent;
                if(const auto upper=find("Bone_"+cap+"_Upper");upper>=0)a.shoulder=upper;
                std::vector<int> roots;for(const auto& finger:fingers)roots.push_back(finger[0]);orderAnonymousFingerRoots(s,roots);
                for(int f=0;f<5;++f)for(const auto& finger:fingers)if(finger[0]==roots[f])a.fingers[f]=finger;
            }
        }
    }
    return result;
}
// Some shared Source exports use the opposite lateral basis from the original
// weapon skeleton. Match the reflected bind as a whole, then skin directly to
// the original bones. A quaternion cannot represent the bind reflection.
inline bool bindMirroredSourceViewSkin(CastScene& driver,const CastScene& skin){
    CastScene original;
    original.skeleton.bones=driver.skeleton.bones;
    for(std::size_t i=0;i<original.skeleton.bones.size();++i){
        auto& name=original.skeleton.bones[i].name;
        if(!name.starts_with("cs_weapon_"))continue;
        const auto suffix=name.find("_v_weapon.");
        if(suffix==std::string::npos)continue;
        name.erase(0,suffix+1);
        if(!original.skeleton.boneByName.emplace(name,i).second)return false;
    }
    if(original.skeleton.boneByName.empty())return false;
    const auto from=viewLayout(skin),to=viewLayout(original);
    if(!from.valid()||!to.valid())return false;
    std::vector<int> mapping(skin.skeleton.bones.size(),-1);
    for(std::size_t i=0;i<mapping.size();++i){
        const auto& name=skin.skeleton.bones[i].name;
        if(!name.starts_with("cs_hand_v_weapon."))continue;
        const auto native=original.skeleton.boneByName.find(name.substr(8));
        if(native!=original.skeleton.boneByName.end())mapping[i]=static_cast<int>(native->second);
    }
    for(int side=0;side<2;++side){
        const auto& a=from.arms[side];const auto& b=to.arms[side];
        mapping[a.elbow]=b.elbow;mapping[a.wrist]=b.wrist;
        for(std::size_t digit=0;digit<a.fingers.size();++digit){
            if(a.hasFinger(digit)!=b.hasFinger(digit))return false;
            if(a.hasFinger(digit))for(std::size_t joint=0;joint<a.fingers[digit].size();++joint)
                mapping[a.fingers[digit][joint]]=b.fingers[digit][joint];
        }
    }
    const auto lateralMirror=scale({1,-1,1});
    const auto shiftFor=[&](std::size_t source){
        return transformPoint(driver.skeleton.bones[mapping[source]].restGlobal,{})-
            transformPoint(lateralMirror*skin.skeleton.bones[source].restGlobal,{});
    };
    const auto shift=shiftFor(from.arms[0].wrist);
    const auto bindTransform=translation(shift)*lateralMirror;
    for(std::size_t i=0;i<mapping.size();++i)
        if(mapping[i]>=0&&!(length(shiftFor(i)-shift)<.01f))return false;
    std::vector<Mesh> hands;
    for(auto mesh:skin.meshes){
        if(mesh.viewmodelWeapon)continue;
        const auto transform=bindTransform*mesh.modelTransform;
        const auto inverse=inverseAffine(transform);
        for(auto& vertex:mesh.vertices){
            for(std::size_t k=0;k<vertex.weights.size();++k)if(vertex.weights[k]>0){
                const auto bone=vertex.bones[k];
                if(bone>=mapping.size()||mapping[bone]<0)return false;
                vertex.bones[k]=static_cast<std::uint32_t>(mapping[bone]);
            }
            vertex.position=transformPoint(transform,vertex.position);
            const auto n=vertex.normal;
            vertex.normal=normalize(Vec3{inverse.v[0]*n.x+inverse.v[1]*n.y+inverse.v[2]*n.z,
                inverse.v[4]*n.x+inverse.v[5]*n.y+inverse.v[6]*n.z,
                inverse.v[8]*n.x+inverse.v[9]*n.y+inverse.v[10]*n.z});
        }
        for(std::size_t i=0;i+2<mesh.indices.size();i+=3)std::swap(mesh.indices[i+1],mesh.indices[i+2]);
        mesh.modelTransform=Mat4::identity();
        hands.push_back(std::move(mesh));
    }
    if(hands.empty())return false;
    std::erase_if(driver.meshes,[](const auto& mesh){return !mesh.viewmodelWeapon;});
    driver.meshes.insert(driver.meshes.end(),std::make_move_iterator(hands.begin()),std::make_move_iterator(hands.end()));
    return true;
}
inline Mat4 viewAnatomicalFrame(Vec3 origin,Vec3 next,Vec3 normal){
    const auto x=normalize(next-origin);const auto y=normalize(cross(normal,x));if(length(x)<.5f||length(y)<.5f)throw std::runtime_error("Degenerate imported hand frame");
    const auto z=normalize(cross(x,y));Mat4 m=Mat4::identity();for(int i=0;i<3;++i){m.v[i]=i==0?x.x:i==1?x.y:x.z;m.v[4+i]=i==0?y.x:i==1?y.y:y.z;m.v[8+i]=i==0?z.x:i==1?z.y:z.z;}m.v[12]=origin.x;m.v[13]=origin.y;m.v[14]=origin.z;return m;
}
// Skin-only conversion: the driver skeleton, clips, camera and weapon
// mechanisms remain byte-for-byte unchanged. Fractional native sampling and
// all subsequent action layers therefore retain their existing behavior.
inline bool viewHasWeightedUpperArm(const CastScene& skin,const ViewArmLayout& arm){
    if(arm.shoulder<0)return false;
    const auto& bones=skin.skeleton.bones;
    std::vector<int> canonical(bones.size());for(size_t i=0;i<bones.size();++i)canonical[i]=static_cast<int>(i);
    for(const auto& [follower,source]:skin.nativePoseFollowers)if(follower<bones.size()&&source<bones.size())canonical[follower]=static_cast<int>(source);
    for(const auto& mesh:skin.meshes)if(!mesh.viewmodelWeapon)for(const auto index:mesh.indices){
        if(index>=mesh.vertices.size())continue;
        const auto& vertex=mesh.vertices[index];
        for(int k=0;k<4;++k)if(vertex.weights[k]>0&&vertex.bones[k]<bones.size()){
            int bone=canonical[vertex.bones[k]];
            // Upper-arm twist/roll children count, but forearm/finger children do not.
            for(size_t step=0;bone>=0&&static_cast<size_t>(bone)<bones.size()&&step<bones.size();++step){
                if(bone==arm.elbow||bone==arm.wrist)break;
                if(bone==arm.shoulder)return true;
                bone=bones[bone].parent;
            }
        }
    }
    return false;
}
struct ProximalContinuityField {
    std::vector<bool> affected;
    Vec3 origin{},axis{};
    float start{},end{};
    float weight(Vec3 p)const{
        const float denominator=end-start,station=dot(p-origin,axis)-start;
        if(!std::isfinite(denominator)||denominator<=0||!std::isfinite(station))return 0.f;
        const float t=std::clamp(station/denominator,0.f,1.f);
        return std::clamp(t*t*t*(t*(t*6-15)+10),0.f,1.f);
    }
};
// Diagnostic: only a single connected, rigid upper-arm component adjoining a
// forearm ownership seam. Flat near-straight source anatomy is required; names
// and game identities are deliberately not eligibility criteria.
inline ProximalContinuityField proximalContinuityField(const CastScene& skin,const Mesh& mesh,const ViewArmLayout& arm){
    ProximalContinuityField result;
    if(!arm.valid()||arm.shoulder<0||mesh.viewmodelWeapon)return result;
    const auto& bones=skin.skeleton.bones;
    for(int b:{arm.shoulder,arm.elbow,arm.wrist})if(static_cast<std::size_t>(b)>=bones.size())return result;
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    for(float v:mesh.modelTransform.v)if(!std::isfinite(v))return result;
    for(int b:{arm.shoulder,arm.elbow,arm.wrist})for(float v:bones[b].restGlobal.v)if(!std::isfinite(v))return result;
    if(bones[arm.shoulder].parent>=0||bones[arm.elbow].parent>=0||bones[arm.wrist].parent>=0)return result;
    const auto pos=[&](int b){return transformPoint(bones[b].restGlobal,{});};
    const auto upper=pos(arm.elbow)-pos(arm.shoulder),fore=pos(arm.wrist)-pos(arm.elbow);
    const float span=length(upper);
    if(!finite(upper)||!finite(fore)||!std::isfinite(span)||!std::isfinite(length(fore))||span<1e-4f||length(fore)<1e-4f||dot(normalize(upper),normalize(fore))<.965925826f)return result;
    const auto count=mesh.vertices.size();std::vector<int> owner(count,-1);std::vector<std::size_t> parent(count);std::vector<bool> boundary(count);
    std::vector<Vec3> points(count);
    for(std::size_t i=0;i<count;++i){parent[i]=i;const auto& v=mesh.vertices[i];float up=0,down=0,other=0;
        points[i]=transformPoint(mesh.modelTransform,v.position);
        if(!finite(v.position)||!finite(v.normal)||!finite(points[i]))return {};
        for(float w:v.weights)if(!std::isfinite(w)||w<0)return {};
        for(int k=0;k<4;++k)if(v.weights[k]>0){if(v.bones[k]==static_cast<uint32_t>(arm.shoulder))up+=v.weights[k];else if(v.bones[k]==static_cast<uint32_t>(arm.elbow))down+=v.weights[k];else other+=v.weights[k];}
        if(other==0&&up>.9999f&&down==0)owner[i]=0;else if(other==0&&down>.9999f&&up==0)owner[i]=1;
    }
    const auto root=[&](std::size_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    // UV/normal seams duplicate vertices without disconnecting the surface.
    // Weld only for connectivity; preserve every original vertex and index.
    const float weldTolerance=span*1e-5f;
    // Three-dimensional cells avoid the quadratic same-X sweep. Exact duplicate
    // UV vertices share one sample; crowded nonidentical cells conservatively
    // reject this optional diagnostic after a linear comparison budget.
    using Cell=std::array<int64_t,3>;
    std::map<Cell,std::vector<std::size_t>> cells;
    std::map<std::array<float,3>,std::size_t> exact;
    std::size_t comparisons=0;
    const std::size_t budget=count>std::numeric_limits<std::size_t>::max()/128?std::numeric_limits<std::size_t>::max():count*128;
    for(std::size_t i=0;i<count;++i)if(owner[i]==0){const auto p=points[i];
        const auto [duplicate,inserted]=exact.emplace(std::array<float,3>{p.x,p.y,p.z},i);
        if(!inserted){parent[root(i)]=root(duplicate->second);continue;}
        Cell cell{};const std::array<float,3> coords{p.x,p.y,p.z};
        for(int axis=0;axis<3;++axis){const double coordinate=std::floor(static_cast<double>(coords[axis])/weldTolerance);
            // Keep neighbor +/-1 safely representable, including conversion.
            if(!std::isfinite(coordinate)||std::abs(coordinate)>9e15)return {};
            cell[axis]=static_cast<int64_t>(coordinate);
        }
        for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){const auto found=cells.find(Cell{cell[0]+x,cell[1]+y,cell[2]+z});if(found==cells.end())continue;
            for(const auto j:found->second){if(comparisons++>=budget)return {};
                if(length(points[j]-p)<=weldTolerance)parent[root(i)]=root(j);
            }
        }
        cells[cell].push_back(i);
    }
    for(std::size_t t=0;t+2<mesh.indices.size();t+=3)for(int edge=0;edge<3;++edge){
        const auto a=mesh.indices[t+edge],b=mesh.indices[t+(edge+1)%3];if(a>=count||b>=count)return {};
        if(owner[a]==0&&owner[b]==0)parent[root(a)]=root(b);
        if(owner[a]==0&&owner[b]==1)boundary[a]=true;if(owner[b]==0&&owner[a]==1)boundary[b]=true;
    }
    std::size_t component=count,boundaryCount=0;result.origin=pos(arm.shoulder);result.axis=upper/span;
    float seamMin=std::numeric_limits<float>::max(),seamMax=-seamMin;
    for(std::size_t i=0;i<count;++i)if(boundary[i]){
        const auto current=root(i);if(component!=count&&component!=current)return {};component=current;++boundaryCount;
        const float station=dot(transformPoint(mesh.modelTransform,mesh.vertices[i].position)-result.origin,result.axis);
        seamMin=std::min(seamMin,station);seamMax=std::max(seamMax,station);
    }
    if(boundaryCount<3||seamMin<.7f*span||seamMax>1.2f*span||seamMax-seamMin>.15f*span)return {};
    float proximal=std::numeric_limits<float>::max();result.affected.assign(count,false);
    for(std::size_t i=0;i<count;++i)if(owner[i]==0&&root(i)==component){result.affected[i]=true;proximal=std::min(proximal,dot(transformPoint(mesh.modelTransform,mesh.vertices[i].position)-result.origin,result.axis));}
    if(!std::isfinite(proximal)||!std::isfinite(seamMin)||!std::isfinite(seamMin-proximal)||seamMin<=proximal||proximal>.25f*span)return {};
    result.start=proximal;result.end=seamMin;
    return result;
}
inline bool proximalContinuityTarget(const Skeleton& skeleton,const ViewArmLayout& arm){
    for(int b:{arm.shoulder,arm.elbow,arm.wrist})if(b<0||static_cast<std::size_t>(b)>=skeleton.bones.size())return false;
    const auto pos=[&](int b){return transformPoint(skeleton.bones[b].restGlobal,{});};
    const auto upper=pos(arm.elbow)-pos(arm.shoulder),fore=pos(arm.wrist)-pos(arm.elbow);
    const float a=length(upper),b=length(fore);
    if(!std::isfinite(a)||!std::isfinite(b)||a<1e-4f||b<1e-4f)return false;
    const float cosine=dot(upper/a,fore/b);
    return std::isfinite(cosine)&&cosine<.965925826f; // bent beyond source eligibility (15 degrees)
}
inline bool proximalContinuitySample(const ProximalContinuityField& field,const std::vector<volume_fit::Bone>& fits,
                                    const ViewArmLayout& arm,Vec3 point,Vec3& position,Mat4& differential){
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    if(!finite(point)||!std::isfinite(field.end-field.start)||field.end<=field.start)return false;
    const std::array<uint32_t,4> ids{static_cast<uint32_t>(arm.shoulder),static_cast<uint32_t>(arm.elbow),0,0};
    if(ids[0]>=fits.size()||ids[1]>=fits.size())return false;
    const float w=field.weight(point);
    position=volume_fit::position(fits,point,ids,std::array<float,4>{1-w,w,0,0},&differential);
    if(w>0&&w<1){
        const float h=(field.end-field.start)*.0001f;if(!std::isfinite(h)||h<=0)return false;
        for(int axis=0;axis<3;++axis){Vec3 delta{};if(axis==0)delta.x=h;else if(axis==1)delta.y=h;else delta.z=h;
            const auto sample=[&](Vec3 p){const float weight=field.weight(p);return volume_fit::position(fits,p,ids,std::array<float,4>{1-weight,weight,0,0});};
            const auto derivative=(sample(point+delta)-sample(point-delta))/(2*h);
            differential.v[axis*4]=derivative.x;differential.v[axis*4+1]=derivative.y;differential.v[axis*4+2]=derivative.z;
        }
    }
    const Vec3 x{differential.v[0],differential.v[1],differential.v[2]},y{differential.v[4],differential.v[5],differential.v[6]},z{differential.v[8],differential.v[9],differential.v[10]};
    const float determinant=dot(x,cross(y,z));
    return finite(position)&&finite(x)&&finite(y)&&finite(z)&&std::isfinite(determinant)&&determinant>1e-6f;
}
inline bool proximalContinuitySafe(const ProximalContinuityField& field,const Mesh& mesh,const std::vector<volume_fit::Bone>& fits,const ViewArmLayout& arm){
    if(field.affected.size()!=mesh.vertices.size()||field.affected.empty())return false;
    for(std::size_t i=0;i<mesh.vertices.size();++i)if(field.affected[i]){const auto& v=mesh.vertices[i];Vec3 position;Mat4 differential;
        try{
            if(!proximalContinuitySample(field,fits,arm,transformPoint(mesh.modelTransform,v.position),position,differential))return false;
        }catch(const std::runtime_error& error){
            // Only this optional deformation's recoverable singular rotation
            // declines the component. Propagate unrelated data/program errors.
            if(std::string_view(error.what())=="Degenerate fitted skin rotation")return false;
            throw;
        }
        const auto normal=volume_fit::vector(volume_fit::transpose3(inverseAffine(differential*mesh.modelTransform)),v.normal);
        if(!std::isfinite(normal.x)||!std::isfinite(normal.y)||!std::isfinite(normal.z))return false;
    }
    return true;
}
// Diagnostic only. Quantify whether tiny shared-ancestor weights are causing
// visible holes. Never called by production assembly; do not infer fidelity
// from the number of retained triangles or silently remap torso geometry.
inline std::size_t diagnosticMinorAncestorWeights(CastScene& skin){
    const auto layout=viewLayout(skin);const auto& bones=skin.skeleton.bones;
    if(!layout.usable()||layout.arms[0].shoulder<0||layout.arms[1].shoulder<0)return 0;
    const auto ancestor=[&](int parent,int child){
        for(std::size_t n=0;child>=0&&static_cast<std::size_t>(child)<bones.size()&&n<bones.size();++n){if(child==parent)return true;child=bones[child].parent;}return false;
    };
    std::vector<bool> shared(bones.size());
    for(std::size_t b=0;b<bones.size();++b)shared[b]=ancestor(static_cast<int>(b),layout.arms[0].shoulder)&&ancestor(static_cast<int>(b),layout.arms[1].shoulder);
    std::size_t changed{};
    for(auto& mesh:skin.meshes)if(!mesh.viewmodelWeapon)for(auto& v:mesh.vertices){
        float total{},residual{};int side=-1;bool valid=true;
        for(int k=0;k<4;++k){const float w=v.weights[k];if(!std::isfinite(w)||w<0){valid=false;break;}if(w==0)continue;
            const auto b=v.bones[k];if(b>=bones.size()){valid=false;break;}total+=w;
            if(shared[b]){residual+=w;continue;}
            int owner=-1;
            for(int s=0;s<2;++s)if(ancestor(layout.arms[s].shoulder,static_cast<int>(b))||ancestor(static_cast<int>(b),layout.arms[s].shoulder))owner=s;
            if(owner<0||(side>=0&&side!=owner)){valid=false;break;}side=owner;
        }
        if(!valid||side<0||std::abs(total-1.f)>1e-4f||residual<=0||residual>.025f)continue;
        for(int k=0;k<4;++k)if(v.weights[k]>0)v.weights[k]=shared[v.bones[k]]?0.f:v.weights[k]/(total-residual);
        ++changed;
    }
    return changed;
}
inline bool assemblePoseOnlySkin(CastScene& driver,const CastScene& skin,const ViewLayout& from,const ViewLayout& to,const std::vector<int>& inputMap,const std::vector<Mat4>& inputRotations,const std::vector<int>& secondary,const std::vector<float>& secondaryWeight,std::string& error){
    auto map=inputMap;auto rotations=inputRotations;
    std::vector<int> twistSide(map.size(),-1);std::vector<float> twistWeight(map.size());
    for(std::size_t i=0;i<map.size();++i)for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
        if(!a.valid()||!b.valid()||int(i)==a.elbow||int(i)==a.wrist||!viewForearmTwist(assets::imported::lower(skin.skeleton.bones[i].name),side,true))continue;
        const auto origin=transformPoint(skin.skeleton.bones[a.elbow].restGlobal,{}),axis=transformPoint(skin.skeleton.bones[a.wrist].restGlobal,{})-origin;
        if(dot(axis,axis)<1e-8f)continue;
        twistSide[i]=side;twistWeight[i]=std::clamp(dot(transformPoint(skin.skeleton.bones[i].restGlobal,{})-origin,axis)/dot(axis,axis),0.f,1.f);
        map[i]=b.elbow;rotations[i]=rotations[a.elbow];
    }
    const auto game=[](const CastScene& s){if(!s.importedViewGame.empty())return s.importedViewGame;const auto key=cadence::content::exportKey(s.sourceModelPath);if(!key.empty())return key.substr(0,key.find('/'));for(const auto& part:s.sourceModelPath)if(assets::imported::lower(part.string())=="cs2")return std::string("cs2");return std::string{};};
    const auto sourceUnits=[&](const CastScene& scene){const auto title=game(scene);
        if(title=="codm"&&!scene.codmNativeCentimetres)return 100.f;
        return (assets::imported::sourceFamily(title)||title=="cs2")&&scene.importedTranslationScale==1.f?2.54f:1.f;
    };
    const float units=sourceUnits(skin)/sourceUnits(driver);
    const auto& src=skin.skeleton;
    const auto flatSharedArm=[](const Skeleton& rig,const ViewArmLayout& arm){
        return arm.valid()&&rig.bones[arm.wrist].parent<0&&rig.bones[arm.wrist].name.starts_with("cs_hand_");
    };
    // A matching shared bind already has absolute tracks for the weighted
    // fingers, cuffs and twist helpers. Preserve those authored transforms.
    bool authoredPalette=units==1.f&&game(skin)==game(driver)&&std::any_of(to.arms.begin(),to.arms.end(),
        [&](const auto& arm){return flatSharedArm(driver.skeleton,arm);});
    if(authoredPalette){
        std::vector<int> direct(map.size(),-1);
        const auto identity=Mat4::identity();
        for(std::size_t i=0;i<src.bones.size();++i){
            const auto& bone=src.bones[i];
            const auto found=driver.skeleton.boneByName.find(bone.name);
            if(!bone.name.starts_with("cs_hand_")||bone.parent>=0||found==driver.skeleton.boneByName.end())continue;
            const auto& target=driver.skeleton.bones[found->second];
            const auto difference=target.restGlobal*bone.inverseBind;
            if(target.parent<0&&std::equal(difference.v.begin(),difference.v.end(),identity.v.begin(),
                [](float a,float b){return std::abs(a-b)<.002f;}))direct[i]=static_cast<int>(found->second);
        }
        for(const auto& mesh:skin.meshes)if(!mesh.viewmodelWeapon)
            for(const auto& v:mesh.vertices)for(std::size_t k=0;k<v.weights.size();++k)
                if(v.weights[k]>0&&(v.bones[k]>=direct.size()||direct[v.bones[k]]<0))authoredPalette=false;
        if(authoredPalette)map=std::move(direct);
    }
    const auto position=[&](int i){return transformPoint(src.bones[i].restGlobal,{})*units;};
    auto adapter=std::make_shared<CodmRigAdapter>();adapter->firstBone=driver.skeleton.bones.size();
    // Confirmed native grip-size mismatches: armored Halo and legacy CS.
    // Fit the weapon driver, never the donor anatomy. Leave other libraries
    // unchanged; mesh silhouettes/poly counts are not anatomical measurements.
    const auto driverTitle=game(driver);
    const bool legacyCsGrip=assets::imported::sourceFamily(driverTitle);
    if((driverTitle=="eldewrito"||legacyCsGrip)&&game(skin)!=driverTitle){
        float ratios=0;int count=0;
        for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
            if(!a.valid()||!b.valid()||!a.hasFinger(2)||!b.hasFinger(2))continue;
            const float donor=length(position(a.fingers[2][0])-position(a.wrist));
            const float native=length(transformPoint(driver.skeleton.bones[b.fingers[2][0]].restGlobal,{})-transformPoint(driver.skeleton.bones[b.wrist].restGlobal,{}));
            if(donor>1e-4f&&native>1e-4f){ratios+=donor/native;++count;adapter->weaponFitAnchor=b.fingers[2][0];}
        }
        if(count){adapter->weaponFitScale=std::clamp(ratios/count,.5f,legacyCsGrip?2.f:1.f);
            for(std::size_t i=0;i<driver.skeleton.bones.size();++i){const auto name=assets::imported::lower(driver.skeleton.bones[i].name);
                if(name.find("camera")!=std::string::npos||name=="tag_view")adapter->weaponFitCameras.push_back(i);
            }
        }
    }
    adapter->identity=(authoredPalette?"Authored shared-hand tracks / ":"Pose-only donor proportions v1 / ")+skin.sourceModelPath.generic_string();
    std::vector<Bone> bones=src.bones;
    for(std::size_t i=0;i<bones.size();++i){
        auto& bone=bones[i];for(int k:{12,13,14})bone.restGlobal.v[k]*=units;
        bone.restLocal.position=bone.restLocal.position*units;bone.inverseBind=inverseAffine(bone.restGlobal);
        CodmRigBinding binding;
        if(authoredPalette&&map[i]>=0){
            binding.source=static_cast<std::size_t>(map[i]);
        }else if(map[i]>=0){binding.source=static_cast<std::size_t>(map[i]);
            Vec3 p,scale;Quat q;decomposeAffine(rotations[i],p,q,scale);
            const auto desired=rotation(q)*bone.restGlobal;
            binding.offset=inverseAffine(driver.skeleton.bones[binding.source].restGlobal)*desired;
            if(secondary[i]>=0&&secondaryWeight[i]>0){
                binding.rollSource=static_cast<size_t>(secondary[i]);
                binding.rollOffset=inverseAffine(driver.skeleton.bones[binding.rollSource].restGlobal)*desired;
                binding.rollWeight=secondaryWeight[i];
            }
            if(twistSide[i]>=0){const auto side=twistSide[i];const auto& a=from.arms[side];const auto& b=to.arms[side];
                Vec3 p1,s1;Quat q1;decomposeAffine(rotations[a.wrist],p1,q1,s1);
                binding.rollSource=static_cast<size_t>(b.wrist);binding.rollOffset=inverseAffine(driver.skeleton.bones[b.wrist].restGlobal)*rotation(q1)*bone.restGlobal;binding.rollWeight=twistWeight[i];
                // Source2 forearm helpers distribute pronation, not wrist
                // flexion. Copying the full wrist swing kinks the glove cuff.
                if(flatSharedArm(driver.skeleton,b)||(game(skin)=="cs2"&&game(driver)!="cs2"))
                    binding.rollAxis=normalize(volume_fit::vector(inverseAffine(bone.restGlobal),position(a.wrist)-position(a.elbow)));
            }
        }
        adapter->bindings.push_back(binding);
    }
    // Full donor sleeves on IW's view driver must continue the forearm,
    // not retain the native upper-arm bend. Only change orientation ownership:
    // the position links below retain donor lengths and the palm anchor.
    // Assembly is shared by live loading and take reconstruction.
    const bool extendSleeves=(game(driver)=="iw_sp"&&game(skin)!="iw_sp")||(game(driver)=="cs2"&&game(skin)!="cs2");
    if(extendSleeves){
        const auto pos=[](const Skeleton& rig,int i){return transformPoint(rig.bones.at(i).restGlobal,{});};
        for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
            if(!a.valid()||!b.valid()||a.shoulder<0||b.shoulder<0)continue;
            const auto se=pos(src,a.elbow),sw=pos(src,a.wrist),de=pos(driver.skeleton,b.elbow),dw=pos(driver.skeleton,b.wrist);
            const auto sn=normalize(cross(pos(src,a.fingers[1][0])-sw,pos(src,a.fingers[4][0])-sw));
            const auto dn=normalize(cross(pos(driver.skeleton,b.fingers[1][0])-dw,pos(driver.skeleton,b.fingers[4][0])-dw));
            if(length(se-pos(src,a.shoulder))<1e-5f||length(dw-de)<1e-5f||length(sn)<.5f||length(dn)<.5f)continue;
            const auto fit=viewAnatomicalFrame(de,dw,dn)*inverseAffine(viewAnatomicalFrame(se,se+(se-pos(src,a.shoulder)),sn));
            Vec3 p,scale;Quat q;decomposeAffine(fit,p,q,scale);
            const auto desired=rotation(q)*bones[a.shoulder].restGlobal;
            const auto& upper=adapter->bindings[a.shoulder];
            if(upper.source>=driver.skeleton.bones.size())continue;
            const auto old=driver.skeleton.bones[upper.source].restGlobal*upper.offset;
            const auto correction=desired*inverseAffine(old);
            for(auto& binding:adapter->bindings)if(binding.source==static_cast<size_t>(b.shoulder)){
                binding.offset=inverseAffine(driver.skeleton.bones[b.elbow].restGlobal)*correction*driver.skeleton.bones[b.shoulder].restGlobal*binding.offset;
                binding.source=b.elbow;binding.rollWeight=0;
            }
        }
    }
    std::vector<bool> linked(bones.size());
    const auto link=[&](int target,int anchor,int orientation,int directionFrom=-1,int directionTo=-1,int rollParent=-1){
        const auto inv=inverseAffine(src.bones[orientation].restGlobal);
        const auto delta=position(target)-position(anchor);
        const Vec3 offset{inv.v[0]*delta.x+inv.v[4]*delta.y+inv.v[8]*delta.z,inv.v[1]*delta.x+inv.v[5]*delta.y+inv.v[9]*delta.z,inv.v[2]*delta.x+inv.v[6]*delta.y+inv.v[10]*delta.z};
        const int rollBone=rollParent>=0?static_cast<int>(adapter->firstBone)+rollParent:-1;
        adapter->positionLinks.push_back({adapter->firstBone+target,adapter->firstBone+anchor,adapter->firstBone+orientation,offset,directionFrom,directionTo,rollBone});linked[target]=true;
    };
    if(authoredPalette)for(std::size_t i=0;i<map.size();++i)if(map[i]>=0){
        adapter->positionLinks.push_back({adapter->firstBone+i,static_cast<std::size_t>(map[i]),adapter->firstBone+i,{}});
        linked[i]=true;
    }
    for(int side=0;!authoredPalette&&side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];if(!a.valid()||!b.valid())continue;
        // Anchor the grip at a palm joint, not at the wrist of a differently
        // proportioned hand. Translate the entire donor chain; never resize it.
        const int digit=a.hasFinger(2)&&b.hasFinger(2)?2:1;
        const auto palmOffset=transformPoint(inverseAffine(src.bones[a.wrist].restGlobal),transformPoint(src.bones[a.fingers[digit][0]].restGlobal,{}))*units;
        adapter->positionLinks.push_back({adapter->firstBone+a.wrist,static_cast<size_t>(b.fingers[digit][0]),adapter->firstBone+a.wrist,palmOffset*-1.f});linked[a.wrist]=true;
        link(a.elbow,a.wrist,a.elbow);if(a.shoulder>=0)link(a.shoulder,a.elbow,a.shoulder);
        for(std::size_t digitIndex=0;digitIndex<a.fingers.size();++digitIndex){
            const auto& finger=a.fingers[digitIndex];
            if(finger[0]<0)continue;
            link(finger[0],a.wrist,a.wrist);
            const bool guide=flatSharedArm(driver.skeleton,b)&&b.hasFinger(digitIndex);
            for(std::size_t joint=1;joint<finger.size();++joint)
                link(finger[joint],finger[joint-1],finger[joint-1],
                     guide?b.fingers[digitIndex][joint-1]:-1,guide?b.fingers[digitIndex][joint]:-1,
                     guide?(joint==1?a.wrist:finger[joint-2]):-1);
        }
    }
    // PB/other forearm-roll chains can be parented to UpperArm. Their spatial
    // anchor is the forearm, not that nominal parent or the previous roll bone.
    for(std::size_t i=0;!authoredPalette&&i<twistSide.size();++i)if(twistSide[i]>=0){const auto elbow=from.arms[twistSide[i]].elbow;link(static_cast<int>(i),elbow,elbow);}
    // Retain authored helper offsets, including flat universal-export roots.
    for(std::size_t pass=0;pass<bones.size();++pass){bool changed=false;
        for(std::size_t i=0;i<bones.size();++i)if(!linked[i]&&map[i]>=0){int parent=src.bones[i].parent;
            if(parent>=0&&linked[parent]){link(static_cast<int>(i),parent,parent);changed=true;continue;}
            int owner=-1;float nearest=std::numeric_limits<float>::max();
            for(std::size_t j=0;j<bones.size();++j)if(linked[j]&&map[j]==map[i]){const float distance=length(position(static_cast<int>(j))-position(static_cast<int>(i)));if(distance<nearest){nearest=distance;owner=static_cast<int>(j);}}
            if(owner>=0){link(static_cast<int>(i),owner,owner);changed=true;}
        }if(!changed)break;
    }
    const auto donorTitle=game(skin);
    if(driverTitle=="cs2"&&(donorTitle=="cs1.6"||donorTitle=="cz"||donorTitle=="cscz")){
        // GoldSrc exports have smaller hands than this driver. Enlarge each
        // complete hand about its grip in the skin palette, preserving bind
        // proportions, weapon motion, and the recorded pose matrices.
        for(int side=0;side<2;++side){
            const auto& donor=from.arms[side];const auto& native=to.arms[side];
            if(!donor.valid()||donor.shoulder>=0||!native.valid()||!donor.hasFinger(2)||!native.hasFinger(2))continue;
            const auto pivot=native.fingers[2][0];
            const float hand=length(position(donor.fingers[2][0])-position(donor.wrist));
            const float grip=length(transformPoint(driver.skeleton.bones[pivot].restGlobal,{})-
                                    transformPoint(driver.skeleton.bones[native.wrist].restGlobal,{}));
            if(hand<1e-4f||grip<=hand)continue;
            const auto prefix=side==0?"cs_hand_L.":"cs_hand_R.";
            ViewhandSizeFit fit{static_cast<std::size_t>(pivot),std::min(grip/hand,1.5f),{}};
            for(std::size_t i=0;i<src.bones.size();++i)
                if(linked[i]&&src.bones[i].name.starts_with(prefix))fit.bones.push_back(adapter->firstBone+i);
            if(!fit.bones.empty())adapter->viewhandSizeFits.push_back(std::move(fit));
        }
    }
    std::vector<Mesh> meshes;
    for(auto mesh:skin.meshes)if(!mesh.viewmodelWeapon){
        std::vector<bool> valid(mesh.vertices.size(),true);
        const auto ancestor=[&](int parent,int child){for(std::size_t n=0;child>=0&&std::size_t(child)<bones.size()&&n<bones.size();++n){if(parent==child)return true;child=src.bones[child].parent;}return false;};
        for(std::size_t i=0;i<mesh.vertices.size();++i){auto& v=mesh.vertices[i];float total=0;
            // Preserve the existing tiny shared-torso residual safeguard;
            // removing anatomical scale must not reopen sleeve triangles.
            float residual=0,sum=0;int owner=-1;bool safe=true;std::array<bool,4> discard{};
            for(int k=0;k<4;++k)if(v.weights[k]>0){const auto bone=v.bones[k];sum+=v.weights[k];if(bone>=map.size()){safe=false;continue;}
                discard[k]=map[bone]<0&&from.arms[0].shoulder>=0&&from.arms[1].shoulder>=0&&ancestor(static_cast<int>(bone),from.arms[0].shoulder)&&ancestor(static_cast<int>(bone),from.arms[1].shoulder);
                if(discard[k]){residual+=v.weights[k];continue;}
                int side=-1;for(int j=0;j<2;++j)if(from.arms[j].shoulder>=0&&ancestor(from.arms[j].shoulder,static_cast<int>(bone)))side=j;
                if(side<0||!linked[bone]||(owner>=0&&owner!=side))safe=false;owner=side;
            }
            if(safe&&residual>0&&residual<=.01f&&std::abs(sum-1.f)<1e-4f)for(int k=0;k<4;++k)v.weights[k]=discard[k]?0:v.weights[k]/(sum-residual);
            for(int k=0;k<4;++k)if(v.weights[k]>0){if(v.bones[k]>=map.size()||map[v.bones[k]]<0||!linked[v.bones[k]])valid[i]=false;total+=v.weights[k];}
            valid[i]=valid[i]&&std::isfinite(total)&&std::abs(total-1.f)<.01f;
            v.position=transformPoint(mesh.modelTransform,v.position)*units;
            const auto n=inverseAffine(mesh.modelTransform);v.normal=normalize(Vec3{n.v[0]*v.normal.x+n.v[1]*v.normal.y+n.v[2]*v.normal.z,n.v[4]*v.normal.x+n.v[5]*v.normal.y+n.v[6]*v.normal.z,n.v[8]*v.normal.x+n.v[9]*v.normal.y+n.v[10]*v.normal.z});
            for(auto& index:v.bones)index+=static_cast<uint32_t>(adapter->firstBone);
        }
        std::vector<uint32_t> indices;for(std::size_t i=0;i+2<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];if(a>=valid.size()||b>=valid.size()||c>=valid.size())throw std::runtime_error("Invalid donor triangle");if(valid[a]&&valid[b]&&valid[c])indices.insert(indices.end(),{a,b,c});}
        if(!indices.empty()){mesh.indices=std::move(indices);mesh.modelTransform=Mat4::identity();meshes.push_back(std::move(mesh));}
    }
    if(meshes.empty())throw std::runtime_error("No mapped donor arm triangles");
    if(!adapter->viewhandSizeFits.empty()){
        auto construction=driver.skeleton;
        construction.bones.insert(construction.bones.end(),bones.begin(),bones.end());
        const int endpoint=static_cast<int>(construction.bones.size());
        construction.bones.emplace_back();
        for(const auto& fit:adapter->viewhandSizeFits){
            const auto arm=std::find_if(from.arms.begin(),from.arms.end(),[&](const auto& a){
                return std::find(fit.bones.begin(),fit.bones.end(),adapter->firstBone+a.elbow)!=fit.bones.end();
            });
            if(arm==from.arms.end())continue;
            const int elbow=static_cast<int>(adapter->firstBone)+arm->elbow,wrist=static_cast<int>(adapter->firstBone)+arm->wrist;
            const auto e=transformPoint(construction.bones[elbow].restGlobal,{}),w=transformPoint(construction.bones[wrist].restGlobal,{});
            construction.bones.back().restGlobal=translation(e+(e-w));
            std::vector<std::size_t> counts;
            for(const auto& mesh:meshes)counts.push_back(mesh.vertices.size());
            const auto extension=continueProximalDiagnostic(meshes,construction,{endpoint,elbow,wrist,true},true,true,true);
            if(extension.addedVertices)driver.warnings.push_back("Continued short GoldSrc forearm from its source rim");
            for(std::size_t m=0;m<meshes.size();++m)for(auto i=counts[m];i<meshes[m].vertices.size();++i)
                for(auto& bone:meshes[m].vertices[i].bones)if(bone==static_cast<std::uint32_t>(endpoint))bone=elbow;
        }
    }
    for(auto& bone:bones){bone.name="pose_only|"+bone.name;if(bone.parent>=0)bone.parent+=static_cast<int>(adapter->firstBone);driver.skeleton.boneByName[bone.name]=driver.skeleton.bones.size();driver.skeleton.bones.push_back(std::move(bone));}
    std::erase_if(driver.meshes,[](const auto& m){return !m.viewmodelWeapon;});for(auto& mesh:meshes)driver.meshes.push_back(std::move(mesh));
    if(extendSleeves){
        std::array<std::pair<int,int>,2> arms;
        for(int side=0;side<2;++side){const auto& a=from.arms[side];arms[side]=a.shoulder>=0&&a.elbow>=0?
            std::pair{int(adapter->firstBone)+a.shoulder,int(adapter->firstBone)+a.elbow}:std::pair{-1,-1};}
        adapter->sleeves=prepareSleeveClearance(driver,adapter->firstBone,arms);
    }
    if(game(driver)!=game(skin))for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
        if(a.valid()&&a.shoulder>=0&&b.valid()&&b.shoulder<0){
            auto support=prepareForearmClearance(driver,adapter->firstBone,adapter->firstBone+a.shoulder,adapter->firstBone+a.elbow,adapter->firstBone+a.wrist);
            if(support.radius>0&&!support.movedBones.empty())adapter->forearmClearance.push_back(std::move(support));
        }
    }
    driver.codmRigAdapter=std::move(adapter);
    if(driver.pointBlankNativeCentimetres)driver.viewHandsDriverGame="pointblank";else if(driver.codmNativeCentimetres)driver.viewHandsDriverGame="codm";
    driver.warnings.push_back("Pose-only replacement hands: donor proportions retained; uniform unit ratio "+std::to_string(units));error.clear();return true;
}
inline bool fitForeignViewSkin(CastScene& driver,const CastScene& skin,std::string& error,bool preserveProximal=true,bool diagnosticUpperRoll=false,bool diagnosticArmTransport=false,bool diagnosticSpatialElbow=false,bool diagnosticFitOnlyElbow=false,bool diagnosticBendPlane=false,bool diagnosticNativeUpperWeights=false,bool diagnosticUniformArmRadii=false,bool diagnosticStraightProximalCarrier=false,bool diagnosticProximalContinuity=false,bool automaticProximalContinuity=true,bool diagnosticNativeArmEnvelope=false,bool diagnosticProximalEnvelopeOnly=false,bool poseOnly=false){
    const auto to=viewLayout(driver),from=viewLayout(skin);if(!to.usable()||!from.usable()){error="Incomplete native/foreign viewhand anatomy";return false;}
    const auto& s=skin.skeleton;const auto& d=driver.skeleton;const auto pos=[](const Skeleton& rig,int b){return transformPoint(rig.bones[b].restGlobal,{});};
    std::vector<int> map(s.bones.size(),-1),secondary(s.bones.size(),-1);std::vector<float> secondaryWeight(s.bones.size());std::vector<Mat4> deform(s.bones.size(),Mat4::identity());
    struct StraightSleeve {int elbow,shoulder,wrist,target,targetWrist;Vec3 origin,split,forearm;float width;};
    std::vector<StraightSleeve> straightSleeves;
    try{
        for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
            if(!a.valid()||!b.valid())continue;
            const auto an=normalize(cross(pos(s,a.fingers[1][0])-pos(s,a.wrist),pos(s,a.fingers[4][0])-pos(s,a.wrist))),bn=normalize(cross(pos(d,b.fingers[1][0])-pos(d,b.wrist),pos(d,b.fingers[4][0])-pos(d,b.wrist)));
            const float width=length(pos(d,b.fingers[1][0])-pos(d,b.fingers[4][0]))/length(pos(s,a.fingers[1][0])-pos(s,a.fingers[4][0]));
            if(!std::isfinite(width)||width<.001f||width>1000)throw std::runtime_error("Invalid imported hand scale");
            // Palm width must not inflate an entire forearm when game rigs
            // have different hand-to-arm proportions. Keep arm segments
            // uniformly scaled by their measured length; fit the palm itself.
            const auto fit=[&](int sb,int db,Vec3 snext,Vec3 dnext,Vec3 snormal=Vec3{},Vec3 dnormal=Vec3{}){const auto sp=pos(s,sb),dp=pos(d,db);const float axial=length(dnext-dp)/length(snext-sp);if(!std::isfinite(axial)||axial<.001f||axial>1000)throw std::runtime_error("Invalid imported arm segment");const float radial=(sb==a.elbow||sb==a.shoulder)?axial:width;
                const auto sourceFrame=viewAnatomicalFrame(sp,snext,length(snormal)>.5f?snormal:an),targetFrame=viewAnatomicalFrame(dp,dnext,length(dnormal)>.5f?dnormal:bn);
                float radialY=radial,radialZ=radial;
                // Universal hand joint spacing describes animation controls,
                // not glove thickness. Fit the skin envelope to the native
                // driver's hand instead of inflating it by knuckle spacing.
                // Explicit experiment for non-universal sleeves. Never alter
                // palm/finger scaling or enable this through production defaults.
                if(!poseOnly&&(s.bones[sb].name.starts_with("cs_hand_")||(diagnosticNativeArmEnvelope&&(sb==a.shoulder||(!diagnosticProximalEnvelopeOnly&&sb==a.elbow))))&&!(diagnosticUniformArmRadii&&(sb==a.shoulder||sb==a.elbow))){
                    const bool helperEnvelope=diagnosticNativeArmEnvelope&&(sb==a.shoulder||(!diagnosticProximalEnvelopeOnly&&sb==a.elbow));
                    const auto sourceOwners=helperEnvelope?diagnosticArmEnvelopeOwners(s,sb,sb==a.shoulder?a.elbow:a.wrist):std::vector<bool>{};
                    const auto targetOwners=helperEnvelope?diagnosticArmEnvelopeOwners(d,db,sb==a.shoulder?b.elbow:b.wrist):std::vector<bool>{};
                    const auto extent=[&](const CastScene& model,int bone,const Mat4& frame,int axis){const auto& owners=(&model==&skin)?sourceOwners:targetOwners;std::vector<float> samples;const auto inv=inverseAffine(frame);for(const auto& mesh:model.meshes){if(mesh.viewmodelWeapon)continue;for(const auto& v:mesh.vertices){float weight{};for(int k=0;k<4;++k)if(owners.empty()?v.bones[k]==static_cast<uint32_t>(bone):(v.bones[k]<owners.size()&&owners[v.bones[k]]))weight+=v.weights[k];if(weight<.5f)continue;const auto p=transformPoint(inv,transformPoint(mesh.modelTransform,v.position));samples.push_back(std::abs(axis==1?p.y:p.z));}}if(samples.size()<6)return 0.f;std::sort(samples.begin(),samples.end());return samples[(samples.size()-1)*9/10];};
                    const auto sy=extent(skin,sb,sourceFrame,1),sz=extent(skin,sb,sourceFrame,2),dy=extent(driver,db,targetFrame,1),dz=extent(driver,db,targetFrame,2);
                    if(sy>1e-5f&&dy>1e-5f)radialY=dy/sy;
                    if(sz>1e-5f&&dz>1e-5f)radialZ=dz/sz;
                }
                deform[sb]=targetFrame*(poseOnly?Mat4::identity():scale({axial,radialY,radialZ}))*inverseAffine(sourceFrame);map[sb]=db;};
            const auto palmCenter=[&](const Skeleton& rig,const ViewArmLayout& arm){return arm.hasFinger(2)?pos(rig,arm.fingers[2][0]):(pos(rig,arm.fingers[1][0])+pos(rig,arm.fingers[4][0]))*.5f;};
            fit(a.elbow,b.elbow,pos(s,a.wrist),pos(d,b.wrist));fit(a.wrist,b.wrist,palmCenter(s,a),palmCenter(d,b));
            if(a.shoulder>=0){if(b.shoulder>=0){
                const auto carry=[](Vec3 normal,Vec3 from,Vec3 to){
                    from=normalize(from);to=normalize(to);
                    normal=normalize(normal-from*dot(normal,from));
                    const float cosine=std::clamp(dot(from,to),-1.f,1.f);
                    const auto axis=cross(from,to);
                    const auto q=cosine<-.9999f?fromAxisAngle(normal,kPi):normalize(Quat{axis.x,axis.y,axis.z,1+cosine});
                    return normalize(volume_fit::vector(rotation(q),normal));
                };
                auto sn=diagnosticArmTransport?carry(an,pos(s,a.wrist)-pos(s,a.elbow),pos(s,a.elbow)-pos(s,a.shoulder)):an;
                auto dn=diagnosticArmTransport?carry(bn,pos(d,b.wrist)-pos(d,b.elbow),pos(d,b.elbow)-pos(d,b.shoulder)):bn;
                if(diagnosticBendPlane){
                    // Humerus roll need not follow wrist pronation. Use the
                    // same ordered cross on both rigs (including mirrored
                    // sides); never independently flip it toward the palm.
                    const auto sbend=cross(normalize(pos(s,a.elbow)-pos(s,a.shoulder)),normalize(pos(s,a.wrist)-pos(s,a.elbow)));
                    const auto dbend=cross(normalize(pos(d,b.elbow)-pos(d,b.shoulder)),normalize(pos(d,b.wrist)-pos(d,b.elbow)));
                    constexpr float minimumBend=.087155743f; // sin(5 degrees)
                    if(length(sbend)>=minimumBend&&length(dbend)>=minimumBend){sn=normalize(sbend);dn=normalize(dbend);}
                }
                fit(a.shoulder,b.shoulder,pos(s,a.elbow),pos(d,b.elbow),sn,dn);
                // Diagnostic only: some flat universal exports use Upper for
                // a straight proximal continuation rather than a bent humerus.
                // Keep the existing forearm fit/radii and carry that skin on
                // its forearm; never rewrite the native driver or hand poses.
                if(diagnosticStraightProximalCarrier&&s.bones[a.shoulder].name.starts_with("cs_hand_Bone_")&&
                   s.bones[a.shoulder].name.ends_with("_Upper")&&s.bones[a.elbow].name.starts_with("cs_hand_")&&
                   s.bones[a.shoulder].parent<0&&s.bones[a.elbow].parent<0&&s.bones[a.wrist].parent<0){
                    const auto upper=pos(s,a.elbow)-pos(s,a.shoulder),fore=pos(s,a.wrist)-pos(s,a.elbow);
                    if(length(upper)>1e-4f&&length(fore)>1e-4f&&dot(normalize(upper),normalize(fore))>=.965925826f){
                        deform[a.shoulder]=deform[a.elbow];map[a.shoulder]=b.elbow;
                    }
                }
            }else{
                // Shoulderless view rigs author a straight sleeve continuing
                // behind the forearm. Reusing its transform preserved the
                // donor's bent upper arm (and exposed the open sleeve cap).
                // Anchor both segments at the elbow; only the donor skin is
                // straightened, never the native hand/weapon animation.
                const auto se=pos(s,a.elbow),de=pos(d,b.elbow);
                const float armScale=length(pos(d,b.wrist)-de)/length(pos(s,a.wrist)-se);
                deform[a.shoulder]=viewAnatomicalFrame(de,pos(d,b.wrist),bn)*(poseOnly?Mat4::identity():scale({armScale,armScale,armScale}))*
                    inverseAffine(viewAnatomicalFrame(se,se+(se-pos(s,a.shoulder)),an));
                map[a.shoulder]=b.elbow;
                straightSleeves.push_back({a.elbow,a.shoulder,a.wrist,b.elbow,b.wrist,se,
                    normalize(normalize(pos(s,a.shoulder)-se)-normalize(pos(s,a.wrist)-se)),
                    pos(s,a.wrist)-se,
                    length(pos(s,a.wrist)-se)*.12f});
            }}
            const auto transport=[](Vec3 normal,Vec3 from,Vec3 to){from=normalize(from);to=normalize(to);const float dotp=std::clamp(dot(from,to),-1.f,1.f);Quat q;if(dotp<-.9999f)q=fromAxisAngle(normal,kPi);else{const auto axis=cross(from,to);q=normalize(Quat{axis.x,axis.y,axis.z,1+dotp});}return normalize(volume_fit::vector(rotation(q),normal));};
            for(int f=0;f<5;++f){if(!a.hasFinger(f))continue;
                if(!b.hasFinger(f)){
                    // Four-digit drivers have no authored motion for a human
                    // middle/ring digit. Interpolate its two neighbours while
                    // retaining a separate bind-space finger, not a collapsed
                    // duplicate. This is skinning only; no new animated rig.
                    int left=f-1,right=f+1;while(left>=1&&!b.hasFinger(left))--left;while(right<5&&!b.hasFinger(right))++right;
                    if(left<1||right>=5)throw std::runtime_error("Missing driver digit has no anatomical neighbours");
                    const float t=float(f-left)/float(right-left);
                    for(int j=0;j<3;++j){const int sb=a.fingers[f][j],dl=b.fingers[left][j],dr=b.fingers[right][j];
                        const auto next=[](const Skeleton& rig,const std::array<int,3>& chain,int joint){return joint<2?transformPoint(rig.bones[chain[joint+1]].restGlobal,{}):transformPoint(rig.bones[chain[2]].restGlobal,{})*2.f-transformPoint(rig.bones[chain[1]].restGlobal,{});};
                        const auto snext=next(s,a.fingers[f],j);fit(sb,dl,snext,next(d,b.fingers[left],j));const auto first=deform[sb];fit(sb,dr,snext,next(d,b.fingers[right],j));
                        for(int k=0;k<16;++k)deform[sb].v[k]=first.v[k]*(1-t)+deform[sb].v[k]*t;
                        map[sb]=dl;secondary[sb]=dr;secondaryWeight[sb]=t;
                    }
                    continue;
                }
                Vec3 sn=an,dn=bn,prevS{},prevD{};
                for(int j=0;j<2;++j){const auto sb=a.fingers[f][j],db=b.fingers[f][j];const auto sa=pos(s,a.fingers[f][j+1])-pos(s,sb),da=pos(d,b.fingers[f][j+1])-pos(d,db);
                    if(j==0){const auto sx=normalize(sa),dx=normalize(da);sn=normalize(an-sx*dot(an,sx));dn=normalize(bn-dx*dot(bn,dx));}else{sn=transport(sn,prevS,sa);dn=transport(dn,prevD,da);}
                    fit(sb,db,pos(s,sb)+sa,pos(d,db)+da,sn,dn);prevS=sa;prevD=da;
                }
                const auto last=a.fingers[f][2],previous=a.fingers[f][1],target=b.fingers[f][2],targetPrevious=b.fingers[f][1];
                auto sa=volume_fit::vector(s.bones[last].restGlobal,volume_fit::vector(inverseAffine(s.bones[previous].restGlobal),prevS));
                auto da=volume_fit::vector(d.bones[target].restGlobal,volume_fit::vector(inverseAffine(d.bones[targetPrevious].restGlobal),prevD));
                const auto skinAxis=[&](const CastScene& model,int bone,Vec3 fallback){Vec3 center{};float total{};const auto origin=pos(model.skeleton,bone);for(const auto& mesh:model.meshes)for(const auto& v:mesh.vertices){float w{};for(int k=0;k<4;++k)if(v.bones[k]==static_cast<std::uint32_t>(bone))w+=v.weights[k];if(w<.75f)continue;center+=transformPoint(mesh.modelTransform,v.position)*w;total+=w;}if(total>0){const auto axis=center/total-origin;if(length(axis)>.05f*length(fallback)&&length(axis)<3.f*length(fallback))return axis;}return fallback;};
                sa=skinAxis(skin,last,sa);da=skinAxis(driver,target,da);sn=transport(sn,prevS,sa);dn=transport(dn,prevD,da);fit(last,target,pos(s,last)+sa,pos(d,target)+da,sn,dn);
            }
        }
        // Some studio rigs skin to zero-length Root/eff bridges rather than
        // their named child joints. Those bridges belong to that child's
        // anatomical segment, not whichever mapped ancestor happens to exist.
        for(int side=0;side<2;++side){const auto& arm=from.arms[side];if(!arm.valid()||!to.arms[side].valid())continue;std::vector<int> joints{arm.elbow,arm.wrist};for(const auto& finger:arm.fingers)for(const int b:finger)if(b>=0)joints.push_back(b);
            const float tolerance=length(pos(s,arm.wrist)-pos(s,arm.elbow))*.01f;
            for(const int joint:joints){int p=s.bones[joint].parent;for(std::size_t n=0;p>=0&&static_cast<std::size_t>(p)<s.bones.size()&&n<s.bones.size();++n){if(map[p]>=0||length(pos(s,p)-pos(s,joint))>tolerance)break;map[p]=map[joint];deform[p]=deform[joint];p=s.bones[p].parent;}}
        }
        for(std::size_t i=0;i<s.bones.size();++i)if(map[i]<0){
            const auto name=assets::imported::lower(s.bones[i].name);int p=s.bones[i].parent;
            for(int side=0;side<2;++side){const std::string lr=side?"r":"l",cod=side?"ri":"le";
                if(!from.arms[side].valid()||!to.arms[side].valid())continue;
                if(name.find(lr+" foretwist")!=std::string::npos||name=="j_elbow_bulge_"+cod)p=from.arms[side].elbow;
                // Universal exports store absolute root tracks, so these skin
                // helpers cannot inherit an anatomical parent by traversal.
                if(name=="cs_hand_"+lr+".clavicle"&&from.arms[side].shoulder>=0)p=from.arms[side].shoulder;
                if(name.find(lr+"_wrist_helper")!=std::string::npos)p=from.arms[side].wrist;
                // Universal CS skins retain these native weighted bridges as
                // flat roots. Their wrist triangles must not be discarded.
                if(name=="cs_hand_bone_se_hand-"+lr)p=from.arms[side].wrist;
                if(name=="arm_upper_"+lr&&from.arms[side].shoulder>=0)p=from.arms[side].shoulder;
                // These exported helpers are flat roots, not independent
                // controls. Recover their nearest anatomical attachment only
                // within the explicitly named arm, never unrelated face parts.
                if(name.starts_with(std::string("cs_hand_")+(side?"right":"left")+"_point_")||name.starts_with("cs_hand_zombief_"+lr)||name=="cs_hand_zombief"+lr+"arm22"){
                    const auto& arm=from.arms[side];float best=std::numeric_limits<float>::max();
                    std::vector<int> candidates{arm.elbow,arm.wrist};for(const auto& finger:arm.fingers)for(int b:finger)if(b>=0)candidates.push_back(b);
                    for(int b:candidates){const auto distance=length(pos(s,static_cast<int>(i))-pos(s,b));if(distance<best){best=distance;p=b;}}
                }
                // GoldSrc and CSS insert weighted wrist bridge/root nodes just
                // before the digit fan. They belong to the hand, not elbow.
                const auto wrist=from.arms[side].wrist,elbow=from.arms[side].elbow;
                if(length(pos(s,static_cast<int>(i))-pos(s,wrist))<.15f*length(pos(s,wrist)-pos(s,elbow))){int ancestor=s.bones[wrist].parent;for(std::size_t n=0;ancestor>=0&&static_cast<std::size_t>(ancestor)<s.bones.size()&&n<s.bones.size();++n){if(ancestor==static_cast<int>(i)){p=wrist;break;}ancestor=s.bones[ancestor].parent;}}}
            for(std::size_t n=0;p>=0&&static_cast<std::size_t>(p)<s.bones.size()&&n<s.bones.size();++n){if(map[p]>=0){map[i]=map[p];deform[i]=deform[p];break;}p=s.bones[p].parent;}}
        if(preserveProximal){
            // Preserve arm-specific proximal skin on corresponding shoulder
            // ancestors. Shared torso/root geometry remains excluded; attaching
            // it to either moving arm would pull body geometry into view.
            std::array<std::vector<int>,2> chains;
            for(int side=0;side<2;++side)if(from.arms[side].shoulder>=0){
                int p=s.bones[from.arms[side].shoulder].parent;
                for(std::size_t n=0;p>=0&&n<s.bones.size();++n){chains[side].push_back(p);p=s.bones[p].parent;}
            }
            for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
                if(a.shoulder<0||b.shoulder<0)continue;
                int target=d.bones[b.shoulder].parent;
                for(int ancestor:chains[side]){
                    if(target<0)break;
                    const bool shared=std::find(chains[1-side].begin(),chains[1-side].end(),ancestor)!=chains[1-side].end();
                    if(map[ancestor]<0&&!shared){map[ancestor]=target;deform[ancestor]=deform[a.shoulder];}
                    target=d.bones[target].parent;
                }
            }
        }
        if(poseOnly)return assemblePoseOnlySkin(driver,skin,from,to,map,deform,secondary,secondaryWeight,error);
        if(diagnosticUpperRoll){
            for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
                if(a.shoulder<0||b.shoulder<0)continue;
                const auto target=d.boneByName.find(side?"j_shouldertwist_ri":"j_shouldertwist_le");
                if(target==d.boneByName.end()||d.bones[target->second].parent!=b.shoulder)continue;
                for(std::size_t i=0;i<s.bones.size();++i){
                    const auto name=assets::imported::lower(s.bones[i].name);
                    if(s.bones[i].parent==a.shoulder&&name.find("uparmtwist")!=std::string::npos){
                        map[i]=static_cast<int>(target->second);deform[i]=deform[a.shoulder];
                    }
                }
            }
        }
        std::vector<volume_fit::Bone> fits;fits.reserve(s.bones.size());
        for(std::size_t i=0;i<s.bones.size();++i)fits.push_back(volume_fit::prepare(deform[i],pos(s,static_cast<int>(i))));
        // The straightening rotation is about the elbow, not the discarded
        // shoulder. A shoulder-centred stretch would swell the blended seam.
        for(const auto& sleeve:straightSleeves)fits[sleeve.shoulder]=volume_fit::prepare(deform[sleeve.shoulder],sleeve.origin);
        // Distributed twist helpers sit along the forearm, not at its elbow.
        // Preserve that distribution through both bind fitting and animation.
for(std::size_t i=0;i<s.bones.size();++i){for(int side=0;side<2;++side){if(!from.arms[side].valid()||!to.arms[side].valid())continue;const auto name=assets::imported::lower(s.bones[i].name);if(!viewForearmTwist(name,side))continue;const auto& arm=from.arms[side];const auto axis=pos(s,arm.wrist)-pos(s,arm.elbow);const float t=std::clamp(dot(pos(s,static_cast<int>(i))-pos(s,arm.elbow),axis)/dot(axis,axis),0.f,1.f);const std::array<uint32_t,4> ids{static_cast<uint32_t>(arm.elbow),static_cast<uint32_t>(arm.wrist),0,0};const std::array<float,4> weights{1-t,t,0,0};Mat4 differential;const auto pivot=pos(s,static_cast<int>(i));const auto point=volume_fit::position(fits,pivot,ids,weights,&differential);const auto offset=point-volume_fit::vector(differential,pivot);differential.v[12]=offset.x;differential.v[13]=offset.y;differential.v[14]=offset.z;fits[i]=volume_fit::prepare(differential,pivot);map[i]=to.arms[side].elbow;secondary[i]=to.arms[side].wrist;secondaryWeight[i]=t;}}
        std::vector<Mesh> meshes;
        // A tiny torso residual must not punch holes in otherwise arm-owned
        // skin. Only discard an UNMAPPED shared ancestor contribution, below
        // one percent, with every remaining influence on the same arm.
        // Native assemblies and substantive torso/mixed-arm skin are unchanged.
        std::vector<int> residualOwner(s.bones.size(),-1);
        std::vector<bool> residualShared(s.bones.size());
        const auto ancestorOf=[&](int ancestor,int child){
            for(std::size_t n=0;child>=0&&std::size_t(child)<s.bones.size()&&n<s.bones.size();++n){if(child==ancestor)return true;child=s.bones[child].parent;}return false;
        };
        const bool foreign=driver.importedViewGame.empty()||driver.importedViewGame!=skin.importedViewGame;
        if(foreign&&from.arms[0].shoulder>=0&&from.arms[1].shoulder>=0)
            for(std::size_t i=0;i<s.bones.size();++i){
                residualShared[i]=map[i]<0&&ancestorOf(int(i),from.arms[0].shoulder)&&ancestorOf(int(i),from.arms[1].shoulder);
                for(int side=0;side<2;++side)if(map[i]>=0&&ancestorOf(from.arms[side].shoulder,int(i)))residualOwner[i]=side;
            }
        for(auto mesh:skin.meshes){if(mesh.viewmodelWeapon)continue;
            for(auto& vertex:mesh.vertices){
                float sum=0,residual=0;int owner=-1;bool safe=true;
                for(int k=0;k<4;++k){const float w=vertex.weights[k];if(!std::isfinite(w)||w<0){safe=false;break;}if(w==0)continue;
                    const auto b=vertex.bones[k];if(b>=map.size()){safe=false;break;}sum+=w;
                    if(residualShared[b]){residual+=w;continue;}
                    if(residualOwner[b]<0||(owner>=0&&owner!=residualOwner[b])){safe=false;break;}owner=residualOwner[b];
                }
                if(safe&&owner>=0&&residual>0&&residual<=.01f&&std::abs(sum-1.f)<1e-4f)
                    for(int k=0;k<4;++k)if(vertex.weights[k]>0)vertex.weights[k]=residualShared[vertex.bones[k]]?0.f:vertex.weights[k]/(sum-residual);
            }
            std::array<ProximalContinuityField,2> proximalFields;
            const bool automatic=automaticProximalContinuity&&!diagnosticUpperRoll&&!diagnosticArmTransport&&!diagnosticBendPlane&&!diagnosticNativeUpperWeights&&!diagnosticUniformArmRadii;
            if((diagnosticProximalContinuity||automatic)&&!diagnosticStraightProximalCarrier&&!diagnosticSpatialElbow&&!diagnosticFitOnlyElbow)
                for(int side=0;side<2;++side){const auto& sourceArm=from.arms[side];const auto& targetArm=to.arms[side];
                    if(!sourceArm.valid()||sourceArm.shoulder<0||!targetArm.valid()||targetArm.shoulder<0)continue;
                    if(!diagnosticProximalContinuity&&(!proximalContinuityTarget(d,targetArm)||map[sourceArm.shoulder]!=targetArm.shoulder||map[sourceArm.elbow]!=targetArm.elbow))continue;
                    auto field=proximalContinuityField(skin,mesh,sourceArm);
                    // Preflight the entire component before changing any skin.
                    // Unsupported/folded fields retain the old, loadable fit.
                    if(proximalContinuitySafe(field,mesh,fits,sourceArm))proximalFields[side]=std::move(field);
                }
            std::vector<bool> valid(mesh.vertices.size(),true);for(std::size_t i=0;i<mesh.vertices.size();++i){auto& v=mesh.vertices[i];float total{};const auto old=v.position;
                for(int k=0;k<4;++k)if(v.weights[k]>0){const auto bone=v.bones[k];if(bone>=map.size()||map[bone]<0){valid[i]=false;break;}total+=v.weights[k];}
                if(valid[i]&&total>.99f&&total<1.01f){Mat4 differential;auto fitBones=v.bones;auto fitWeights=v.weights;const auto point=transformPoint(mesh.modelTransform,old);const ProximalContinuityField* proximalField=nullptr;
                    for(int side=0;side<2;++side)if(i<proximalFields[side].affected.size()&&proximalFields[side].affected[i]){
                        proximalField=&proximalFields[side];const auto& a=from.arms[side];const float w=proximalField->weight(point);
                        fitBones={static_cast<uint32_t>(a.shoulder),static_cast<uint32_t>(a.elbow),0,0};fitWeights={1-w,w,0,0};v.bones=fitBones;v.weights=fitWeights;break;
                    }
                    if(diagnosticSpatialElbow||diagnosticFitOnlyElbow)for(int side=0;side<2;++side){
                        const auto& a=from.arms[side];const auto& b=to.arms[side];
                        if(!a.valid()||!b.valid()||a.shoulder<0||b.shoulder<0)continue;
                        bool armOnly=true;
                        for(int k=0;k<4;++k)if(v.weights[k]>0&&(secondaryWeight[v.bones[k]]>0||(map[v.bones[k]]!=b.shoulder&&map[v.bones[k]]!=b.elbow)))armOnly=false;
                        if(!armOnly)continue;
                        const auto elbow=pos(s,a.elbow),upper=pos(s,a.shoulder)-elbow,lower=pos(s,a.wrist)-elbow;
                        const auto bisector=normalize(normalize(lower)-normalize(upper));
                        const float width=.2f*std::min(length(upper),length(lower));
                        if(width<.001f||length(bisector)<.5f)continue;
                        const float t=std::clamp(.5f+.5f*dot(point-elbow,bisector)/width,0.f,1.f),w=t*t*(3-2*t);
                        fitBones={static_cast<uint32_t>(a.shoulder),static_cast<uint32_t>(a.elbow),0,0};fitWeights={1-w,w,0,0};
                        // Separate bind placement from animation weighting:
                        // the fit-only experiment leaves authored skin weights
                        // intact, unlike the earlier combined spatial probe.
                        if(!diagnosticFitOnlyElbow){v.bones=fitBones;v.weights=fitWeights;}
                        break;
                    }
                    // CoD elbow weights intentionally reach into its bent
                    // upper arm. Reusing those weights leaves a kink even
                    // after rotating the shoulder. Fit the sleeve by its
                    // anatomical segment, retaining native animation weights.
                    for(const auto& sleeve:straightSleeves){bool sleeveOnly=true;for(int k=0;k<4;++k)if(v.weights[k]>0&&map[v.bones[k]]!=sleeve.target&&map[v.bones[k]]!=sleeve.targetWrist)sleeveOnly=false;if(!sleeveOnly)continue;
                        const float t=std::clamp(.5f+.5f*dot(point-sleeve.origin,sleeve.split)/std::max(.001f,sleeve.width),0.f,1.f),w=t*t*(3-2*t);
                        const float along=dot(point-sleeve.origin,sleeve.forearm)/dot(sleeve.forearm,sleeve.forearm);
                        const float handT=std::clamp((along-.85f)/.15f,0.f,1.f),hand=handT*handT*(3-2*handT);
                        // Donor forearms often carry wrist influence far up
                        // the sleeve. A shoulderless rig needs one continuous
                        // forearm, with wrist rotation confined to the cuff.
                        fitBones={static_cast<uint32_t>(sleeve.elbow),static_cast<uint32_t>(sleeve.shoulder),static_cast<uint32_t>(sleeve.wrist),0};fitWeights={(1-w)*(1-hand),w*(1-hand),hand,0};
                        v.bones=fitBones;v.weights=fitWeights;break;}
                    v.position=volume_fit::position(fits,point,fitBones,fitWeights,&differential);
                    if(proximalField&&fitWeights[0]>0&&fitWeights[1]>0){
                        // Identical to component preflight, including the
                        // spatial derivative rather than constant skin weights.
                        ViewArmLayout fittedArm;fittedArm.shoulder=static_cast<int>(fitBones[0]);fittedArm.elbow=static_cast<int>(fitBones[1]);
                        proximalContinuitySample(*proximalField,fits,fittedArm,point,v.position,differential);
                    }
                    v.normal=normalize(volume_fit::vector(volume_fit::transpose3(inverseAffine(differential*mesh.modelTransform)),v.normal));std::vector<std::pair<uint32_t,float>> weights;const auto add=[&](int b,float w){if(w<=0)return;for(auto& item:weights)if(item.first==static_cast<uint32_t>(b)){item.second+=w;return;}weights.emplace_back(static_cast<uint32_t>(b),w);};for(int k=0;k<4;++k)if(v.weights[k]>0){const auto b=v.bones[k];add(map[b],v.weights[k]*(1-secondaryWeight[b]));if(secondary[b]>=0)add(secondary[b],v.weights[k]*secondaryWeight[b]);}std::sort(weights.begin(),weights.end(),[](const auto&a,const auto&b){return a.second>b.second;});v.bones={};v.weights={};float sum{};for(std::size_t k=0;k<std::min<std::size_t>(4,weights.size());++k){v.bones[k]=weights[k].first;v.weights[k]=weights[k].second;sum+=v.weights[k];}for(auto& w:v.weights)w/=sum;}else{valid[i]=false;v.bones={};v.weights={1,0,0,0};}}
            std::vector<uint32_t> triangles;for(std::size_t i=0;i+2<mesh.indices.size();i+=3){const auto x=mesh.indices[i],y=mesh.indices[i+1],z=mesh.indices[i+2];if(x>=valid.size()||y>=valid.size()||z>=valid.size())throw std::runtime_error("Invalid foreign hand triangle");if(valid[x]&&valid[y]&&valid[z])triangles.insert(triangles.end(),{x,y,z});}
            if(!triangles.empty()){mesh.indices=std::move(triangles);mesh.modelTransform=Mat4::identity();mesh.viewmodelWeapon=false;meshes.push_back(std::move(mesh));}}
        if(meshes.empty())throw std::runtime_error("No arm-weighted foreign hand triangles");
        if(diagnosticNativeUpperWeights)for(const auto& arm:to.arms){
            if(!arm.valid()||arm.shoulder<0)continue;
            const auto origin=pos(d,arm.shoulder),axis=pos(d,arm.elbow)-origin;
            const float extent2=dot(axis,axis);if(extent2<1e-8f)continue;
            std::vector<int> allowed{arm.shoulder,arm.elbow};
            for(std::size_t i=0;i<d.bones.size();++i){
                const auto name=assets::imported::lower(d.bones[i].name);
                if(name.find("twist")==std::string::npos&&name.find("roll")==std::string::npos)continue;
                int parent=d.bones[i].parent;bool upper=false;
                for(std::size_t n=0;parent>=0&&n<d.bones.size();++n){if(parent==arm.elbow||parent==arm.wrist)break;if(parent==arm.shoulder){upper=true;break;}parent=d.bones[parent].parent;}
                const auto offset=pos(d,static_cast<int>(i))-origin;const float t=dot(offset,axis)/extent2;
                if(upper&&t>=0&&t<=1&&length(offset-axis*t)<=.01f*std::sqrt(extent2))allowed.push_back(static_cast<int>(i));
            }
            constexpr int bins=32;
            std::array<float,bins> totals{};
            std::vector<std::array<float,bins>> profile(allowed.size());
            const auto slot=[&](uint32_t bone){const auto it=std::find(allowed.begin(),allowed.end(),static_cast<int>(bone));return it==allowed.end()?-1:static_cast<int>(it-allowed.begin());};
            const auto coordinate=[&](Vec3 p){return dot(p-origin,axis)/extent2;};
            std::vector<std::size_t> nativeSource(d.bones.size());for(std::size_t i=0;i<nativeSource.size();++i)nativeSource[i]=i;
            for(const auto& [follower,bone]:driver.nativePoseFollowers)if(follower<nativeSource.size()&&bone<nativeSource.size())nativeSource[follower]=bone;
            // Only native arm-only vertices contribute. A descendant armor
            // socket is not an arm joint just because it is near the sleeve.
            for(const auto& mesh:driver.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices){
                bool eligible=true;float sum=0;Vec3 point{};const auto raw=transformPoint(mesh.modelTransform,v.position);
                for(int k=0;k<4;++k)if(v.weights[k]>0){const auto bone=v.bones[k];if(bone>=nativeSource.size()){eligible=false;continue;}const auto mapped=nativeSource[bone];eligible&=slot(static_cast<uint32_t>(mapped))>=0;sum+=v.weights[k];point+=transformPoint(d.bones[mapped].restGlobal*d.bones[bone].inverseBind,raw)*v.weights[k];}
                const float t=coordinate(point);
                if(!eligible||sum<.99f||sum>1.01f||t<0||t>1)continue;
                const float x=t*(bins-1);const int lo=static_cast<int>(x),hi=std::min(lo+1,bins-1);const float blend=x-lo;
                totals[lo]+=1-blend;totals[hi]+=blend;
                for(int k=0;k<4;++k)if(v.weights[k]>0){const auto j=slot(static_cast<uint32_t>(nativeSource[v.bones[k]]));profile[j][lo]+=v.weights[k]*(1-blend)/sum;profile[j][hi]+=v.weights[k]*blend/sum;}
            }
            for(auto& mesh:meshes)for(auto& v:mesh.vertices){
                bool eligible=true;float sum=0;for(int k=0;k<4;++k)if(v.weights[k]>0){eligible&=slot(v.bones[k])>=0;sum+=v.weights[k];}
                const float t=coordinate(v.position);
                if(!eligible||sum<.99f||sum>1.01f||t<0||t>1)continue;
                const float x=t*(bins-1);int lo=static_cast<int>(x),hi=std::min(lo+1,bins-1);
                while(lo>=0&&totals[lo]<=0)--lo;while(hi<bins&&totals[hi]<=0)++hi;
                // No extrapolation beyond actual native coverage.
                if(lo<0||hi>=bins)continue;
                const float blend=hi==lo?0:std::clamp((x-lo)/(hi-lo),0.f,1.f);
                std::vector<std::pair<uint32_t,float>> weights;
                for(std::size_t j=0;j<allowed.size();++j){const float w=(1-blend)*profile[j][lo]/totals[lo]+blend*profile[j][hi]/totals[hi];if(w>0)weights.emplace_back(static_cast<uint32_t>(allowed[j]),w);}
                std::sort(weights.begin(),weights.end(),[](const auto& a,const auto& b){return a.second>b.second;});
                if(weights.empty())continue;
                v.bones={};v.weights={};float norm=0;for(std::size_t j=0;j<std::min<std::size_t>(4,weights.size());++j){v.bones[j]=weights[j].first;v.weights[j]=weights[j].second;norm+=weights[j].second;}for(auto& w:v.weights)w/=norm;
            }
        }
        // The optional upper profile consumes anatomical shoulder/elbow
        // weights. Apply distal roll afterwards so its helper does not make
        // the entire sleeve vertex ineligible for the diagnostic profile.
        for(const auto& arm:to.arms)if(arm.valid())transferNativeForearmRoll(driver,meshes,arm.elbow,arm.wrist);
        // Shoulderless skins can expose their authored open forearm rim in
        // full-arm animations. Continue only validated closed forearm boundaries
        // using their own surface UV strip, without moving existing geometry.
        // Fitted vertices already address the driver's anatomical palette;
        // native bind-space followers must not be assigned to the new rings.
        for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
            if(a.valid()&&b.valid()&&!viewHasWeightedUpperArm(skin,a))
                continueProximalDiagnostic(meshes,d,{b.shoulder,b.elbow,b.wrist,true},true,true);
        }
        const bool sameNativeGame=!driver.importedViewGame.empty()&&driver.importedViewGame==skin.importedViewGame;
        for(auto& mesh:meshes)for(auto& vertex:mesh.vertices){
            bool eligible=!sameNativeGame;float total{};
            for(int k=0;k<4;++k)if(vertex.weights[k]>0){bool armJoint=false;for(const auto& arm:to.arms)if(arm.valid())armJoint|=int(vertex.bones[k])==arm.shoulder||int(vertex.bones[k])==arm.elbow||int(vertex.bones[k])==arm.wrist;eligible&=armJoint;total+=vertex.weights[k];}
            vertex.armSkinning=eligible&&std::abs(total-1.f)<.001f?1.f:0.f;
        }
        auto output=driver.meshes;std::erase_if(output,[](const auto& m){return !m.viewmodelWeapon;});for(auto& m:meshes)output.push_back(std::move(m));driver.meshes=std::move(output);
        if(driver.pointBlankNativeCentimetres)driver.viewHandsDriverGame="pointblank";
        else if(driver.codmNativeCentimetres)driver.viewHandsDriverGame="codm";
        error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
inline bool fitPoseOnlyViewSkin(CastScene& driver,const CastScene& skin,std::string& error){
    // Split CS2 skins retain the weapon driver's authored bind and twist
    // helpers. Preserve a separate bind palette for compatible native skins:
    // an anatomical retarget replaces authored forearm roll unnecessarily.
    const auto cs2=[](const CastScene& s){for(const auto& p:s.sourceModelPath)if(assets::imported::lower(p.string())=="cs2")return true;return false;};
    if(cs2(driver)&&cs2(skin)){
        std::vector<int> remap(skin.skeleton.bones.size(),-1);bool compatible=true,populated=false;
        for(std::size_t i=0;i<remap.size();++i){auto found=driver.skeleton.boneByName.find(skin.skeleton.bones[i].name);if(found!=driver.skeleton.boneByName.end())remap[i]=static_cast<int>(found->second);}
        for(const auto& mesh:skin.meshes)if(!mesh.viewmodelWeapon)for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0){
            populated=true;const auto b=v.bones[k];if(b>=remap.size()){compatible=false;continue;}
            const auto& bone=skin.skeleton.bones[b];const auto found=driver.skeleton.boneByName.find(bone.name);
            if(found==driver.skeleton.boneByName.end()){int parent=bone.parent;for(std::size_t n=0;parent>=0&&n<remap.size();++n){if(remap[parent]>=0){remap[b]=remap[parent];break;}parent=skin.skeleton.bones[parent].parent;}if(remap[b]<0)compatible=false;continue;}
            const auto& other=driver.skeleton.bones[found->second];
            // Exported body and view skeletons have small bind differences.
            // Keep a separate inverse bind instead of deforming the geometry.
            for(int n=0;n<16;++n)if(!std::isfinite(bone.restGlobal.v[n])||!std::isfinite(other.restGlobal.v[n])||std::abs(bone.restGlobal.v[n]-other.restGlobal.v[n])>.05f)compatible=false;
            remap[b]=static_cast<int>(found->second);
        }
        if(compatible&&populated){
            auto adapter=std::make_shared<CodmRigAdapter>();adapter->firstBone=driver.skeleton.bones.size();adapter->identity="CS2 authored skin binding";
            for(std::size_t i=0;i<remap.size();++i){auto bone=skin.skeleton.bones[i];int mapped=remap[i];if(mapped<0){int parent=bone.parent;for(std::size_t n=0;parent>=0&&n<remap.size();++n){if(remap[parent]>=0){mapped=remap[parent];break;}parent=skin.skeleton.bones[parent].parent;}}if(mapped<0)mapped=0;
                CodmRigBinding binding;binding.source=static_cast<std::size_t>(mapped);binding.offset=inverseAffine(driver.skeleton.bones[mapped].restGlobal)*bone.restGlobal;adapter->bindings.push_back(binding);
                bone.name="cs2_skin|"+bone.name;if(bone.parent>=0)bone.parent+=static_cast<int>(adapter->firstBone);driver.skeleton.boneByName[bone.name]=driver.skeleton.bones.size();driver.skeleton.bones.push_back(std::move(bone));}
            std::vector<Mesh> meshes;for(auto mesh:skin.meshes)if(!mesh.viewmodelWeapon){for(auto& v:mesh.vertices)for(auto& b:v.bones)b+=static_cast<uint32_t>(adapter->firstBone);meshes.push_back(std::move(mesh));}
            std::erase_if(driver.meshes,[](const auto& m){return !m.viewmodelWeapon;});for(auto& mesh:meshes)driver.meshes.push_back(std::move(mesh));
            driver.codmRigAdapter=std::move(adapter);
            driver.warnings.push_back("CS2 shared bind: authored skin weights and twist bones retained");error.clear();return true;
        }
    }
    return fitForeignViewSkin(driver,skin,error,true,false,false,false,false,false,false,false,false,false,true,false,false,true);
}
}
