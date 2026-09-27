#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include "take/PoseInterpolation.h"
int main(){
 int failures=0;std::string error;take::Take t;if(!take::load("awawggg.c_dm",t,error)){std::cerr<<error;return 1;}
 std::cout<<"Samples "<<t.samples.size()<<" neutral "<<t.neutralGunPosition<<std::endl;
 for(int s=0;s<3;++s){const auto&a=t.actorSlots[s];std::cout<<"SLOT "<<s<<" bones "<<t.bonesForSlot(s)<<" base "<<a.baseModel<<std::endl;for(auto&p:a.rigModels)std::cout<<"PART "<<p<<std::endl;}
 glfwInit();glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(640,360,"Check",nullptr,nullptr);glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;a.window=w;a.deferSceneUpload=true;a.renderer.initialize(error);a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto game:{"pointblank","bo2","cs2","mw3"})assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error);
 for(int s=0;s<3;++s){const auto&m=t.actorSlots[s];if(m.empty())continue;
  auto index=[&](const std::string&p){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].path==std::filesystem::path(p))return i;return a.assetCatalog.entries.size();};
  a.selectedBaseAsset=index(m.baseModel);if(m.rigModels.empty())continue;auto wi=index(m.rigModels[0]);if(wi>=a.assetCatalog.entries.size()){std::cout<<"weapon missing"<<std::endl;continue;}
  equipViewWeapon(a,wi);const auto live=a.scene;
  if(!restoreTakeActor(a,m,t.bonesForSlot(s),error)){std::cout<<"RESTORE FAIL "<<error<<std::endl;++failures;continue;}
  float worst=0;size_t bad=0;for(size_t b=0;b<std::min(live.skeleton.bones.size(),a.scene.skeleton.bones.size());++b)for(int k=0;k<16;++k){auto d=std::abs(live.skeleton.bones[b].inverseBind.v[k]-a.scene.skeleton.bones[b].inverseBind.v[k]);if(d>worst){worst=d;bad=b;}}
  std::cout<<"BIND "<<s<<" error "<<worst<<" bone "<<live.skeleton.bones[bad].name<<" sizes "<<live.skeleton.bones.size()<<"/"<<a.scene.skeleton.bones.size()<<std::endl;
  failures+=worst>.002f||live.skeleton.bones.size()!=a.scene.skeleton.bones.size();
  float drift=0;std::string driftBone;for(auto&sample:t.samples)if(sample.weaponSlot==s){auto p=take::interpolatePose(sample.pose,sample.pose,.5f,&a.scene.skeleton);for(size_t b=0;b<p.size();++b)for(int k=0;k<16;++k){float d=std::abs(p[b].v[k]-sample.pose[b].v[k]);if(d>drift){drift=d;driftBone=a.scene.skeleton.bones[b].name;}}}
  std::cout<<"INTERPOLATION self error "<<drift<<" bone "<<driftBone<<std::endl;
  failures+=drift>.02f;
 }
 return failures?1:0;
}
