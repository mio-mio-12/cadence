#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv){
 if(argc<2||!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,720,"Weapon audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;app.deferSceneUpload=true;app.defaultSalukiDirectory="exported_files";std::string error;
 assets::appendScan(app.defaultSalukiDirectory/"bo2","bo2",app.assetCatalog,error);assets::appendScan(app.defaultSalukiDirectory/"bo","bo",app.assetCatalog,error);assets::appendScan(app.defaultSalukiDirectory/"iw_sp","iw_sp",app.assetCatalog,error);
 size_t hands=SIZE_MAX;for(size_t i=0;i<app.assetCatalog.entries.size();++i)if(app.assetCatalog.entries[i].role==assets::Role::ViewHands&&app.assetCatalog.entries[i].name.find("seal6_longsleeve")!=std::string::npos){hands=i;break;}if(hands==SIZE_MAX)return 3;
 int failures=0;for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto asset=app.assetCatalog.entries[i];if(asset.role!=assets::Role::ViewWeapon||asset.name.find(argv[1])==std::string::npos)continue;
 app.selectedBaseAsset=hands;app.scene={};app.animationDocuments.clear();app.animationSources.clear();app.selectedWeaponAsset=SIZE_MAX;equipViewWeapon(app,i);
 std::cout<<"WEAPON "<<asset.name<<" prefix="<<animationPrefixForWeapon(asset)<<" bones="<<app.scene.skeleton.bones.size()<<" clips="<<app.scene.animations.size()<<std::endl;
 for(const char* slot:{"idle","pullout","first_raise","fire","reload"}){const auto index=findViewmodelClip(app,slot);std::cout<<slot<<"="<<(index?app.scene.animations[*index].sourceName:"MISSING")<<std::endl;if(!index){++failures;continue;}for(float f:{0.f,.5f,1.f}){const auto p=app.scene.samplePose(*index,app.scene.animations[*index].durationFrames*f);for(const auto& m:p)for(float v:m.v)if(!std::isfinite(v))++failures;}}
 for(const auto& warning:app.scene.warnings)std::cout<<"WARNING "<<warning<<std::endl;
 }
 return failures?1:0;
}
