#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

// Standalone diagnostic. Arguments: export-root output-dir [scale=2.4]
// [socket-local rearward distance=4] [animation.cast] [frame=last].
// Materials and attachment transforms use the ordinary production importer.
int main(int argc,char** argv){
    if(argc<3){std::cerr<<"Usage: worldmodel_visual_check export-root output-dir [scale] [rearward] [animation.cast] [frame]\n";return 2;}
    const auto root=std::filesystem::u8path(argv[1]),output=std::filesystem::u8path(argv[2]);
    const float scale=argc>3?std::stof(argv[3]):2.4f,rearward=argc>4?std::stof(argv[4]):4.0f;
    const auto playerPath=root/R"(bo2\models\playermodels\seal6\c_usa_mp_seal6_sniper_fb\c_usa_mp_seal6_sniper_fb_LOD0.cast)";
    const auto weaponPath=root/R"(cs2\models\weapons\awp\awp_model.cast)";
    const auto animationPath=argc>5?std::filesystem::u8path(argv[5]):root/R"(bo2\animations\pt\first raise\sniper\pt_sniper_stand_firstraise.cast)";
    const auto player=cast::Document::load(playerPath),weapon=cast::Document::load(weaponPath),animation=cast::Document::load(animationPath);
    if(!player.valid()||!weapon.valid()||!animation.valid()){std::cerr<<"Required model or animation unavailable\n";return 1;}
    auto value=scene::buildScene(player);const auto imported=scene::buildScene(weapon);
    std::size_t socket=0;
    for(const auto name:{"tag_weapon_right","tag_weapon","j_gun","tag_weapon_left"}){
        const auto it=value.skeleton.boneByName.find(name);if(it!=value.skeleton.boneByName.end()){socket=it->second;break;}
    }
    if(value.skeleton.bones.empty())return 1;
    scene::Vec3 grip{};
    for(const auto name:{"weapon_offset","tag_weapon","weapon","j_gun"}){
        const auto it=imported.skeleton.boneByCanonicalName.find(name);if(it!=imported.skeleton.boneByCanonicalName.end()){
            const auto& matrix=imported.skeleton.bones[it->second].restGlobal;grip={matrix.v[12],matrix.v[13],matrix.v[14]};break;
        }
    }
    scene::appendAttachment(weapon,value,socket,"CS2 AWP");if(value.attachments.empty())return 1;
    const auto animationIndex=value.animations.size();scene::appendAnimations(animation,value);if(animationIndex>=value.animations.size())return 1;
    const float frame=argc>6?std::stof(argv[6]):static_cast<float>(value.animations[animationIndex].durationFrames);
    const auto pose=value.samplePose(animationIndex,frame);
    std::filesystem::create_directories(output);
    std::ofstream report(output/"placement.txt");
    report<<"player="<<playerPath.string()<<"\nweapon="<<weaponPath.string()<<"\nanimation="<<animationPath.string()<<"\nframe="<<frame<<"\nsocket="<<value.skeleton.bones[socket].name<<"\nscale="<<scale<<"\nrearward_local_x="<<rearward<<"\ngrip="<<grip.x<<','<<grip.y<<','<<grip.z<<'\n';
    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(64,64,"Worldmodel diagnostic",nullptr,nullptr);if(!window){glfwTerminate();return 1;}glfwMakeContextCurrent(window);
    int status=0;
    {
        render::StageRenderer renderer;std::string error;
        if(!renderer.initialize(error)){std::cerr<<error;status=1;}
        else {
            renderer.setAntialiasing(true);renderer.setViewmodelCapture(true,{.06f,.06f,.06f,1});
            // Both variants share the same framing; only attachment placement changes.
            const auto& socketPose=pose[socket];const scene::Vec3 center{socketPose.v[12],socketPose.v[13],socketPose.v[14]};
            const std::array<scene::Vec3,3> directions{{{0,-1,0},{-1,-1,.3f},{0,0,1}}};
            const std::array<const char*,3> names{{"side","rear-quarter","top"}};
            for(int variant=0;variant<2;++variant){
                auto& attachment=value.attachments.back();const float activeScale=variant?scale:2.0f;
                attachment.scale={activeScale,activeScale,activeScale};attachment.position=grip*(-activeScale)+scene::Vec3{variant?-rearward:0,0,0};
                if(!renderer.loadScene(value,error)){std::cerr<<error;status=1;break;}
                renderer.setDebugView(0);
                for(std::size_t view=0;view<directions.size();++view){
                    const auto direction=scene::normalize(directions[view]),up=view==2?scene::Vec3{1,0,0}:scene::Vec3{0,0,1};
                    const auto camera=center+direction*180.0f;renderer.setCameraPosition(camera);
                    renderer.setSun(true,false,{-.4f,.3f,-1},1.0f,.7f,{1,1,1},{1,1,1},512,200,150,center,false,512,200,400,10);
                    const auto vp=scene::orthographic(-65,65,-48.75f,48.75f,.05f,1000)*scene::lookAt(camera,center,up);
                    renderer.render(value,pose,vp,1200,900,false,false,false);
                    const auto path=output/(std::string{variant?"candidate_":"baseline_"}+names[view]+".png");
                    if(!renderer.saveColorPng(path,error)){std::cerr<<error;status=1;}
                }
            }
        }
    }
    glfwDestroyWindow(window);glfwTerminate();return status;
}
