#include "weapon/WeaponProfile.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(){int failures{};const auto expect=[&](bool value,const char* message){if(!value){std::cerr<<message<<'\n';++failures;}};
  expect(weapon::inferArchetype("weapon_arx_160_LOD0")==weapon::Archetype::Rifle,"Ghosts separated ARX-160 identity must retain rifle posture");
  expect(weapon::inferArchetype("viewmodel_AR_AK-47_DualMag")==weapon::Archetype::Rifle,"DualMag is a rifle magazine, not dual wield");
  expect(weapon::inferArchetype("viewmodel_Dual_DesertEagle")==weapon::Archetype::DualWield,"real dual weapons retain dual wield");
  {auto family=weapon::makeGenerated("family",weapon::Archetype::BoltSniper,"vm_sniper_standard_t9_");
   family.stats.fireTime=.91f;family.stats.adsKickPitchMin=2.3f;family.animations["fire"]="chosen.cast";family.animationVariants["inspect"]={"one.cast","two.cast"};family.animationFiles["chosen.cast"]="D:/foreign/chosen.cast";
   auto variant=weapon::makeGenerated("blueprint",weapon::Archetype::Rifle);variant.baseModel="blueprint.cast";variant.rigModels={"blueprint_mag.cast"};variant.animationOffsets.push_back({"chosen.cast",{1,2,3}});
   weapon::applyFamilyBehavior(variant,family);
   expect(variant.animationFiles==family.animationFiles,"foreign animation dependencies must propagate to variants");
   expect(variant.stats.fireTime==.91f&&variant.stats.adsKickPitchMin==2.3f&&variant.animations==family.animations&&variant.animationVariants==family.animationVariants,"family timing, recoil, and animation choices must propagate");
   expect(variant.name=="blueprint"&&variant.baseModel=="blueprint.cast"&&variant.rigModels.front()=="blueprint_mag.cast"&&variant.animationOffsets.front().position.x==1,"family behavior must preserve variant model and rig fitting");}
  auto profile = weapon::makeGenerated("AN94 custom", weapon::Archetype::Rifle,
                                       "viewmodel_an94_");
  expect(!profile.materials.overrideMetalness,"metalness override must default off");
  {
    const auto bad=std::filesystem::temp_directory_path()/"cadence_v139_invalid_metalness.iwweapon";
    for(const auto*value:{"1 2","1 nan","2 .5","1"}){
      {std::ofstream f(bad);f<<"IWWEAPON 1\nmaterial metalness_override "<<value<<'\n';}
      auto unchanged=profile;std::string error;expect(!weapon::load(bad,unchanged,error),"invalid metalness must fail");expect(unchanged.name==profile.name,"failed metalness parsing must preserve profile");
    }
    for(const auto*value:{"1 .2 .8 0","1 nan .8 1","1 .2 2 1","2 .2 .8 1","1 .2"}){
      {std::ofstream f(bad);f<<"IWWEAPON 1\nmaterial metalness_diffuse "<<value<<'\n';}
      auto unchanged=profile;std::string error;expect(!weapon::load(bad,unchanged,error),"invalid diffuse metalness must fail");expect(unchanged.name==profile.name,"failed diffuse levels must preserve profile");
    }
    {std::ofstream f(bad);f<<"IWWEAPON 1\nname old\n";}
    auto legacy=profile;legacy.materials.overrideMetalness=true;std::string error;expect(weapon::load(bad,legacy,error)&&!legacy.materials.overrideMetalness,"legacy profile must retain original material behavior");
    std::error_code ec;std::filesystem::remove(bad,ec);
  }
  expect(std::abs(profile.stats.yyReturnScale - 1.5f) < .0001f,
         "generated weapons default YY return multiplier to 1.5");
  profile.source = "t6:an94_mp";
  profile.viewModelName = "t6_wpn_ar_an94_view";
  profile.worldModelName = "t6_wpn_ar_an94_world";
  profile.handModelName = "viewhands_seal6";
  profile.baseModel = "hands.cast";
  weapon::RigMount attachment;attachment.attachedModel=true;attachment.model="scope";attachment.parentTag="tag_scope";attachment.position={1,2,3};attachment.attachmentRotationDegrees={10,20,30};attachment.attachmentScale={1,2,1};profile.rigMounts.push_back(attachment);
  profile.rigModels = {"an94.cast", "holo.cast"};
  profile.gunPosition = {1.25f, -2.5f, 3.75f};
  profile.separateAdsPosition=true;profile.adsGunPosition={-4,5,6};
  expect(scene::length(weapon::gunPositionAt(profile,0)-profile.gunPosition)<.00001f,"hip position endpoint");
  expect(scene::length(weapon::gunPositionAt(profile,1)-profile.adsGunPosition)<.00001f,"ADS position endpoint");
  expect(scene::length(weapon::gunPositionAt(profile,.5f)-(profile.gunPosition+profile.adsGunPosition)*.5f)<.00001f,"ADS position halfway blend");
  auto oldPosition=profile;oldPosition.separateAdsPosition=false;
  expect(scene::length(weapon::gunPositionAt(oldPosition,1)-profile.gunPosition)<.00001f,"disabled ADS position preserves old behavior");
  profile.stats.fireTime = .08f;
  profile.stats.fullAuto = true;
  profile.stats.hideWeaponOnAds = true;
  expect(weapon::canInterruptRechamber(profile.stats,0),"legacy rechamber interruption default changed");
  profile.stats.rechamberFireUnlock=.3f;
  expect(!weapon::canInterruptRechamber(profile.stats,.299f)&&weapon::canInterruptRechamber(profile.stats,.3f),"rechamber unlock boundary");
  profile.stats.canFireWhileRechambering=false;
  expect(!weapon::canInterruptRechamber(profile.stats,100),"disabled rechamber firing allowed");
  weapon::Stats bolt;bolt.rechamberStartDelay=-.25f;expect(std::abs(weapon::rechamberStartSeconds(bolt,1.f)-.75f)<.0001f,"negative bolt delay overlaps fire");bolt.rechamberStartDelay=-2;expect(weapon::rechamberStartSeconds(bolt,1.f)==0,"overlap clamped to shot start");bolt.rechamberStartDelay=.2f;expect(std::abs(weapon::rechamberStartSeconds(bolt,1.f)-1.2f)<.0001f,"positive bolt delay follows fire end");bolt.rechamberDelayFromFireEnd=false;expect(std::abs(weapon::rechamberStartSeconds(bolt,1.f)-.2f)<.0001f,"legacy absolute delay changed");bolt.rechamberStartDelay=-1;expect(weapon::rechamberStartSeconds(bolt,1.f)==1.f,"legacy automatic bolt delay changed");
  profile.stats.rechamberStartDelay=.12f;profile.stats.sprintPlaybackScale=.85f;
  profile.stats.recoil.duration=.6f;profile.stats.recoil.points[2].y=-.1f;profile.stats.recoil.intensity=1.75f;
  profile.scopeOverlayImage="C:/scope images/test square.png";
  profile.animations["reload"] = "viewmodel_mp7_reload.cast";
  expect(!weapon::animationFor(profile,"mantle"),"legacy/generated profile should not force a mantle clip");
  profile.animations["mantle"] = "viewmodel_mp7_vault.cast";
  profile.animations["pullout.from_sidearm"] =
      "viewmodel_an94_pullout_quick.cast";
  profile.animationVariants["inspect"] = {
      "lookat01_karambit.cast", "lookat02_karambit.cast"};
  profile.animationVariants["melee"] = {
      "light_hit1_karambit.cast", "light_hit2_karambit.cast"};
  profile.meleeWeapon = true;
  profile.animationOffsets.push_back(
      {"viewmodel_mp7_reload.cast", {1, 2, 3}, {4, 5, 6}, .9f});
  profile.hiddenTags.push_back("tag_ironsight");
  profile.lockInspectRig = true;
  profile.lockInspectGrip = true;
  profile.materials.useBaseColorLumaMask = true;
  profile.materials.camoAlphaLow = .8f;
  profile.materials.camoAlphaHigh = .2f;
  profile.materials.camoLumaLow = .9f;
  profile.materials.camoLumaHigh = .1f;
  profile.materials.specularMultiplier = 2.25f;
  profile.materials.overrideMetalness=true;profile.materials.metalness=.73f;
  profile.materials.metalnessFromDiffuse=true;profile.materials.metalnessBlack=.8f;profile.materials.metalnessWhite=.2f;profile.materials.metalnessGamma=1.7f;
  profile.materials.cubemapSpecularIntensity = 2.75f;
  profile.materials.specularColorLow = {.1f, .2f, .3f};
  profile.materials.specularColorHigh = {.9f, .8f, .7f};
  const auto path = std::filesystem::temp_directory_path() /
                    "cast_stage_weapon_profile_test.iwweapon";
  std::string error;
  expect(weapon::save(profile, path, error), "profile save failed");
  weapon::Profile loaded;
  expect(weapon::load(path, loaded, error), "profile load failed");
  expect(loaded.rigMounts.size()==1&&loaded.rigMounts[0].attachedModel&&loaded.rigMounts[0].parentTag=="tag_scope"&&loaded.rigMounts[0].position.z==3&&loaded.rigMounts[0].attachmentRotationDegrees.y==20&&loaded.rigMounts[0].attachmentScale.y==2,"live attachment transform did not round trip");
  expect(!loaded.stats.canFireWhileRechambering&&std::abs(loaded.stats.rechamberFireUnlock-.3f)<.0001f,"rechamber firing settings did not round-trip");
  expect(loaded.stats.rechamberDelayFromFireEnd&&std::abs(loaded.stats.rechamberStartDelay-.12f)<.0001f,"new signed bolt timing did not round-trip");
  expect(loaded.name == profile.name, "name did not round trip");
  for(float oldDelay:{-1.f,.2f}){
    const auto oldPath=path.parent_path()/"cadence_legacy_bolt_timing.iwweapon";
    {std::ofstream old(oldPath);old<<"IWWEAPON 1\narchetype bolt_sniper\nstat rechamber_start_delay "<<oldDelay<<'\n';}
    weapon::Profile legacy;expect(weapon::load(oldPath,legacy,error),"legacy timing load");
    expect(!legacy.stats.rechamberDelayFromFireEnd&&std::abs(weapon::rechamberStartSeconds(legacy.stats,1.f)-(oldDelay<0?1.f:oldDelay))<.0001f,"legacy bolt timing changed");
    expect(weapon::save(legacy,oldPath,error)&&weapon::load(oldPath,legacy,error)&&!legacy.stats.rechamberDelayFromFireEnd,"legacy timing mode survives resave");
    std::filesystem::remove(oldPath);
  }
  expect(loaded.stats.hideWeaponOnAds,"ADS visibility did not round trip");
  expect(std::abs(loaded.stats.rechamberStartDelay-.12f)<.0001f&&std::abs(loaded.stats.sprintPlaybackScale-.85f)<.0001f,"new timing fields round trip");
  expect(loaded.scopeOverlayImage==profile.scopeOverlayImage&&loaded.stats.recoil.duration==.6f&&loaded.stats.recoil.points[2].y==-.1f&&loaded.stats.recoil.intensity==1.75f,"scope path and response curve round trip");
  {gameplay::view::RecoilSettings shape;gameplay::view::RecoilState a,b;
   a.kick({2,1,.5f},15,shape);b.kick({2,1,.5f},15,shape);expect(scene::length(a.advance(0,shape))==0,"recoil never instantly snaps on shot");
   scene::Vec3 x,y;for(int i=0;i<12;++i)x=a.advance(1.f/120,shape);for(int i=0;i<6;++i)y=b.advance(1.f/60,shape);
   expect(scene::length(x-y)<.0001f&&x.x>0,"recoil response frame-rate independent");expect(scene::length(a.advance(2,shape))==0,"recoil settles exactly to neutral");
   for(int i=0;i<200;++i)b.kick({100,100,100},15,shape);x=b.advance(.048f,shape);expect(x.x<=shape.maxPitch&&x.y<=shape.maxYaw&&x.z<=shape.maxRoll,"recoil accumulation bounded");
   shape.intensity=2;expect(scene::length(b.advance(0,shape)-x*2)<.0001f,"master intensity scales the final bounded response");shape.intensity=0;expect(scene::length(b.advance(0,shape))==0,"zero master intensity disables procedural recoil");
   scene::Vec3 forward{1,0,0},up{0,0,1};gameplay::view::additiveCamera(forward,up,{2000,2000,2000});expect(std::isfinite(forward.x)&&std::abs(scene::length(forward)-1)<.0001f&&std::abs(scene::dot(forward,up))<.0001f,"camera rotation orthonormal without tangent singularity");
   expect(gameplay::view::rechamberDelay(-1,.3f)==.3f&&gameplay::view::rechamberDelay(.1f,.3f)==.1f,"rechamber starts at explicit delay or authored completion");}
  {auto visible=weapon::makeGenerated("Visible sniper",weapon::Archetype::BoltSniper,"vm_test_");visible.stats.hideWeaponOnAds=false;
   const auto visiblePath=path.parent_path()/"cast_stage_visible_ads_test.iwweapon";weapon::Profile restored;
   expect(weapon::save(visible,visiblePath,error)&&weapon::load(visiblePath,restored,error)&&!restored.stats.hideWeaponOnAds,"explicit visible sniper overrides legacy hide default");std::filesystem::remove(visiblePath);}
  expect(weapon::inferArchetype("wpn_t9_sniper_standard_xmas_view_LOD0")==weapon::Archetype::BoltSniper,"Cold War sniper_standard variant is bolt action");
  expect(weapon::isAdsDownClip("vm_sniper_standard_t9_ads_base_down.cast")&&!weapon::isAdsDownClip("vm_sniper_standard_t9_ads_base_up.cast"),"native ADS direction aliases");
  expect(weapon::isAdsUpClip("viewmodel_an94_ads_up.cast")&&weapon::isAdsDownClip("viewmodel_an94_ads_down.cast"),"legacy ADS aliases retained");
  {const auto old=path.parent_path()/"cast_stage_old_ads_test.iwweapon";std::ofstream f(old);f<<"IWWEAPON 1\narchetype bolt_sniper\n";f.close();weapon::Profile legacy;expect(weapon::load(old,legacy,error)&&legacy.stats.hideWeaponOnAds,"old sniper profile retains visibility policy");std::filesystem::remove(old);}
  expect(loaded.baseModel == profile.baseModel && loaded.rigModels.size() == 2,
         "model configuration did not round trip");
  expect(std::abs(loaded.gunPosition.x - 1.25f) < .0001f &&
             std::abs(loaded.gunPosition.y + 2.5f) < .0001f &&
             std::abs(loaded.gunPosition.z - 3.75f) < .0001f,
         "gun position did not round trip");
  expect(loaded.separateAdsPosition&&scene::length(loaded.adsGunPosition-profile.adsGunPosition)<.00001f,"ADS gun position did not round trip");
  expect(std::abs(loaded.stats.fireTime - .08f) < .0001f &&
             loaded.stats.fullAuto,
         "stats did not round trip");
  expect(weapon::animationFor(loaded, "reload") == profile.animations["reload"],
         "animation map did not round trip");
  expect(weapon::animationFor(loaded, "pullout", "from_sidearm") ==
             profile.animations["pullout.from_sidearm"],
         "paired animation did not resolve");
  expect(weapon::animationFor(loaded,"mantle")==profile.animations["mantle"],
         "mantle animation did not round trip and resolve");
  expect(loaded.meleeWeapon &&
             loaded.animationVariants["inspect"].size() == 2 &&
             loaded.animationVariants["melee"].size() == 2,
         "variant animation lists did not round trip");
  expect(weapon::animationFor(loaded, "inspect") ==
             std::optional<std::string>{"lookat01_karambit.cast"},
         "variant animation did not select the deterministic first clip");
  expect(weapon::offsetFor(loaded, "VIEWMODEL_MP7_RELOAD.CAST") != nullptr,
         "case-insensitive offset lookup failed");
  expect(loaded.hiddenTags.size() == 1, "hidden tags did not round trip");
  expect(loaded.lockInspectRig && loaded.lockInspectGrip,
         "lock inspect flags did not round trip");
  std::error_code ec;
  std::filesystem::remove(path, ec);
  expect(loaded.viewModelName == profile.viewModelName &&
             loaded.worldModelName == profile.worldModelName &&
             loaded.handModelName == profile.handModelName,
         "legacy model metadata did not round trip");
  expect(loaded.materials.useBaseColorLumaMask &&
             std::abs(loaded.materials.camoAlphaLow - .8f) < .0001f &&
             std::abs(loaded.materials.camoAlphaHigh - .2f) < .0001f,
         "invertible camo material levels did not round trip");
  expect(std::abs(loaded.materials.specularMultiplier - 2.25f) < .0001f &&
             loaded.materials.overrideMetalness&&std::abs(loaded.materials.metalness-.73f)<.0001f&&
             loaded.materials.metalnessFromDiffuse&&loaded.materials.metalnessBlack==.8f&&loaded.materials.metalnessWhite==.2f&&loaded.materials.metalnessGamma==1.7f&&
             std::abs(loaded.materials.cubemapSpecularIntensity - 2.75f) < .0001f &&
             std::abs(loaded.materials.specularColorLow.y - .2f) < .0001f &&
             std::abs(loaded.materials.specularColorHigh.z - .7f) < .0001f,
         "per-weapon color specular levels did not round trip");
  const auto legacyPath =
      std::filesystem::temp_directory_path() / "iw5_test_weapon_mp";
  {
    std::ofstream legacy(legacyPath, std::ios::binary);
    legacy << "WEAPONFILE\\weaponClass\\rifle\\fireType\\Full "
              "Auto\\fireTime\\0.085\\adsTransInTime\\0.3\\reloadTime\\1."
              "899\\gunModel\\viewmodel_remington_acr_iw5\\worldModel\\weapon_"
              "remington_acr_iw5\\handModel\\viewmodel_base_"
              "viewhands\\idleAnim\\viewmodel_acr_idle\\fireAnim\\viewmodel_"
              "acr_fire\\adsFireAnim\\viewmodel_acr_ads_"
              "fire\\raiseAnim\\viewmodel_acr_pullout\\";}
    weapon::Profile imported;expect(weapon::importLegacyWeaponFile(imported,legacyPath,error),"classic weaponfile import failed");expect(imported.archetype==weapon::Archetype::Rifle&&imported.stats.fullAuto,"classic weapon behavior did not import");expect(std::abs(imported.stats.fireTime-.085f)<.0001f&&std::abs(imported.stats.reloadTime-1.899f)<.0001f,"classic weapon timing did not import");expect(imported.animationPrefix=="viewmodel_acr_","animation prefix was not derived from weaponfile");expect(weapon::animationFor(imported,"ads_fire")==std::optional<std::string>{"viewmodel_acr_ads_fire.cast"},"explicit animation did not import");expect(imported.viewModelName=="viewmodel_remington_acr_iw5"&&imported.handModelName=="viewmodel_base_viewhands","classic weapon model references did not import");std::filesystem::remove(legacyPath,ec);
    const auto t5SniperPath=std::filesystem::temp_directory_path()/"t5_l96a1_test_mp";{std::ofstream legacy(t5SniperPath,std::ios::binary);legacy<<"WEAPONFILE\\weaponClass\\rifle\\boltAction\\1\\gunModel\\t5_weapon_l96a1_viewmodel\\idleAnim\\viewmodel_l96a1_idle_loop\\";}weapon::Profile t5Sniper;expect(weapon::importLegacyWeaponFile(t5Sniper,t5SniperPath,error),"T5 sniper weaponfile import failed");expect(t5Sniper.archetype==weapon::Archetype::BoltSniper,"T5 rifle-labelled L96 was not promoted to sniper");std::filesystem::remove(t5SniperPath,ec);
    expect(weapon::inferArchetype("t6_wpn_sniper_ballista_view")==weapon::Archetype::BoltSniper,"ballista class inference failed");expect(weapon::inferArchetype("viewmodel_m40a3")==weapon::Archetype::BoltIndividualSniper,"M40A3 was not classified as bolt + individual reload");expect(weapon::defaultsFor(weapon::Archetype::BoltIndividualSniper).individualReload,"M40A3 archetype does not default to individual reload");expect(weapon::inferArchetype("t5_weapon_l96a1_viewmodel")==weapon::Archetype::BoltSniper,"T5 L96 class inference failed");expect(weapon::inferArchetype("t5_weapon_psg1_viewmodel")==weapon::Archetype::SemiSniper,"T5 PSG1 class inference failed");expect(weapon::inferArchetype("t5_weapon_dragunov_viewmodel")==weapon::Archetype::SemiSniper,"T5 Dragunov class inference failed");expect(weapon::inferArchetype("t5_weapon_wa2000_viewmodel")==weapon::Archetype::SemiSniper,"T5 WA2000 class inference failed");expect(weapon::inferArchetype("viewmodel_as50_iw5_LOD0")==weapon::Archetype::SemiSniper,"IW5 AS50 class inference failed");expect(weapon::inferArchetype("awp_model")==weapon::Archetype::BoltSniper,"CS2 AWP class inference failed");expect(weapon::inferArchetype("scar20_model")==weapon::Archetype::SemiSniper,"CS2 SCAR-20 class inference failed");expect(weapon::inferArchetype("viewmodel_desert_eagle_LOD0")==weapon::Archetype::Pistol,"COD4 Desert Eagle class inference failed");expect(weapon::inferArchetype("vm_bal27_base_operator1_LOD0")==weapon::Archetype::Rifle,"AW BAL-27 class inference failed");expect(weapon::inferArchetype("vm_asm1_base_LOD0")==weapon::Archetype::Smg,"AW ASM1 class inference failed");expect(weapon::inferArchetype("vm_mors_base_standard_LOD0")==weapon::Archetype::BoltSniper,"AW MORS class inference failed");expect(weapon::defaultsFor(weapon::Archetype::Smg).fullAuto,"SMG defaults should be automatic");if(!failures)std::cout<<"All weapon profile tests passed.\n";return failures?1:0;}
