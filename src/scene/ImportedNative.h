#pragma once
#include "scene/CastScene.h"
#include "assets/ImportedGamePolicy.h"
#include "content/AssetPaths.h"
#include <algorithm>

namespace scene::imported {
inline bool explicitHandMesh(const std::string& name){const auto n=assets::imported::lower(name);return n.starts_with("hands/")||n.starts_with("rhand/")||n.starts_with("lhand/");}
inline void retainExplicitHandMeshes(CastScene& scene){
    for(auto& mesh:scene.meshes)if(assets::imported::lower(mesh.name).starts_with("weapon/")){
        // Some universal-hand exports retain legacy weapon/ mesh labels on
        // the arms. Explicit shared arm weights outrank that container label.
        bool armOnly=!mesh.vertices.empty();
        for(const auto& v:mesh.vertices){float armWeight{},total{};for(int k=0;k<4;++k)if(v.weights[k]>0){total+=v.weights[k];if(v.bones[k]<scene.skeleton.bones.size()){const auto n=assets::imported::lower(scene.skeleton.bones[v.bones[k]].name);if(n.starts_with("cs_hand_"))armWeight+=v.weights[k];}}if(total<=0||armWeight<total*.999f){armOnly=false;break;}}
        mesh.viewmodelWeapon=!armOnly;
    }
    if(!std::any_of(scene.meshes.begin(),scene.meshes.end(),[](const auto& m){return explicitHandMesh(m.name);}))return;
    std::vector<bool> handBones(scene.skeleton.bones.size());
    // Shared-hand exports intentionally flatten their tracks to root bones.
    // The namespace, not a missing parent chain, identifies sleeves/helpers.
    for(std::size_t i=0;i<handBones.size();++i)handBones[i]=assets::imported::lower(scene.skeleton.bones[i].name).starts_with("cs_hand_");
    for(const auto& mesh:scene.meshes)if(explicitHandMesh(mesh.name))for(const auto& v:mesh.vertices)for(int k=0;k<4;++k)if(v.weights[k]>0){int p=static_cast<int>(v.bones[k]);for(std::size_t n=0;p>=0&&static_cast<std::size_t>(p)<handBones.size()&&n<handBones.size();++n){handBones[p]=true;p=scene.skeleton.bones[p].parent;}}
    for(std::size_t i=0;i<handBones.size();++i)if(!handBones[i]){const auto name=assets::imported::lower(scene.skeleton.bones[i].name);
        if(name.find("twist")==std::string::npos&&name.find("sleave")==std::string::npos&&name.find("sleeve")==std::string::npos&&name.find("bulge")==std::string::npos)continue;
        int p=scene.skeleton.bones[i].parent;for(std::size_t n=0;p>=0&&static_cast<std::size_t>(p)<handBones.size()&&n<handBones.size();++n){if(handBones[p]){handBones[i]=true;break;}p=scene.skeleton.bones[p].parent;}}
    for(auto& mesh:scene.meshes)if(!explicitHandMesh(mesh.name)&&!mesh.viewmodelWeapon){
        std::vector<bool> belongs(mesh.vertices.size());for(std::size_t i=0;i<mesh.vertices.size();++i){float total{};const auto& v=mesh.vertices[i];for(int k=0;k<4;++k)if(v.bones[k]<handBones.size()&&handBones[v.bones[k]])total+=v.weights[k];belongs[i]=total>.999f;}
        std::vector<uint32_t> indices;for(std::size_t i=0;i+2<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];if(a<belongs.size()&&b<belongs.size()&&c<belongs.size()&&belongs[a]&&belongs[b]&&belongs[c])indices.insert(indices.end(),{a,b,c});}mesh.indices=std::move(indices);
    }
    std::erase_if(scene.meshes,[](const auto& m){return !explicitHandMesh(m.name)&&m.indices.empty();});
}
inline void orderAnonymousFingerRoots(const Skeleton& s,std::vector<int>& digits){
    if(digits.size()!=5)return;
    const auto position=[&](int n){return transformPoint(s.bones[n].restGlobal,{});};
    std::array<float,5> isolation{};for(int i=0;i<5;++i)for(int j=0;j<5;++j)isolation[i]+=length(position(digits[i])-position(digits[j]));
    const int thumb=digits[std::max_element(isolation.begin(),isolation.end())-isolation.begin()];
    std::erase(digits,thumb);std::sort(digits.begin(),digits.end());auto best=digits;float bestLength=1e30f,bestThumb=1e30f;
    // Curled knuckles are not radially ordered around the thumb. Recover their
    // adjacent row first, then resolve its two directions using the thumb.
    do{float row{};for(int i=0;i<3;++i)row+=length(position(digits[i+1])-position(digits[i]));const float fromThumb=length(position(digits[0])-position(thumb));
        if(row<bestLength-1e-5f||(std::abs(row-bestLength)<1e-5f&&fromThumb<bestThumb)){bestLength=row;bestThumb=fromThumb;best=digits;}
    }while(std::next_permutation(digits.begin(),digits.end()));
    digits={thumb};digits.insert(digits.end(),best.begin(),best.end());
}
inline std::string gameForPath(const std::filesystem::path& path){
    const auto logical=cadence::content::exportKey(path);
    if(const auto split=logical.find('/');split!=std::string::npos){const auto game=logical.substr(0,split);return assets::imported::supported(game)?game:std::string{};}
    if(cadence::content::hasExportMarker(path))return {};
    for(const auto& part:path){const auto game=assets::imported::lower(assets::imported::utf8(part));if(assets::imported::supported(game))return game;}return {};
}
inline void addViewCamera(const cast::Document& doc,CastScene& s){
    const auto path=std::filesystem::path(doc.sourceName());const auto game=gameForPath(path);
    if(assets::imported::role(game,path.stem().string())!=assets::Role::ViewHands)return;
    s.importedViewGame=game;
    // Some split GoldSrc hand exports retain weapon-labelled furniture.
    // Honour the exported mesh category, not ambiguous material words like forearm.
    if(assets::imported::sourceFamily(game))retainExplicitHandMeshes(s);
    for(const char* name:{"tag_view","tag_camera"})if(!s.skeleton.boneByName.contains(name)){
        Bone b;b.name=name;
        // Exported first-person poses are normalized to +X forward, +Z up.
        // Using the game's pre-export basis here rotates the viewer twice.
        b.restGlobal=trs(b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);b.inverseBind=inverseAffine(b.restGlobal);
        s.skeleton.boneByName[name]=s.skeleton.bones.size();s.skeleton.boneByCanonicalName[name]=s.skeleton.bones.size();s.skeleton.bones.push_back(b);
    }
}
inline bool dewExport(const cast::Document& doc){
    for(const auto& root:doc.roots())for(const auto& n:root.children){
        if(const auto* p=n.findProperty("dew2cast");p&&p->stringValue)return true;
        if(cast::nodeTypeName(n.identifier)=="Metadata")if(const auto* p=n.findProperty("s");p&&p->stringValue&&*p->stringValue=="dew2cast")return true;
    }
    return false;
}
// Blam source world units: 10 feet = 304.8 cm. Keep models, inverse binds
// and all native translation curves in one coordinate system, including sockets.
inline void normalizeDew(CastScene& s){
    if(s.importedTranslationScale!=1.f)return;
    constexpr float scale=304.8f;
    // Called only for embedded dew2cast provenance. Those exports use the
    // opposite V origin from our top-down texture upload. Convert once along
    // with native units; never clamp/fold tiled UVs or affect other exporters.
    for(auto& m:s.meshes){for(auto& v:m.vertices){v.position=v.position*scale;v.uv.y=1.f-v.uv.y;}for(const auto k:{12,13,14})m.modelTransform.v[k]*=scale;}
    for(auto& b:s.skeleton.bones){b.restLocal.position=b.restLocal.position*scale;b.absoluteTranslationOffset=b.absoluteTranslationOffset*scale;for(const auto k:{12,13,14})b.restGlobal.v[k]*=scale;b.inverseBind=inverseAffine(b.restGlobal);}
    s.importedTranslationScale=scale;
}
// The weapon export carries the authoritative native animated bind. Shared
// hands are re-based to that bind rather than replacing mechanism bones with
// similarly named bones from another weapon's hand export.
// GoldSrc's BoneNN identifiers are authoring-local, not anatomical names.
// Resolve the two arms only when mesh labels or explicit hand labels identify
// their sides and both wrists have the complete five three-joint digit layout.
inline std::vector<int> goldSrcHandCorrespondence(const CastScene& skin,const CastScene& gun){
    const auto& a=skin.skeleton;const auto& b=gun.skeleton;
    std::vector<int> mapping(a.bones.size(),-1);
    const auto rootOf=[](const Skeleton& s,int n){for(std::size_t count=0;n>=0&&count<s.bones.size();++count){const auto p=s.bones[n].parent;if(p<0)return n;n=p;}return -1;};
    const auto armRoot=[&](const CastScene& s,bool right){
        int named=-1;const auto side=right?"righthand":"lefthand";
        for(std::size_t i=0;i<s.skeleton.bones.size();++i)if(assets::imported::lower(s.skeleton.bones[i].name).find(side)!=std::string::npos){const auto r=rootOf(s.skeleton,static_cast<int>(i));if(named>=0&&r!=named)return -1;named=r;}
        if(named>=0)return named;
        std::vector<float> votes(s.skeleton.bones.size());
        for(const auto& m:s.meshes)if(assets::imported::lower(m.name).starts_with(right?"rhand/":"lhand/"))for(const auto& v:m.vertices)for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<votes.size()){const auto r=rootOf(s.skeleton,static_cast<int>(v.bones[k]));if(r>=0)votes[r]+=v.weights[k];}
        if(votes.empty())return -1;const auto it=std::max_element(votes.begin(),votes.end());if(*it<=0)return -1;
        float total{};for(const auto v:votes)total+=v;if(*it<total*.98f)return -1;
        return static_cast<int>(it-votes.begin());
    };
    const auto layout=[&](const Skeleton& s,int root){
        std::vector<int> result;if(root<0)return result;
        std::vector<std::vector<int>> children(s.bones.size());for(std::size_t i=0;i<s.bones.size();++i)if(s.bones[i].parent>=0)children[s.bones[i].parent].push_back(static_cast<int>(i));
        int wrist=-1;
        for(std::size_t i=0;i<s.bones.size();++i)if(rootOf(s,static_cast<int>(i))==root&&children[i].size()==5){
            bool valid=true;for(const auto finger:children[i]){auto n=finger;for(int depth=0;depth<2;++depth){if(children[n].size()!=1){valid=false;break;}n=children[n][0];}if(!children[n].empty())valid=false;}
            if(valid){if(wrist>=0)return result;wrist=static_cast<int>(i);}
        }
        if(wrist<0)return result;const int forearm=s.bones[wrist].parent;if(forearm<0||s.bones[forearm].parent!=root)return result;
        auto digits=children[wrist];orderAnonymousFingerRoots(s,digits);
        result={root,forearm,wrist};for(auto n:digits)for(int depth=0;depth<3;++depth){result.push_back(n);if(depth<2)n=children[n][0];}return result;
    };
    const auto ar=armRoot(skin,true),al=armRoot(skin,false),br=armRoot(gun,true),bl=armRoot(gun,false);
    if(ar<0||al<0||br<0||bl<0||ar==al||br==bl)return mapping;
    for(const auto pair:{std::pair{ar,br},std::pair{al,bl}}){const auto from=layout(a,pair.first),to=layout(b,pair.second);if(from.size()!=18||to.size()!=18)return std::vector<int>(a.bones.size(),-1);for(std::size_t i=0;i<from.size();++i)mapping[from[i]]=to[i];}
    return mapping;
}
inline bool assemblePrepared(CastScene gun,CastScene skin,CastScene& out,std::string& error){
    if(gun.meshes.empty()||skin.meshes.empty()){error="Missing native weapon or hand geometry";return false;}
    // Split exports may put rigid gun furniture in the companion hand file.
    // Recover only the selected gun's explicitly identical mesh family, never
    // furniture belonging to alternate native or foreign hand donors.
    const auto family=[](const std::string& name){const auto n=assets::imported::lower(name);const auto slash=n.rfind('/');return slash==std::string::npos?std::string{}:n.substr(0,slash+1);};
    std::erase_if(skin.meshes,[&](const auto& m){if(!m.viewmodelWeapon)return false;const auto f=family(m.name);if(f.empty())return true;bool match=false;for(const auto& g:gun.meshes){if(g.name==m.name)return true;match|=family(g.name)==f;}return !match;});
    const auto source=skin.skeleton;
    const bool goldSrc=skin.importedViewGame=="cs1.6"||skin.importedViewGame=="cz"||skin.importedViewGame=="cscz";
    const auto anatomical=goldSrc?goldSrcHandCorrespondence(skin,gun):std::vector<int>(source.bones.size(),-1);
    if(goldSrc){
        // When complete anatomical correspondence is unavailable, anonymous
        // BoneNN labels may only be reused with the same ancestry. Never turn
        // a thumb into an arm root merely because both exporters called it 04.
        std::vector<bool> checked(source.bones.size());
        for(const auto& mesh:skin.meshes)for(const auto& v:mesh.vertices)for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0){
            const auto index=v.bones[k];if(index>=source.bones.size()){error="Invalid native hand skin index";return false;}
            if(checked[index]||anatomical[index]>=0)continue;checked[index]=true;
            int from=static_cast<int>(index);std::size_t visited{};
            while(from>=0){
                if(static_cast<std::size_t>(from)>=source.bones.size()||++visited>source.bones.size()){error="Invalid native hand hierarchy";return false;}
                if(anatomical[from]>=0)break;
                const auto& bone=source.bones[from];const auto it=gun.skeleton.boneByName.find(bone.name);
                if(it==gun.skeleton.boneByName.end()){error="Native hand anatomy cannot be matched safely: "+bone.name;return false;}
                const auto targetParent=gun.skeleton.bones[it->second].parent;
                if((bone.parent<0)!=(targetParent<0)||(bone.parent>=0&&(static_cast<std::size_t>(bone.parent)>=source.bones.size()||static_cast<std::size_t>(targetParent)>=gun.skeleton.bones.size()||source.bones[bone.parent].name!=gun.skeleton.bones[targetParent].name))){error="Incompatible native hand hierarchy at "+bone.name;return false;}
                from=bone.parent;
            }
        }
    }
    std::vector<std::size_t> map(source.bones.size());
    for(std::size_t i=0;i<source.bones.size();++i){
        const auto& b=source.bones[i];
        if(anatomical[i]>=0)map[i]=static_cast<std::size_t>(anatomical[i]);
        else if(const auto it=gun.skeleton.boneByName.find(b.name);it!=gun.skeleton.boneByName.end())map[i]=it->second;
        else{
            auto added=b;
            if(b.parent>=0){if(static_cast<std::size_t>(b.parent)>=i){error="Native hands have a non-topological skeleton";return false;}added.parent=static_cast<int>(map[b.parent]);}
            map[i]=gun.skeleton.bones.size();gun.skeleton.boneByName[b.name]=map[i];
            auto canonical=b.name;std::transform(canonical.begin(),canonical.end(),canonical.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});gun.skeleton.boneByCanonicalName[canonical]=map[i];
            added.restGlobal=added.parent>=0?gun.skeleton.bones[added.parent].restGlobal*trs(added.restLocal.position,added.restLocal.rotation,added.restLocal.scale):trs(added.restLocal.position,added.restLocal.rotation,added.restLocal.scale);
            added.inverseBind=inverseAffine(added.restGlobal);gun.skeleton.bones.push_back(std::move(added));
        }
    }
    for(auto& m:gun.meshes)m.viewmodelWeapon=true;
    for(auto m:skin.meshes){
        for(auto& v:m.vertices){Vec3 p{},n{};float total{};for(std::size_t k=0;k<v.weights.size();++k){const auto b=v.bones[k];if(v.weights[k]<=0)continue;if(b>=map.size()){error="Invalid native hand skin index";return false;}
            const auto correction=gun.skeleton.bones[map[b]].restGlobal*source.bones[b].inverseBind;
            p=p+transformPoint(correction,v.position)*v.weights[k];
            const auto direction=transformPoint(correction,v.normal)-transformPoint(correction,{});n=n+direction*v.weights[k];total+=v.weights[k];v.bones[k]=static_cast<std::uint32_t>(map[b]);
        }if(total>0){v.position=p/total;v.normal=normalize(n);}}
        gun.meshes.push_back(std::move(m));
    }
    gun.importedViewGame=skin.importedViewGame;
    gun.viewHandsDriverGame=skin.viewHandsDriverGame;
    out=std::move(gun);return true;
}
inline bool assemble(const cast::Document& weapon,const cast::Document& hands,CastScene& out,std::string& error){return assemblePrepared(buildScene(weapon,false),buildScene(hands,false),out,error);}
}
