#include "cast/CastDocument.h"
#include "scene/CastScene.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <string_view>

int main(int argc,char** argv){
    if(argc>=5&&std::string_view(argv[1])=="--rig-inspect"){
        auto baseDocument=cast::Document::load(std::filesystem::u8path(argv[2]));if(!baseDocument.valid())return 1;auto rig=scene::buildScene(baseDocument);
        for(int i=3;i<argc-1;++i){auto part=cast::Document::load(std::filesystem::u8path(argv[i]));if(!part.valid()||!scene::appendRigModel(part,rig,std::filesystem::path(argv[i]).stem().string()))return 1;}
        auto animation=cast::Document::load(std::filesystem::u8path(argv[argc-1]));const auto added=scene::appendAnimations(animation,rig);std::cout<<"Bones: "<<rig.skeleton.bones.size()<<" meshes: "<<rig.meshes.size()<<" clips: "<<added<<'\n';
        if(added){const auto& clip=rig.animations.back();std::size_t absolute{},relative{},additive{};for(const auto& track:clip.tracks){if(track.mode==scene::TrackMode::Absolute)++absolute;else if(track.mode==scene::TrackMode::Relative)++relative;else ++additive;}
            std::cout<<clip.sourceName<<" fps="<<clip.framerate<<" durationFrames="<<clip.durationFrames<<" curves="<<clip.sourceCurveCount<<" mapped="<<clip.tracks.size()<<" unmapped="<<clip.unmappedCurveCount<<" absolute="<<absolute<<" relative="<<relative<<" additive="<<additive<<'\n';
            std::vector<std::size_t> trackCounts(rig.skeleton.bones.size());for(const auto& track:clip.tracks)if(track.boneIndex<trackCounts.size())++trackCounts[track.boneIndex];
            for(std::size_t i=0;i<trackCounts.size();++i)if(trackCounts[i]){const auto& bone=rig.skeleton.bones[i];std::cout<<"TRACKS "<<trackCounts[i]<<"  "<<bone.name<<"  parent="<<bone.parent<<" rest=("<<bone.restLocal.position.x<<','<<bone.restLocal.position.y<<','<<bone.restLocal.position.z<<")\n";}
            for(const auto& track:clip.tracks){if(track.boneIndex>=rig.skeleton.bones.size())continue;const auto& bone=rig.skeleton.bones[track.boneIndex];
                if(bone.name!="tag_origin"&&bone.name!="tag_weapon"&&bone.name!="tag_torso"&&bone.name!="tag_ads"&&bone.name!="tag_camera"&&bone.name!="j_gun"&&bone.name!="j_barrel"&&bone.name.find("bolt")==std::string::npos&&bone.name!="tag_flash"&&bone.name!="tag_clip1"&&bone.name!="tag_clip"&&bone.name!="tag_silencer"&&bone.name!="tag_sights")continue;
                const char* property=track.property==scene::TrackProperty::TranslationX?"tx":track.property==scene::TrackProperty::TranslationY?"ty":track.property==scene::TrackProperty::TranslationZ?"tz":track.property==scene::TrackProperty::Rotation?"rq":"scale";
                std::cout<<"CHANNEL "<<bone.name<<' '<<property<<" keys="<<track.frames.size();if(!track.scalarValues.empty()){const auto bounds=std::minmax_element(track.scalarValues.begin(),track.scalarValues.end());std::cout<<" first="<<track.scalarValues.front()<<" last="<<track.scalarValues.back()<<" min="<<*bounds.first<<" max="<<*bounds.second;}
                if(!track.rotationValues.empty())std::cout<<" firstQuat=("<<track.rotationValues.front().x<<','<<track.rotationValues.front().y<<','<<track.rotationValues.front().z<<','<<track.rotationValues.front().w<<")";std::cout<<'\n';}
            const auto visit=[&](const auto& self,const cast::Node& node)->void{if(const auto* property=node.findProperty("nn");property&&property->stringValue){std::string key=*property->stringValue;std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                    if(!rig.skeleton.boneByName.contains(*property->stringValue)&&!rig.skeleton.boneByCanonicalName.contains(key)){std::cout<<"UNMAPPED "<<*property->stringValue;if(const auto* channel=node.findProperty("kp");channel&&channel->stringValue)std::cout<<' '<<*channel->stringValue;if(const auto* values=node.findProperty("kv"))std::cout<<" values="<<animation.propertyPreview(*values,8);std::cout<<'\n';}}for(const auto& child:node.children)self(self,child);};
            for(const auto& root:animation.roots())visit(visit,root);
        }return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--inspect"){
        auto document=cast::Document::load(std::filesystem::u8path(argv[2]));if(!document.valid()){std::cerr<<"Invalid Cast file\n";return 1;}const auto inspected=scene::buildScene(document);
        std::vector<scene::Transform> rest;for(const auto& bone:inspected.skeleton.bones)rest.push_back(bone.restLocal);const auto derivedGlobals=inspected.globalPose(rest);float maxBindDifference{};
        for(std::size_t i=0;i<derivedGlobals.size();++i)for(std::size_t j=0;j<16;++j)maxBindDifference=std::max(maxBindDifference,std::abs(derivedGlobals[i].v[j]-inspected.skeleton.bones[i].restGlobal.v[j]));
        std::cout<<"Meshes: "<<inspected.meshes.size()<<"  bones: "<<inspected.skeleton.bones.size()<<"  animations: "<<inspected.animations.size()<<"  bind mismatch: "<<maxBindDifference<<'\n';
        for(std::size_t i=0;i<inspected.meshes.size();++i){const auto& mesh=inspected.meshes[i];scene::Vec3 minimum{},maximum{};scene::Vec2 uvMinimum{},uvMaximum{};bool hasBounds=false;for(const auto& vertex:mesh.vertices){if(!hasBounds){minimum=maximum=vertex.position;uvMinimum=uvMaximum=vertex.uv;hasBounds=true;}else{minimum={std::min(minimum.x,vertex.position.x),std::min(minimum.y,vertex.position.y),std::min(minimum.z,vertex.position.z)};maximum={std::max(maximum.x,vertex.position.x),std::max(maximum.y,vertex.position.y),std::max(maximum.z,vertex.position.z)};uvMinimum={std::min(uvMinimum.x,vertex.uv.x),std::min(uvMinimum.y,vertex.uv.y)};uvMaximum={std::max(uvMaximum.x,vertex.uv.x),std::max(uvMaximum.y,vertex.uv.y)};}}
            std::cout<<"MESH "<<i<<"  "<<mesh.name<<"  vertices="<<mesh.vertices.size()<<"  skinned="<<(mesh.skinned?"yes":"no")<<" bounds=("<<minimum.x<<','<<minimum.y<<','<<minimum.z<<")..("<<maximum.x<<','<<maximum.y<<','<<maximum.z<<')';
            std::cout<<" uv=("<<uvMinimum.x<<','<<uvMinimum.y<<")..("<<uvMaximum.x<<','<<uvMaximum.y<<") material="<<mesh.materialName<<" albedo="<<mesh.albedoPath.string();
            if(mesh.skinned&&!mesh.vertices.empty()){std::vector<float> influence(inspected.skeleton.bones.size());for(const auto& vertex:mesh.vertices)for(std::size_t j=0;j<vertex.bones.size();++j)if(vertex.bones[j]<influence.size())influence[vertex.bones[j]]+=vertex.weights[j];
                std::vector<std::size_t> order(influence.size());for(std::size_t bone=0;bone<order.size();++bone)order[bone]=bone;std::sort(order.begin(),order.end(),[&](auto a,auto b){return influence[a]>influence[b];});std::cout<<"  binds=";for(std::size_t rank=0;rank<std::min<std::size_t>(3,order.size())&&influence[order[rank]]>0.01f;++rank)std::cout<<inspected.skeleton.bones[order[rank]].name<<':'<<influence[order[rank]]<<' ';}std::cout<<'\n';}
        for(std::size_t i=0;i<inspected.skeleton.bones.size();++i){const auto& bone=inspected.skeleton.bones[i];std::cout<<i<<"  parent="<<bone.parent<<"  "<<bone.name<<"  local=("<<bone.restLocal.position.x<<','<<bone.restLocal.position.y<<','<<bone.restLocal.position.z<<") global=("<<bone.restGlobal.v[12]<<','<<bone.restGlobal.v[13]<<','<<bone.restGlobal.v[14]<<")\n";}
        for(const auto& warning:inspected.warnings)std::cout<<"WARN "<<warning<<'\n';
        const auto dumpNode=[&](const auto& self,const cast::Node& node)->void{if(node.identifier==0x6C74616D||node.identifier==0x656C6966||node.identifier==0x6873656D){std::cout<<"RAW "<<cast::nodeTypeName(node.identifier)<<" "<<node.displayName()<<" hash="<<node.hash<<'\n';for(const auto& property:node.properties)if(node.identifier!=0x6873656D||property.name.starts_with("u")||property.name=="m")std::cout<<"  "<<property.name<<" ["<<property.type<<"] "<<document.propertyPreview(property,12)<<'\n';}for(const auto& child:node.children)self(self,child);};for(const auto& root:document.roots())dumpNode(dumpNode,root);
        for(const auto& animation:inspected.animations)std::cout<<"ANIM "<<animation.name<<" mapped="<<animation.tracks.size()<<" unmapped="<<animation.unmappedCurveCount<<" motion="<<scene::motionRoleName(animation.motion)<<" stance="<<scene::stanceName(animation.stance)<<" direction="<<scene::directionName(animation.direction)<<" weapon="<<scene::weaponClassName(animation.weapon)<<'\n';return 0;
    }
    if(argc==4&&std::string_view(argv[1])=="--resolve"){
        auto modelDocument=cast::Document::load(std::filesystem::u8path(argv[2]));
        if(!modelDocument.valid()){std::cerr<<"Invalid model Cast file\n";return 1;}
        auto resolvedScene=scene::buildScene(modelDocument);const std::filesystem::path animationFolder=std::filesystem::u8path(argv[3]);std::error_code scanError;
        for(std::filesystem::recursive_directory_iterator it(animationFolder,std::filesystem::directory_options::skip_permission_denied,scanError),end;it!=end;it.increment(scanError)){
            if(scanError){scanError.clear();continue;}if(!it->is_regular_file(scanError)||it->path().extension()!=".cast")continue;
            auto animationDocument=cast::Document::load(it->path());if(animationDocument.valid())scene::appendAnimations(animationDocument,resolvedScene);
        }
        const scene::WeaponClass weaponTypes[]={scene::WeaponClass::Any,scene::WeaponClass::Rifle,scene::WeaponClass::Sniper,scene::WeaponClass::Automatic,scene::WeaponClass::Pistol,scene::WeaponClass::DualWield,scene::WeaponClass::Shotgun,scene::WeaponClass::M1216,scene::WeaponClass::Judge,scene::WeaponClass::G11,scene::WeaponClass::LMG,scene::WeaponClass::Crossbow,scene::WeaponClass::BallisticKnife,scene::WeaponClass::Knife,scene::WeaponClass::Grenade,scene::WeaponClass::Launcher,scene::WeaponClass::RiotShield,scene::WeaponClass::Heavy,scene::WeaponClass::Minigun,scene::WeaponClass::Equipment,scene::WeaponClass::Tablet,scene::WeaponClass::Radio,scene::WeaponClass::Briefcase,scene::WeaponClass::RC};
        const scene::ActionRole actionTypes[]={scene::ActionRole::Fire,scene::ActionRole::Reload,scene::ActionRole::Equip,scene::ActionRole::FirstRaise};
        for(const auto action:actionTypes)for(const auto weapon:weaponTypes)for(int adsPass=0;adsPass<(action==scene::ActionRole::Fire?2:1);++adsPass){scene::AnimationQuery query;query.domain=scene::AnimationDomain::PlayerTorso;query.motion=scene::MotionRole::Idle;query.action=action;query.weapon=weapon;query.stance=scene::Stance::Stand;query.direction=scene::Direction::Forward;query.ads=adsPass!=0;
            const auto match=scene::findBestAnimation(resolvedScene,query);std::cout<<scene::actionRoleName(action)<<(query.ads?" ADS":"")<<" / "<<scene::weaponClassName(weapon)<<": ";
            if(match){const auto& clip=resolvedScene.animations[*match];std::cout<<clip.sourceName<<" ("<<clip.tracks.size()<<" mapped, "<<clip.unmappedCurveCount<<" unmapped)\n";}else std::cout<<"unavailable\n";
        }
        std::cout<<"--- locomotion matrix (forward) ---\n";
        const scene::MotionRole movementTypes[]={scene::MotionRole::Idle,scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint,scene::MotionRole::Crawl};
        const scene::Stance stanceTypes[]={scene::Stance::Stand,scene::Stance::Crouch,scene::Stance::Prone};
        for(const auto stance:stanceTypes)for(const auto motion:movementTypes)for(const auto weapon:weaponTypes){scene::AnimationQuery query;query.domain=scene::AnimationDomain::PlayerBody;query.motion=motion;query.weapon=weapon;query.stance=stance;query.direction=scene::Direction::Forward;
            const auto match=scene::findBestAnimation(resolvedScene,query);std::cout<<scene::stanceName(stance)<<' '<<scene::motionRoleName(motion)<<" / "<<scene::weaponClassName(weapon)<<": ";
            if(match){const auto& clip=resolvedScene.animations[*match];std::cout<<clip.sourceName<<" ["<<scene::motionRoleName(clip.motion)<<", "<<scene::stanceName(clip.stance)<<", "<<scene::directionName(clip.direction)<<", "<<scene::weaponClassName(clip.weapon)<<"]\n";}else std::cout<<"unavailable\n";
        }
        return 0;
    }
    if(argc!=2){std::cerr<<"Usage: cast_catalog_report <folder>\n       cast_catalog_report --resolve <model.cast> <animation-folder>\n";return 2;}
    const std::filesystem::path folder=std::filesystem::u8path(argv[1]);
    std::array<std::size_t,4> domains{};std::array<std::size_t,14> motions{};
    std::array<std::size_t,18> actions{};std::array<std::size_t,24> weapons{};
    std::size_t files{},invalid{},clips{};std::error_code error;
    scene::CastScene scene;
    for(std::filesystem::recursive_directory_iterator it(folder,std::filesystem::directory_options::skip_permission_denied,error),end;it!=end;it.increment(error)){
        if(error){error.clear();continue;}
        if(!it->is_regular_file(error)||it->path().extension()!=".cast")continue;
        ++files;auto document=cast::Document::load(it->path());if(!document.valid()){++invalid;continue;}
        const auto before=scene.animations.size();clips+=scene::appendAnimations(document,scene);
        for(std::size_t i=before;i<scene.animations.size();++i){const auto& clip=scene.animations[i];
            ++domains[static_cast<std::size_t>(clip.domain)];++motions[static_cast<std::size_t>(clip.motion)];
            ++actions[static_cast<std::size_t>(clip.action)];++weapons[static_cast<std::size_t>(clip.weapon)];
        }
    }
    std::cout<<"Files: "<<files<<"  invalid: "<<invalid<<"  clips: "<<clips<<'\n';
    std::cout<<"Domains:";for(std::size_t i=0;i<domains.size();++i)if(domains[i])std::cout<<' '<<scene::animationDomainName(static_cast<scene::AnimationDomain>(i))<<'='<<domains[i];
    std::cout<<"\nMotions:";for(std::size_t i=0;i<motions.size();++i)if(motions[i])std::cout<<' '<<scene::motionRoleName(static_cast<scene::MotionRole>(i))<<'='<<motions[i];
    std::cout<<"\nActions:";for(std::size_t i=0;i<actions.size();++i)if(actions[i])std::cout<<' '<<scene::actionRoleName(static_cast<scene::ActionRole>(i))<<'='<<actions[i];
    std::cout<<"\nWeapons:";for(std::size_t i=0;i<weapons.size();++i)if(weapons[i])std::cout<<' '<<scene::weaponClassName(static_cast<scene::WeaponClass>(i))<<'='<<weapons[i];
    std::cout<<'\n';return invalid?1:0;
}
