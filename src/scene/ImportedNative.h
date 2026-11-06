#pragma once
#include "scene/CastScene.h"
#include "scene/ImportedDirtyBomb.h"
#include "scene/BlackSquadActions.h"
#include "scene/ImportRevision.h"
#include "assets/ImportedGamePolicy.h"
#include "content/AssetPaths.h"
#include <algorithm>
#include <sstream>

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
        if(game=="gitsfa"){
            if(const auto camera=s.skeleton.boneByName.find("Camera_dummy");camera!=s.skeleton.boneByName.end())b.restLocal.position=transformPoint(s.skeleton.bones[camera->second].restGlobal,{});
            b.restLocal.rotation=fromEulerRadians({0,0,-kPi*.5f});
        }
        if(game=="dirtybomb")b.restLocal.rotation=fromEulerRadians({0,0,-kPi*.5f});
        // Exported first-person poses are normalized to +X forward, +Z up.
        // Using the game's pre-export basis here rotates the viewer twice.
        b.restGlobal=trs(b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);b.inverseBind=inverseAffine(b.restGlobal);
        s.skeleton.boneByName[name]=s.skeleton.bones.size();s.skeleton.boneByCanonicalName[name]=s.skeleton.bones.size();s.skeleton.bones.push_back(b);
    }
}
inline void parentSyntheticViewCamera(CastScene& s,const std::string& game){
    // Some GITS first-person rigs omit Camera_dummy but retain animated
    // character-space placement on their root. The generated camera must
    // follow that placement too, otherwise reloads move the whole gun away.
    if(game!="gitsfa"||s.skeleton.boneByName.contains("Camera_dummy"))return;
    const auto root=s.skeleton.boneByName.find("CharacterWorldSpaceTM");
    if(root==s.skeleton.boneByName.end())return;
    for(const char* name:{"tag_camera","tag_view"})if(const auto it=s.skeleton.boneByName.find(name);it!=s.skeleton.boneByName.end()){
        auto& b=s.skeleton.bones[it->second];
        const auto local=inverseAffine(s.skeleton.bones[root->second].restGlobal)*b.restGlobal;
        decomposeAffine(local,b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);
        b.parent=static_cast<int>(root->second);
    }
}
inline bool dewExport(const cast::Document& doc){
    for(const auto& root:doc.roots())for(const auto& n:root.children){
        if(const auto* p=n.findProperty("dew2cast");p&&p->stringValue)return true;
        if(cast::nodeTypeName(n.identifier)=="Metadata")if(const auto* p=n.findProperty("s");p&&p->stringValue&&*p->stringValue=="dew2cast")return true;
    }
    return false;
}
inline bool blackSquadMetres(const cast::Document& doc){
    for(const auto& root:doc.roots())for(const auto& n:root.children)if(const auto* p=n.findProperty("bs.units");p&&p->stringValue&&*p->stringValue=="meters")return true;
    return false;
}
inline void readDirtyBombMount(const cast::Document& doc,CastScene& s){
    if(gameForPath(std::filesystem::path(doc.sourceName()))!="dirtybomb")return;
    for(const auto& root:doc.roots())for(const auto& node:root.children){
        const auto* origin=node.findProperty("db.mesh_origin.native_cm");const auto* rot=node.findProperty("db.rotation_origin.native_rotator");
        if(!origin||!rot||!origin->stringValue||!rot->stringValue)continue;
        const auto vec=[](std::string text,Vec3& v){std::replace(text.begin(),text.end(),',',' ');std::istringstream in(text);return bool(in>>v.x>>v.y>>v.z);};
        Vec3 p,r;if(!vec(*origin->stringValue,p)||!vec(*rot->stringValue,r))continue;
        p.y=-p.y;constexpr float toRadians=2*kPi/65536.f;
        s.importedWeaponMount=rotation(fromEulerRadians({-r.z*toRadians,r.x*toRadians,-r.y*toRadians}))*translation(p);
    }
}
inline void appendDirtyBombParts(CastScene& s,const cast::Document& doc){
    const std::filesystem::path path(doc.sourceName());
    if(gameForPath(path)!="dirtybomb")return;
    if(dbProperty(doc,"db.attachment.part.0.child_source").empty()){
        if(assets::imported::role("dirtybomb",path.stem().string())==assets::Role::WorldWeapon){
            const auto key=assets::imported::additionalWeaponKey("dirtybomb",path.stem().string());std::error_code ec;
            for(const auto& entry:std::filesystem::directory_iterator(path.parent_path(),ec))if(entry.path().extension()==".cast"&&entry.path().filename().string().starts_with("viewmodel_")&&assets::imported::additionalWeaponKey("dirtybomb",entry.path().stem().string())==key){appendDirtyBombParts(s,cast::Document::load(entry.path()));break;}
        }return;
    }
    std::map<std::string,std::vector<unsigned>> slots;
    for(unsigned i=0;i<128;++i){const auto p="db.attachment.part."+std::to_string(i)+".";if(dbProperty(doc,p+"child_source").empty())break;slots[dbProperty(doc,p+"bone")].push_back(i);}
    std::map<std::string,std::filesystem::path> sources;std::error_code ec;
    for(const auto& entry:std::filesystem::directory_iterator(path.parent_path(),ec))if(entry.path().extension()==".cast"&&entry.path().filename().string().starts_with("part_")){
        const auto part=cast::Document::load(entry.path());const auto source=dbProperty(part,"db.source");if(!source.empty())sources.emplace(source,entry.path());
    }
    const auto vector=[](std::string text,Vec3 fallback){std::replace(text.begin(),text.end(),',',' ');std::istringstream in(text);Vec3 result;if(in>>result.x>>result.y>>result.z)return result;return fallback;};
    for(const auto& [bone,indices]:slots){
        if(indices.size()!=1){s.warnings.push_back("Dirty Bomb attachment needs selection: "+bone);continue;}
        const auto socket=s.skeleton.boneByName.find(bone);if(socket==s.skeleton.boneByName.end())continue;
        const auto p="db.attachment.part."+std::to_string(indices.front())+".";
        const auto found=sources.find(dbProperty(doc,p+"child_source"));if(found==sources.end())continue;
        const auto childDoc=cast::Document::load(found->second);auto part=buildScene(childDoc,false);
        auto location=vector(dbProperty(doc,p+"relative_location_cm"),{})*s.importedTranslationScale;location.y=-location.y;
        const auto r=vector(dbProperty(doc,p+"relative_rotator_ue3"),{});constexpr float rad=2*kPi/65536.f;
        auto correction=Mat4::identity();if(dbProperty(doc,p+"correction_baked")!="1"){auto text=dbProperty(doc,p+"child_mount_correction_4x4");std::replace(text.begin(),text.end(),',',' ');std::istringstream in(text);for(auto& v:correction.v)if(!(in>>v)){correction=Mat4::identity();break;}}
        for(int k:{12,13,14})correction.v[k]*=s.importedTranslationScale;
        const auto origin=dbProperty(doc,p+"child_origin_baked")=="1"?Mat4::identity():part.importedWeaponMount.value_or(Mat4::identity());
        const auto mount=s.skeleton.bones[socket->second].restGlobal*trs(location,fromEulerRadians({-r.z*rad,r.x*rad,-r.y*rad}),vector(dbProperty(doc,p+"relative_scale"),{1,1,1}))*correction*origin;
        for(auto& mesh:part.meshes){const auto basis=mount*mesh.modelTransform;for(auto& v:mesh.vertices){v.position=transformPoint(basis,v.position);v.normal=normalize(transformPoint(basis,v.normal)-transformPoint(basis,{}));v.bones={static_cast<uint32_t>(socket->second),0,0,0};v.weights={1,0,0,0};}mesh.modelTransform=Mat4::identity();mesh.skinned=true;mesh.viewmodelWeapon=true;s.meshes.push_back(std::move(mesh));}
    }
}
inline void normalizeBlackSquad(CastScene& s){
    if(s.importedTranslationScale!=1.f)return;
    // bs.basis=UE_X_Z_Y: native forward Z, left X, up Y -> Cadence X,Y,Z.
    // Export metre conversion retains the legacy half-height character scale:
    // PM02 body joints are exactly 0.5 of their matching CoD reference joints.
    const Quat q{.5f,.5f,.5f,.5f};const auto basis=rotation(q),inv=inverseAffine(basis);const float scale=activeImportRevision?200.f:100.f;
    for(auto& mesh:s.meshes){for(auto& v:mesh.vertices){v.position=transformPoint(basis,v.position*scale);v.normal=transformPoint(basis,v.normal);}for(int k=12;k<=14;++k)mesh.modelTransform.v[k]*=scale;mesh.modelTransform=basis*mesh.modelTransform*inv;}
    for(auto& b:s.skeleton.bones){b.restLocal.position=b.restLocal.position*scale;b.absoluteTranslationOffset=b.absoluteTranslationOffset*scale;for(int k=12;k<=14;++k)b.restGlobal.v[k]*=scale;b.restGlobal=basis*b.restGlobal;b.inverseBind=inverseAffine(b.restGlobal);if(b.parent<0){b.restLocal.position=transformPoint(basis,b.restLocal.position);b.restLocal.rotation=multiply(q,b.restLocal.rotation);b.absoluteTranslationOffset=transformPoint(basis,b.absoluteTranslationOffset);}}
    s.importedTranslationScale=scale;
}
inline void normalizeDirtyBomb(CastScene& s){
    if(s.importedTranslationScale!=1.f||!activeImportRevision)return;
    constexpr float factor=2.f; // Exported UE3 human body head is approximately 80 source units.
    for(auto& mesh:s.meshes){for(auto& v:mesh.vertices)v.position=v.position*factor;for(int k:{12,13,14})mesh.modelTransform.v[k]*=factor;}
    for(auto& b:s.skeleton.bones){b.restLocal.position=b.restLocal.position*factor;b.absoluteTranslationOffset=b.absoluteTranslationOffset*factor;for(int k:{12,13,14})b.restGlobal.v[k]*=factor;b.inverseBind=inverseAffine(b.restGlobal);}
    if(s.importedWeaponMount)for(int k:{12,13,14})s.importedWeaponMount->v[k]*=factor;
    s.importedTranslationScale=factor;
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
    if(skin.codmRigAdapter&&!skin.codmRigAdapter->positionLinks.empty()){
        // Replacement hands already retain this weapon's native driver plus
        // a separate donor palette. Mount its geometry without rebuilding or
        // remapping that palette through native hand correspondence again.
        std::vector<std::size_t> map(gun.skeleton.bones.size());
        for(std::size_t i=0;i<map.size();++i){
            const auto found=skin.skeleton.boneByName.find(gun.skeleton.bones[i].name);
            if(found==skin.skeleton.boneByName.end()||found->second>=skin.codmRigAdapter->firstBone){error="Prepared weapon driver missing joint: "+gun.skeleton.bones[i].name;return false;}
            map[i]=found->second;
        }
        for(auto& mesh:gun.meshes){mesh.viewmodelWeapon=true;
            for(auto& v:mesh.vertices){Vec3 p{},n{};float total{};
                for(int k=0;k<4;++k)if(v.weights[k]>0){const auto old=v.bones[k];if(old>=map.size()){error="Invalid prepared weapon skin index";return false;}
                    const auto correction=skin.skeleton.bones[map[old]].restGlobal*gun.skeleton.bones[old].inverseBind;
                    p+=transformPoint(correction,v.position)*v.weights[k];n+=(transformPoint(correction,v.normal)-transformPoint(correction,{}))*v.weights[k];total+=v.weights[k];v.bones[k]=static_cast<uint32_t>(map[old]);}
                if(total>0){v.position=p/total;v.normal=normalize(n);}
            }
        }
        skin.meshes.insert(skin.meshes.begin(),std::make_move_iterator(gun.meshes.begin()),std::make_move_iterator(gun.meshes.end()));
        out=std::move(skin);error.clear();return true;
    }
    if(skin.importedViewGame=="dirtybomb"){
        const auto socket=skin.skeleton.boneByName.find("Socket_RightHand");
        if(socket==skin.skeleton.boneByName.end()){error="Dirty Bomb reference lacks RightHand mount";return false;}
        // Weapon actions are exported in weapon-local space, separately from
        // hand actions. Preserve that hierarchy; never bind a gun at origin.
        constexpr const char* mountName="cadence_db_weapon_mount";
        if(!skin.skeleton.boneByName.contains(mountName)){
            Bone mount;mount.name=mountName;mount.parent=static_cast<int>(socket->second);
            if(!gun.importedWeaponMount){error="Dirty Bomb weapon lacks exported mount transform";return false;}
            decomposeAffine(*gun.importedWeaponMount,mount.restLocal.position,mount.restLocal.rotation,mount.restLocal.scale);
            mount.restGlobal=skin.skeleton.bones[socket->second].restGlobal**gun.importedWeaponMount;mount.inverseBind=inverseAffine(mount.restGlobal);
            skin.skeleton.boneByName[mountName]=skin.skeleton.bones.size();skin.skeleton.boneByCanonicalName[mountName]=skin.skeleton.bones.size();skin.skeleton.bones.push_back(mount);
        }
        const auto mount=skin.skeleton.boneByName.at(mountName);std::vector<std::size_t> indices(gun.skeleton.bones.size());
        for(std::size_t i=0;i<gun.skeleton.bones.size();++i){const auto& b=gun.skeleton.bones[i];if(const auto existing=skin.skeleton.boneByName.find(b.name);existing!=skin.skeleton.boneByName.end()){indices[i]=existing->second;continue;}
            auto copy=b;copy.parent=b.parent<0?static_cast<int>(mount):static_cast<int>(indices[b.parent]);copy.restGlobal=skin.skeleton.bones[copy.parent].restGlobal*trs(copy.restLocal.position,copy.restLocal.rotation,copy.restLocal.scale);copy.inverseBind=inverseAffine(copy.restGlobal);
            indices[i]=skin.skeleton.bones.size();skin.skeleton.boneByName[b.name]=indices[i];skin.skeleton.boneByCanonicalName[canonicalName(b.name)]=indices[i];skin.skeleton.bones.push_back(copy);
        }
        for(auto& mesh:gun.meshes){mesh.viewmodelWeapon=true;for(auto& v:mesh.vertices){Vec3 position{},normal{};float total{};for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<indices.size()){const auto old=v.bones[k];const auto transform=skin.skeleton.bones[indices[old]].restGlobal*gun.skeleton.bones[old].inverseBind;position=position+transformPoint(transform,v.position)*v.weights[k];normal=normal+(transformPoint(transform,v.normal)-transformPoint(transform,{}))*v.weights[k];total+=v.weights[k];v.bones[k]=static_cast<uint32_t>(indices[old]);}if(total>0){v.position=position/total;v.normal=normalize(normal);}}}
        for(auto anchor:gun.muzzleAnchors){if(anchor.bone<indices.size()){anchor.bone=indices[anchor.bone];skin.muzzleAnchors.push_back(anchor);}}
        skin.meshes.insert(skin.meshes.begin(),std::make_move_iterator(gun.meshes.begin()),std::make_move_iterator(gun.meshes.end()));out=std::move(skin);return true;
    }
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
inline void recoverCollapsedViewBind(CastScene& s,const std::filesystem::path& path){
    const auto game=gameForPath(path);
    if(game!="blacksquad"||assets::imported::role(game,path.stem().string())!=assets::Role::ViewWeapon)return;
    std::array<std::size_t,6> joints{};int n{};
    for(const auto* name:{"J_Shoulder_RI","J_Elbow_RI","J_Wrist_RI","J_Shoulder_LE","J_Elbow_LE","J_Wrist_LE"}){const auto found=s.skeleton.boneByName.find(name);if(found==s.skeleton.boneByName.end())return;joints[n++]=found->second;}
    const auto valid=[&](const auto& pose){for(int side:{0,3})for(int k:{0,1})if(length(transformPoint(pose[joints[side+k]],{})-transformPoint(pose[joints[side+k+1]],{}))<.01f)return false;return true;};
    std::vector<Mat4> bind;for(const auto& b:s.skeleton.bones)bind.push_back(b.restGlobal);if(valid(bind))return;
    std::vector<std::filesystem::path> candidates;std::error_code ec;
    for(const auto& entry:std::filesystem::directory_iterator(path.parent_path().parent_path()/"animations",ec))if(entry.path().extension()==".cast"&&assets::imported::matchesAnimation(game,path.stem().string(),entry.path().filename().string())&&assets::imported::additionalAction(game,entry.path().filename().string())=="idle"&&assets::imported::lower(entry.path().filename().string()).find("add_")==std::string::npos)candidates.push_back(entry.path());
    std::sort(candidates.begin(),candidates.end());
    for(const auto& clip:candidates){const auto before=s.animations.size();appendAnimations(cast::Document::load(clip),s);if(s.animations.size()==before)continue;auto pose=s.samplePose(before,0);s.animations.resize(before);
        if(!valid(pose)||std::any_of(pose.begin(),pose.end(),[](const auto& m){return std::any_of(m.v.begin(),m.v.end(),[](float v){return !std::isfinite(v);});}))continue;
        // Some exports store zero-length arm bind segments but a complete
        // absolute idle. Rebase all geometry and inverse binds together, once;
        // never inject guessed lengths or weapon-specific offsets.
        for(auto& mesh:s.meshes)if(mesh.skinned)for(auto& v:mesh.vertices){Vec3 p{},normal{};float total{};for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){const auto m=pose[v.bones[k]]*s.skeleton.bones[v.bones[k]].inverseBind;p=p+transformPoint(m,v.position)*v.weights[k];normal=normal+(transformPoint(m,v.normal)-transformPoint(m,{}))*v.weights[k];total+=v.weights[k];}if(total>0){v.position=p/total;v.normal=normalize(normal);}}
        for(std::size_t i=0;i<pose.size();++i){auto& b=s.skeleton.bones[i];const auto local=b.parent>=0?inverseAffine(pose[b.parent])*pose[i]:pose[i];decomposeAffine(local,b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);b.restGlobal=pose[i];b.inverseBind=inverseAffine(pose[i]);}
        s.warnings.push_back("Recovered collapsed view bind from authored idle: "+clip.filename().string());return;
    }
}
inline void appendGitsParts(CastScene& s,const std::filesystem::path& path){
    const auto n=assets::imported::lower(path.stem().string());const auto role=assets::imported::role("gitsfa",n);
    if(gameForPath(path)!="gitsfa"||(role!=assets::Role::ViewWeapon&&role!=assets::Role::WorldWeapon))return;
    const auto hash=n.find("__");if(hash==std::string::npos||hash+10>n.size())return;
    const auto prefix="gitsfa__"+assets::imported::additionalWeaponKey("gitsfa",n)+(role==assets::Role::ViewWeapon?"__1p__":"__3p__")+n.substr(hash+2,8)+"__att__";
    std::map<std::string,std::vector<std::filesystem::path>> slots;std::error_code ec;
    for(std::filesystem::directory_iterator it(path.parent_path(),ec),end;!ec&&it!=end;it.increment(ec)){const auto file=assets::imported::lower(it->path().filename().string());if(it->path().extension()!=".cast"||!file.starts_with(prefix))continue;const auto endSlot=file.find("__",prefix.size());if(endSlot!=std::string::npos)slots[file.substr(prefix.size(),endSlot-prefix.size())].push_back(it->path());}
    for(const auto& [slot,files]:slots){if(files.size()!=1){s.warnings.push_back("Ambiguous GITS attachment slot: "+slot);continue;}const auto socketPrefix="gitsfa_socket__"+slot+"__";std::size_t socket=s.skeleton.bones.size();for(std::size_t b=0;b<s.skeleton.bones.size();++b)if(s.skeleton.bones[b].name.starts_with(socketPrefix)){socket=b;break;}if(socket==s.skeleton.bones.size())continue;
        const auto doc=cast::Document::load(files.front());if(!doc.valid())continue;auto part=buildScene(doc,false);
        for(auto& mesh:part.meshes){const auto basis=s.skeleton.bones[socket].restGlobal*mesh.modelTransform;for(auto& v:mesh.vertices){v.position=transformPoint(basis,v.position);v.normal=normalize(transformPoint(basis,v.normal)-transformPoint(basis,{}));v.bones={static_cast<uint32_t>(socket),0,0,0};v.weights={1,0,0,0};}mesh.modelTransform=Mat4::identity();mesh.skinned=true;mesh.viewmodelWeapon=true;s.meshes.push_back(std::move(mesh));}
    }
}
}
