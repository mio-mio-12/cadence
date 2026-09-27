#pragma once
#include "scene/ImportedNative.h"
#include "scene/VolumePreservingFit.h"
#include <stdexcept>

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
inline ViewLayout viewLayout(const CastScene& scene){
    auto s=scene.skeleton;ViewLayout result;
    // Recover anatomical relationships from the retained original weapon rig.
    // Shared-hand animation tracks remain absolute roots; only this local
    // discovery copy receives parents, never the animated driver skeleton.
    for(std::size_t i=0;i<s.bones.size();++i){auto& hand=s.bones[i];if(!hand.name.starts_with("cs_hand_")||hand.parent>=0)continue;const auto suffix=hand.name.substr(8);int recovered=-1;bool ambiguous=false;
        for(const auto& native:scene.skeleton.bones){if(!native.name.starts_with("cs_weapon_")||!native.name.ends_with("_"+suffix)||native.parent<0)continue;if(length(transformPoint(native.restGlobal,{})-transformPoint(hand.restGlobal,{}))>.001f)continue;const auto prefix=native.name.substr(0,native.name.size()-suffix.size());const auto& parent=scene.skeleton.bones[native.parent];if(!parent.name.starts_with(prefix))continue;const auto it=s.boneByName.find("cs_hand_"+parent.name.substr(prefix.size()));if(it==s.boneByName.end())continue;const auto candidate=static_cast<int>(it->second);if(recovered>=0&&recovered!=candidate)ambiguous=true;recovered=candidate;}
        if(!ambiguous&&recovered>=0&&recovered!=static_cast<int>(i))hand.parent=recovered;
    }
    const auto find=[&](std::string name){const auto it=s.boneByName.find(name);return it==s.boneByName.end()?-1:static_cast<int>(it->second);};
    const auto pos=[&](int b){return transformPoint(s.bones[b].restGlobal,{});};
    std::vector<std::vector<int>> children(s.bones.size());for(std::size_t i=0;i<s.bones.size();++i)if(s.bones[i].parent>=0&&static_cast<std::size_t>(s.bones[i].parent)<s.bones.size())children[s.bones[i].parent].push_back(static_cast<int>(i));
    const std::array<const char*,5> digits{"thumb","index","middle","ring","pinky"};
    for(int side=0;side<2;++side){
        auto& a=result.arms[side];const std::string lr=side?"r":"l",cod=side?"ri":"le",word=side?"Right":"Left",cap=side?"R":"L";
        if(find("cs_hand_"+cap+".palm")>=0){
            const auto p="cs_hand_"+cap+".";
            a.shoulder=find(p+"upperarm");a.elbow=find(p+"forearm");a.wrist=find(p+"palm");
            std::vector<int> roots;
            for(int f=0;f<5;++f)roots.push_back(find(p+"finger"+std::to_string(f)+".0"));
            if(std::all_of(roots.begin(),roots.end(),[](int b){return b>=0;})){
                orderAnonymousFingerRoots(s,roots);
                for(int f=0;f<5;++f){a.fingers[f][0]=roots[f];const auto stem=s.bones[roots[f]].name.substr(0,s.bones[roots[f]].name.size()-1);for(int j=1;j<3;++j)a.fingers[f][j]=find(stem+std::to_string(j));}
            }
        }else if(find("j_wrist_"+cod)>=0){
            a.shoulder=find("j_shoulder_"+cod);a.elbow=find("j_elbow_"+cod);a.wrist=find("j_wrist_"+cod);
            const bool oneBased=find("j_metaindex_"+cod+"_1")>=0&&find("j_pinky_"+cod+"_0")<0;
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)a.fingers[f][j]=find("j_"+std::string(f==2?"mid":digits[f])+"_"+cod+"_"+std::to_string(j+(oneBased?1:0)));
        }else if(find(lr+"_hand")>=0){
            a.shoulder=find(lr+"_upperarm");a.elbow=find(lr+"_forearm");a.wrist=find(lr+"_hand");
            for(int f=0;f<5;++f)for(int j=0;j<3;++j)a.fingers[f][j]=find(lr+"_"+digits[f]+"_"+(j==0?"low":j==1?"mid":"tip"));
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
inline Mat4 viewAnatomicalFrame(Vec3 origin,Vec3 next,Vec3 normal){
    const auto x=normalize(next-origin);const auto y=normalize(cross(normal,x));if(length(x)<.5f||length(y)<.5f)throw std::runtime_error("Degenerate imported hand frame");
    const auto z=normalize(cross(x,y));Mat4 m=Mat4::identity();for(int i=0;i<3;++i){m.v[i]=i==0?x.x:i==1?x.y:x.z;m.v[4+i]=i==0?y.x:i==1?y.y:y.z;m.v[8+i]=i==0?z.x:i==1?z.y:z.z;}m.v[12]=origin.x;m.v[13]=origin.y;m.v[14]=origin.z;return m;
}
// Skin-only conversion: the driver skeleton, clips, camera and weapon
// mechanisms remain byte-for-byte unchanged. Fractional native sampling and
// all subsequent action layers therefore retain their existing behavior.
inline bool fitForeignViewSkin(CastScene& driver,const CastScene& skin,std::string& error){
    const auto to=viewLayout(driver),from=viewLayout(skin);if(!to.usable()||!from.usable()){error="Incomplete native/foreign viewhand anatomy";return false;}
    const auto& s=skin.skeleton;const auto& d=driver.skeleton;const auto pos=[](const Skeleton& rig,int b){return transformPoint(rig.bones[b].restGlobal,{});};
    std::vector<int> map(s.bones.size(),-1),secondary(s.bones.size(),-1);std::vector<float> secondaryWeight(s.bones.size());std::vector<Mat4> deform(s.bones.size(),Mat4::identity());
    struct StraightSleeve {int elbow,shoulder,wrist,target,targetWrist;Vec3 origin,split,forearm;float width;};
    std::vector<StraightSleeve> straightSleeves;
    try{
        for(int side=0;side<2;++side){const auto& a=from.arms[side];const auto& b=to.arms[side];
            if(!a.valid()||!b.valid())continue;
            // Never invent a target joint or collapse two visible fingers onto
            // one driver chain. Subset skins are supported; the reverse pairing
            // needs an explicitly authored extra-digit animation policy.
            for(std::size_t f=0;f<a.fingers.size();++f)if(a.hasFinger(f)&&!b.hasFinger(f))throw std::runtime_error("Native viewhand driver lacks a digit required by the selected skin");
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
                if(s.bones[sb].name.starts_with("cs_hand_")){
                    const auto extent=[](const CastScene& model,int bone,const Mat4& frame,int axis){std::vector<float> samples;const auto inv=inverseAffine(frame);for(const auto& mesh:model.meshes){if(mesh.viewmodelWeapon)continue;for(const auto& v:mesh.vertices){float weight{};for(int k=0;k<4;++k)if(v.bones[k]==static_cast<uint32_t>(bone))weight+=v.weights[k];if(weight<.5f)continue;const auto p=transformPoint(inv,transformPoint(mesh.modelTransform,v.position));samples.push_back(std::abs(axis==1?p.y:p.z));}}if(samples.size()<6)return 0.f;std::sort(samples.begin(),samples.end());return samples[(samples.size()-1)*9/10];};
                    const auto sy=extent(skin,sb,sourceFrame,1),sz=extent(skin,sb,sourceFrame,2),dy=extent(driver,db,targetFrame,1),dz=extent(driver,db,targetFrame,2);
                    if(sy>1e-5f&&dy>1e-5f)radialY=dy/sy;
                    if(sz>1e-5f&&dz>1e-5f)radialZ=dz/sz;
                }
                deform[sb]=targetFrame*scale({axial,radialY,radialZ})*inverseAffine(sourceFrame);map[sb]=db;};
            const auto palmCenter=[&](const Skeleton& rig,const ViewArmLayout& arm){return arm.hasFinger(2)?pos(rig,arm.fingers[2][0]):(pos(rig,arm.fingers[1][0])+pos(rig,arm.fingers[4][0]))*.5f;};
            fit(a.elbow,b.elbow,pos(s,a.wrist),pos(d,b.wrist));fit(a.wrist,b.wrist,palmCenter(s,a),palmCenter(d,b));
            if(a.shoulder>=0){if(b.shoulder>=0)fit(a.shoulder,b.shoulder,pos(s,a.elbow),pos(d,b.elbow));else{
                // Shoulderless view rigs author a straight sleeve continuing
                // behind the forearm. Reusing its transform preserved the
                // donor's bent upper arm (and exposed the open sleeve cap).
                // Anchor both segments at the elbow; only the donor skin is
                // straightened, never the native hand/weapon animation.
                const auto se=pos(s,a.elbow),de=pos(d,b.elbow);
                const float armScale=length(pos(d,b.wrist)-de)/length(pos(s,a.wrist)-se);
                deform[a.shoulder]=viewAnatomicalFrame(de,pos(d,b.wrist),bn)*scale({armScale,armScale,armScale})*
                    inverseAffine(viewAnatomicalFrame(se,se+(se-pos(s,a.shoulder)),an));
                map[a.shoulder]=b.elbow;
                straightSleeves.push_back({a.elbow,a.shoulder,a.wrist,b.elbow,b.wrist,se,
                    normalize(normalize(pos(s,a.shoulder)-se)-normalize(pos(s,a.wrist)-se)),
                    pos(s,a.wrist)-se,
                    length(pos(s,a.wrist)-se)*.12f});
            }}
            const auto transport=[](Vec3 normal,Vec3 from,Vec3 to){from=normalize(from);to=normalize(to);const float dotp=std::clamp(dot(from,to),-1.f,1.f);Quat q;if(dotp<-.9999f)q=fromAxisAngle(normal,kPi);else{const auto axis=cross(from,to);q=normalize(Quat{axis.x,axis.y,axis.z,1+dotp});}return normalize(volume_fit::vector(rotation(q),normal));};
            for(int f=0;f<5;++f){if(!a.hasFinger(f))continue;Vec3 sn=an,dn=bn,prevS{},prevD{};
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
                // GoldSrc and CSS insert weighted wrist bridge/root nodes just
                // before the digit fan. They belong to the hand, not elbow.
                const auto wrist=from.arms[side].wrist,elbow=from.arms[side].elbow;
                if(length(pos(s,static_cast<int>(i))-pos(s,wrist))<.15f*length(pos(s,wrist)-pos(s,elbow))){int ancestor=s.bones[wrist].parent;for(std::size_t n=0;ancestor>=0&&static_cast<std::size_t>(ancestor)<s.bones.size()&&n<s.bones.size();++n){if(ancestor==static_cast<int>(i)){p=wrist;break;}ancestor=s.bones[ancestor].parent;}}}
            for(std::size_t n=0;p>=0&&static_cast<std::size_t>(p)<s.bones.size()&&n<s.bones.size();++n){if(map[p]>=0){map[i]=map[p];deform[i]=deform[p];break;}p=s.bones[p].parent;}}
        std::vector<volume_fit::Bone> fits;fits.reserve(s.bones.size());
        for(std::size_t i=0;i<s.bones.size();++i)fits.push_back(volume_fit::prepare(deform[i],pos(s,static_cast<int>(i))));
        // The straightening rotation is about the elbow, not the discarded
        // shoulder. A shoulder-centred stretch would swell the blended seam.
        for(const auto& sleeve:straightSleeves)fits[sleeve.shoulder]=volume_fit::prepare(deform[sleeve.shoulder],sleeve.origin);
        // Distributed twist helpers sit along the forearm, not at its elbow.
        // Preserve that distribution through both bind fitting and animation.
        for(std::size_t i=0;i<s.bones.size();++i)if(assets::imported::lower(s.bones[i].name).find("foretwist")!=std::string::npos){for(int side=0;side<2;++side){if(!from.arms[side].valid()||!to.arms[side].valid())continue;const auto token=std::string(side?"r":"l")+" foretwist";if(assets::imported::lower(s.bones[i].name).find(token)==std::string::npos)continue;const auto& arm=from.arms[side];const auto axis=pos(s,arm.wrist)-pos(s,arm.elbow);const float t=std::clamp(dot(pos(s,static_cast<int>(i))-pos(s,arm.elbow),axis)/dot(axis,axis),0.f,1.f);const std::array<uint32_t,4> ids{static_cast<uint32_t>(arm.elbow),static_cast<uint32_t>(arm.wrist),0,0};const std::array<float,4> weights{1-t,t,0,0};Mat4 differential;const auto pivot=pos(s,static_cast<int>(i));const auto point=volume_fit::position(fits,pivot,ids,weights,&differential);const auto offset=point-volume_fit::vector(differential,pivot);differential.v[12]=offset.x;differential.v[13]=offset.y;differential.v[14]=offset.z;fits[i]=volume_fit::prepare(differential,pivot);map[i]=to.arms[side].elbow;secondary[i]=to.arms[side].wrist;secondaryWeight[i]=t;}}
        std::vector<Mesh> meshes;
        for(auto mesh:skin.meshes){if(mesh.viewmodelWeapon)continue;std::vector<bool> valid(mesh.vertices.size(),true);for(std::size_t i=0;i<mesh.vertices.size();++i){auto& v=mesh.vertices[i];float total{};const auto old=v.position;
                for(int k=0;k<4;++k)if(v.weights[k]>0){const auto bone=v.bones[k];if(bone>=map.size()||map[bone]<0){valid[i]=false;break;}total+=v.weights[k];}
                if(valid[i]&&total>.99f&&total<1.01f){Mat4 differential;auto fitBones=v.bones;auto fitWeights=v.weights;const auto point=transformPoint(mesh.modelTransform,old);
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
                    v.position=volume_fit::position(fits,point,fitBones,fitWeights,&differential);v.normal=normalize(volume_fit::vector(volume_fit::transpose3(inverseAffine(differential*mesh.modelTransform)),v.normal));std::vector<std::pair<uint32_t,float>> weights;const auto add=[&](int b,float w){if(w<=0)return;for(auto& item:weights)if(item.first==static_cast<uint32_t>(b)){item.second+=w;return;}weights.emplace_back(static_cast<uint32_t>(b),w);};for(int k=0;k<4;++k)if(v.weights[k]>0){const auto b=v.bones[k];add(map[b],v.weights[k]*(1-secondaryWeight[b]));if(secondary[b]>=0)add(secondary[b],v.weights[k]*secondaryWeight[b]);}std::sort(weights.begin(),weights.end(),[](const auto&a,const auto&b){return a.second>b.second;});v.bones={};v.weights={};float sum{};for(std::size_t k=0;k<std::min<std::size_t>(4,weights.size());++k){v.bones[k]=weights[k].first;v.weights[k]=weights[k].second;sum+=v.weights[k];}for(auto& w:v.weights)w/=sum;}else{valid[i]=false;v.bones={};v.weights={1,0,0,0};}}
            std::vector<uint32_t> triangles;for(std::size_t i=0;i+2<mesh.indices.size();i+=3){const auto x=mesh.indices[i],y=mesh.indices[i+1],z=mesh.indices[i+2];if(x>=valid.size()||y>=valid.size()||z>=valid.size())throw std::runtime_error("Invalid foreign hand triangle");if(valid[x]&&valid[y]&&valid[z])triangles.insert(triangles.end(),{x,y,z});}
            if(!triangles.empty()){mesh.indices=std::move(triangles);mesh.modelTransform=Mat4::identity();mesh.viewmodelWeapon=false;meshes.push_back(std::move(mesh));}}
        if(meshes.empty())throw std::runtime_error("No arm-weighted foreign hand triangles");
        auto output=driver.meshes;std::erase_if(output,[](const auto& m){return !m.viewmodelWeapon;});for(auto& m:meshes)output.push_back(std::move(m));driver.meshes=std::move(output);
        if(driver.pointBlankNativeCentimetres)driver.viewHandsDriverGame="pointblank";
        else if(driver.codmNativeCentimetres)driver.viewHandsDriverGame="codm";
        error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
