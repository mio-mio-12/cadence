#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
 auto state=std::make_unique<AppState>();auto& a=*state;std::string error;
 const std::filesystem::path root=cadence::local_assets::exportPath("");
 CHECK(assets::appendScan(root/"codm","codm",a.assetCatalog,error));
 size_t paired=0,mounted=0;
 for(size_t i=0;i<a.assetCatalog.entries.size();++i){const auto& v=a.assetCatalog.entries[i];if(v.role!=assets::Role::ViewWeapon)continue;const auto wi=findWorldWeaponForViewWeapon(a,v);if(wi>=a.assetCatalog.entries.size())continue;++paired;
  for(int family=0;family<2;++family){auto actor=scene::buildScene(cast::Document::load(root/(family?"pointblank/models/playermodels/SWAT/playermode_SWAT_Male_fb/playermode_SWAT_Male_fb.cast":"bo2/models/playermodels/isa/c_usa_mp_isa_assault_fb/c_usa_mp_isa_assault_fb_LOD0.cast")),false);
   auto restored=actor;CHECK(attachCodmWorldWeapon(a,cast::Document::load(a.assetCatalog.entries[wi].path),i,actor));CHECK(actor.attachments.size()==1);++mounted;
   take::Take recording;recording.boneCount=1;recording.samples.resize(1);recording.samples[0].pose={scene::Mat4::identity()};captureCharacterAttachments(a,actor,recording.worldActor);CHECK(recording.worldActor.attachedModels.size()==1);
   CHECK(take::save(recording,"diagnostics/v145/world-roundtrip.casttake",error));take::Take loaded;CHECK(take::load("diagnostics/v145/world-roundtrip.casttake",loaded,error));CHECK(restoreCharacterAttachments(a,loaded.worldActor,restored,error));CHECK(restored.attachments.size()==1);
   const auto x=actor.attachments[0].localMatrix(),y=restored.attachments[0].localMatrix();for(int k=0;k<16;++k)CHECK(std::abs(x.v[k]-y.v[k])<.0001f);CHECK(actor.meshes.size()==restored.meshes.size());
  }
 }
 // M4A1 Techs has a world export/report but no viewmodel CAST in this fixture.
 CHECK(paired==22&&mounted==44);
 a.recordedTake.samples.resize(1);a.takePreview=true;a.cameraEditMode=true;a.actorMode=true;a.freeCameraActive=true;a.actorThirdPerson=true;
 for(int n=0;n<8;++n){toggleTakePerspective(a);CHECK(a.takeFirstPersonView==(n%2==0));CHECK(cadence::showPlayerWorldProxy(a.takePreview,a.takeFirstPersonView,a.cameraEditMode,a.actorMode,a.actorThirdPerson,a.freeCameraActive)==!a.takeFirstPersonView);}
 std::cout<<"Production pairing="<<paired<<" BO2/PB mounts="<<mounted<<"; serialized attachment restoration and repeated F8 transitions passed\n";
}
