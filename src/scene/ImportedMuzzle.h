#pragma once
#include "scene/ImportedNative.h"
#include "scene/MuzzleGeometry.h"
#include <map>

namespace scene::imported {
inline void prepareMuzzleAnchors(CastScene& scene,const std::filesystem::path& path){
    const auto game=gameForPath(path);
    if(!assets::imported::supported(game))return;
    // Keep the model socket fixed within its moving parent: fire clips may
    // carry generic or zeroed socket tracks that do not describe this barrel.
    for(std::size_t i=0;i<scene.skeleton.bones.size();++i){
        const auto& b=scene.skeleton.bones[i];const auto n=assets::imported::lower(b.name);
        const bool gitsFx=game=="gitsfa"&&n.starts_with("gitsfa_socket__muzzle_fx__");
        const bool gitsBarrel=game=="gitsfa"&&n.starts_with("gitsfa_socket__barrel__");
        if(gitsBarrel&&std::any_of(scene.skeleton.bones.begin(),scene.skeleton.bones.end(),[](const auto& bone){return assets::imported::lower(bone.name).starts_with("gitsfa_socket__muzzle_fx__");}))continue;
        const bool halo=n.find("dew2cast_socket__")!=std::string::npos&&(n.ends_with("muzzle_flash__0")||n.ends_with("primary_trigger__0"));
        const bool cs=n=="muzzle"||n.ends_with("__muzzle")||n.ends_with("_muzzle_flash")||n.ends_with("_tag_flash")||(game=="dirtybomb"&&n=="socket_muzzle")||(game=="blacksquad"&&(n=="b_tip"||(n=="b_silencer"&&!scene.skeleton.boneByName.contains("b_tip"))))||(game=="gitsfa"&&n.starts_with("gitsfa_socket__barrel__"));
        if(!halo&&!cs&&!gitsFx)continue;
        const auto parent=b.parent>=0?static_cast<std::size_t>(b.parent):i;
        auto local=transformPoint(scene.skeleton.bones[parent].inverseBind,transformPoint(b.restGlobal,{}));
        if(gitsBarrel||(game=="blacksquad"&&n=="b_silencer")){
            // The exported barrel slot is the barrel's mounting point, not
            // its tip. Follow its native +X bore axis through gun geometry.
            for(const auto& mesh:scene.meshes)for(const auto& v:mesh.vertices){
                bool owned=false;for(int k=0;k<4;++k)if(v.weights[k]>.5f){int bone=static_cast<int>(v.bones[k]);for(std::size_t steps=0;bone>=0&&static_cast<std::size_t>(bone)<scene.skeleton.bones.size()&&steps<scene.skeleton.bones.size();++steps){if(static_cast<std::size_t>(bone)==parent){owned=true;break;}bone=scene.skeleton.bones[bone].parent;}}
                // Third-person GITS barrels may be skinned to a separate
                // model root, rather than beneath their slot bone. The
                // world-weapon asset contains no arms: its gun geometry is
                // still valid for the slot's bore query.
                if(game=="gitsfa"&&assets::imported::role(game,path.stem().string())==assets::Role::WorldWeapon&&!explicitHandMesh(mesh.name))owned=true;
                if(!owned)continue;const auto p=transformPoint(scene.skeleton.bones[parent].inverseBind,transformPoint(mesh.modelTransform,v.position));
                if(game=="gitsfa"){
                    if((p.y-local.y)*(p.y-local.y)+(p.z-local.z)*(p.z-local.z)<=4.f)local.x=std::max(local.x,p.x);
                }else{
                    // Black Squad's silencer socket sits at the attachment
                    // base; its parent-local bore is +Z. Include the fitted
                    // suppressor tip without drifting toward off-axis parts.
                    const float radius=.02f*scene.importedTranslationScale;
                    if((p.x-local.x)*(p.x-local.x)+(p.y-local.y)*(p.y-local.y)<=radius*radius)local.z=std::max(local.z,p.z);
                }
            }
        }
        const int side=n.find("__left__")!=std::string::npos?1:0;
        // Halo weapons without a muzzle-flash marker export the firing
        // origin as primary_trigger. Prefer a flash marker when both exist.
        const bool flash=n.ends_with("muzzle_flash__0");
        const bool hasFlash=std::any_of(scene.skeleton.bones.begin(),scene.skeleton.bones.end(),[&](const auto& other){const auto key=assets::imported::lower(other.name);return key.ends_with("muzzle_flash__0")&&(key.find("__left__")!=std::string::npos?1:0)==side;});
        if(halo&&!flash&&hasFlash)continue;
        scene.muzzleAnchors.push_back({parent,local,side});
    }
    if(scene.muzzleAnchors.empty()&&game=="blacksquad"){
        const auto role=assets::imported::role(game,path.stem().string());
        const auto category=assets::imported::additionalCategory(game,path.stem().string());
        if((role==assets::Role::ViewWeapon||role==assets::Role::WorldWeapon)&&category!="knives"&&category!="grenades"){
            std::vector<bool> arms(scene.skeleton.bones.size());
            const bool hasHands=std::any_of(scene.skeleton.bones.begin(),scene.skeleton.bones.end(),[](const auto& b){return assets::imported::lower(b.name).starts_with("j_shoulder_");});
            for(std::size_t i=0;i<arms.size();++i){int bone=static_cast<int>(i);for(std::size_t n=0;bone>=0&&static_cast<std::size_t>(bone)<arms.size()&&n<arms.size();++n){const auto name=assets::imported::lower(scene.skeleton.bones[bone].name);if(name.starts_with("j_")||(hasHands&&name=="b_root")){arms[i]=true;break;}bone=scene.skeleton.bones[bone].parent;}}
            // normalizeBlackSquad maps the exported UE_X_Z_Y forward axis
            // into +X. Explicit sockets above always outrank this fallback.
            if(const auto anchor=geometryMuzzle(scene,{1,0,0},arms))scene.muzzleAnchors.push_back(*anchor);
        }
    }
    if(!scene.muzzleAnchors.empty()||!assets::imported::sourceFamily(game)||
       (assets::imported::role(game,path.stem().string())!=assets::Role::ViewWeapon&&assets::imported::role(game,path.stem().string())!=assets::Role::WorldWeapon))return;
    const auto name=assets::imported::lower(path.stem().string());
    if(name.find("knife")!=std::string::npos||name.find("grenade")!=std::string::npos||name.find("bomb")!=std::string::npos)return;
    // The bind frame varies between GoldSrc/Source exporters. Their exported
    // first-person idle poses share +X camera forward. Locate the barrel there
    // and store it in its actual moving bone, rather than guessing a bind axis.
    if(assets::imported::role(game,path.stem().string())==assets::Role::ViewWeapon){
        std::vector<bool> arms(scene.skeleton.bones.size());
        for(std::size_t i=0;i<arms.size();++i)arms[i]=assets::imported::lower(scene.skeleton.bones[i].name).starts_with("cs_hand_");
        for(const auto& mesh:scene.meshes)if(explicitHandMesh(mesh.name))for(const auto& v:mesh.vertices)
            for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<arms.size())arms[v.bones[k]]=true;
        auto folder=path.parent_path();while(!folder.empty()&&folder.filename()!="models"&&folder!=folder.root_path())folder=folder.parent_path();
        if(folder.filename()=="models"){
            const auto filename=path.stem().string()+"_idle.cast";
            for(const auto& animationPath:{folder.parent_path()/"animations"/filename,folder.parent_path()/"animations"/"vm"/filename}){
                std::error_code ec;if(!std::filesystem::is_regular_file(animationPath,ec))continue;
                const auto document=cast::Document::load(animationPath);if(!document.valid())continue;
                CastScene reference;reference.skeleton=scene.skeleton;appendAnimations(document,reference);
                if(reference.animations.empty()||reference.animations.front().tracks.empty())continue;
                const auto pose=reference.samplePose(0,0);
                if(const auto anchor=geometryMuzzle(scene,{1,0,0},arms,&pose)){scene.muzzleAnchors.push_back(*anchor);return;}
            }
        }
    }
    // World/older exports can use +X or legacy -Y. Detect the longitudinal
    // horizontal axis so a sideways edge cannot become a firing origin.
    float front=std::numeric_limits<float>::max(),rear=-front;
    float minX=front,maxX=rear;
    for(const auto& m:scene.meshes)if(!explicitHandMesh(m.name))for(const auto& v:m.vertices){
        const auto p=transformPoint(m.modelTransform,v.position);front=std::min(front,p.y);rear=std::max(rear,p.y);
        minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);
    }
    if(maxX-minX>(rear-front)*1.5f){
        if(const auto anchor=geometryMuzzle(scene,{1,0,0},{})){scene.muzzleAnchors.push_back(*anchor);return;}
    }
    if(!std::isfinite(front)||rear-front<.01f)return;
    const float slice=std::max(.025f,(rear-front)*.003f);
    std::map<std::size_t,std::pair<Vec3,float>> groups;
    for(const auto& m:scene.meshes)if(!explicitHandMesh(m.name))for(const auto& v:m.vertices){
        const auto p=transformPoint(m.modelTransform,v.position);if(p.y>front+slice)continue;
        const auto influence=std::max_element(v.weights.begin(),v.weights.end())-v.weights.begin();
        if(v.weights[influence]<=0||v.bones[influence]>=scene.skeleton.bones.size())continue;
        auto& group=groups[v.bones[influence]];group.first=group.first+p;group.second+=1.f;
    }
    if(groups.empty())return;
    const auto chosen=std::max_element(groups.begin(),groups.end(),[](const auto& a,const auto& b){return a.second.second<b.second.second;});
    auto tip=chosen->second.first/chosen->second.second;tip.y=front;
    scene.muzzleAnchors.push_back({chosen->first,transformPoint(scene.skeleton.bones[chosen->first].inverseBind,tip),0});
}
}
