#include "app/CodmAnimationPolicy.h"
#include <cstdlib>
#include <cmath>
#define CHECK(v) do{if(!(v))std::abort();}while(false)
int main(){
 {
  scene::CastScene sample;scene::Bone controller;controller.name="Bone_RightHand";controller.parent=-1;
  sample.skeleton.bones.push_back(controller);sample.skeleton.boneByName[controller.name]=0;
  scene::Animation shot;shot.durationFrames=2;scene::Track track;track.boneIndex=0;track.property=scene::TrackProperty::TranslationZ;track.frames={0,1,2};track.scalarValues={0,5,0};shot.tracks.push_back(track);sample.animations.push_back(shot);
  auto base=sample.samplePose(0,0);base[0].v[12]=17;
  auto peak=cadence::codm_actions::alignedHipBolt(sample,base,0,1,1);
  CHECK(std::abs(peak[0].v[14]-5)<.0001f);CHECK(std::abs(peak[0].v[12]-17)<.0001f);
  CHECK(cadence::codm_actions::alignedHipBolt(sample,base,0,1,0)[0].v==base[0].v);
 }
 for(const auto& [name,slot]:std::initializer_list<std::pair<const char*,const char*>>{
  {"viewmodel_ar_bal27_ads_down.cast","ads_down"},{"viewmodel_ar_bal27_un_aiming_on.cast","ads_down"},
  {"viewmodel_ar_bal27_ads_idle.cast","ads_idle"},{"viewmodel_ar_bal27_reload_empty.cast","reload_empty"},
  {"viewmodel_ar_bal27_reload_quick_empty.cast","reload_quick_empty"},{"viewmodel_special_m1887_aiming_bolt.cast","ads_rechamber"},
  {"viewmodel_special_m1887_fire_bolt.cast","rechamber"},{"viewmodel_ar_bal27_jump_end.cast","jump_land"},
  {"viewmodel_ar_bal27_jump_start.cast","jump_takeoff"},{"viewmodel_ar_bal27_aimingjump_end.cast","ads_jump_land"},
  {"viewmodel_ar_bal27_sprint_to_walk.cast","sprint_out"},{"viewmodel_ar_bal27_walk_to_sprint.cast","sprint_in"},
  {"viewmodel_ar_bal27_ads_down_abcdef0123456789.cast","ads_down"}})CHECK(cadence::codm_actions::slotForName(name)==slot);
 for(const auto* name:{"viewmodel_ar_bal27_reload_camera.cast","viewmodel_ar_bal27_ads_up_camera_abcdef01.cast","viewmodel_ar_bal27_aiming_on_br.cast","viewmodel_ar_bal27_slide_start_mask.cast","viewmodel_ar_bal27_change_clip_long_e.cast","viewmodel_ar_bal27_advquick_melee1_hit.cast"})CHECK(cadence::codm_actions::slotForName(name).empty());
 CHECK(!cadence::codm_actions::useAdsBase(true,false,false));
 CHECK(cadence::codm_actions::useAdsBase(true,true,false));
 CHECK(cadence::codm_actions::useAdsBase(true,false,true));
 CHECK(cadence::codm_actions::useAdsBase(false,false,false));
 CHECK(cadence::codm_actions::adsDownWeight(0)==1&&cadence::codm_actions::adsDownWeight(1)==0);
 CHECK(cadence::codm_actions::adsDownWeight(.5f)==.5f);
 CHECK(cadence::codm_actions::adsDownWeight(.75f)==.15625f);
 CHECK(cadence::codm_actions::adsDownWeight(-1)==1&&cadence::codm_actions::adsDownWeight(2)==0);
 {float previous=1;for(int i=1;i<=1000;++i){const float w=cadence::codm_actions::adsDownWeight(i/1000.f);CHECK(w<=previous);CHECK(previous-w<.00151f);previous=w;}}
 {auto a=scene::Mat4::identity(),b=scene::Mat4::identity();a.v[4]=.2f;b.v[8]=-.15f;b.v[12]=10;for(float t:{0.f,.1f,.5f,.9f,1.f}){auto m=cadence::codm_actions::blendAffine(a,b,t);for(int i=0;i<16;++i){CHECK(std::isfinite(m.v[i]));if(t==0)CHECK(m.v[i]==a.v[i]);if(t==1)CHECK(m.v[i]==b.v[i]);}}}
 {scene::CastScene muzzleScene;muzzleScene.skeleton.bones.emplace_back();muzzleScene.skeleton.bones[0].name="Muzzle_point";auto m=scene::Mat4::identity();m.v[12]=42;auto pos=cadence::codm_actions::muzzle(muzzleScene,{m});CHECK(pos&&pos->x==42);}
 scene::CastScene s;s.codmNativeCentimetres=true;s.codmNativeWeaponStem="viewmodel_shotty_test_Default";s.skeleton.bones.emplace_back();
 const auto prefix=cadence::codm_actions::lower(s.codmNativeWeaponStem)+"_";
 auto add=[&](const char* suffix,unsigned duration){scene::Animation a;a.sourceName=prefix+suffix+".cast";a.domain=scene::AnimationDomain::ViewModel;a.durationFrames=duration;scene::Track t;t.frames={0,duration};t.scalarValues={0,10};a.tracks.push_back(t);s.animations.push_back(a);};
 add("ads_up",9);add("fire",3);add("fire_single",17);add("fire_bolt",40);add("aiming_bolt",42);add("reload_empty",80);add("reload",65);
 CHECK(cadence::codm_actions::hipBolt(s,3));CHECK(!cadence::codm_actions::hipBolt(s,4));CHECK(!cadence::codm_actions::hipBolt(s,999));s.codmNativeCentimetres=false;CHECK(!cadence::codm_actions::hipBolt(s,3));s.codmNativeCentimetres=true;
 cadence::codm_actions::prepare(s);auto count=s.animations.size();cadence::codm_actions::prepare(s);CHECK(count==s.animations.size());
 for(float f=0;f<=9;f+=.125f){auto up=s.samplePose(0,9-f),down=s.samplePose(count-1,f);CHECK(std::abs(up[0].v[12]-down[0].v[12])<1e-5f);}
 weapon::Profile p;p.stats.fullAuto=false;cadence::codm_actions::populate(s,p,s.codmNativeWeaponStem);cadence::codm_actions::defaultTimings(s,p);
 CHECK(p.animations.at("fire")==prefix+"fire_single.cast");CHECK(p.animations.at("reload_empty")==prefix+"reload_empty.cast");
 CHECK(p.animations.at("rechamber")==prefix+"fire_bolt.cast");CHECK(p.animations.at("ads_rechamber")==prefix+"aiming_bolt.cast");CHECK(p.stats.boltAction);
 CHECK(std::abs(p.stats.rechamberTime-40.f/30)<1e-6f);
 weapon::Profile automatic;automatic.stats.fullAuto=true;cadence::codm_actions::populate(s,automatic,s.codmNativeWeaponStem);CHECK(automatic.animations.at("fire")==prefix+"fire.cast");
 p.animations["fire"]=prefix+"fire.cast";cadence::codm_actions::populate(s,p,s.codmNativeWeaponStem);CHECK(p.animations.at("fire")==prefix+"fire.cast");
 // Absolute source paths must not discard an explicit mapping in favour of
 // the generated default (the native library may be loaded through a cache).
 for(auto& a:s.animations)if(a.sourceName==prefix+"fire.cast")a.sourceName="clips/"+a.sourceName;
 cadence::codm_actions::populate(s,p,s.codmNativeWeaponStem);CHECK(p.animations.at("fire")==prefix+"fire.cast");
 weapon::Profile unrelated;cadence::codm_actions::populate(s,unrelated,"other");CHECK(unrelated.animations.empty());
 add("ads_down",8);weapon::Profile authored;cadence::codm_actions::populate(s,authored,s.codmNativeWeaponStem);CHECK(authored.animations.at("ads_down")==prefix+"ads_down.cast");
 // Current exporter vocabulary, including disambiguated filenames.
 s.animations.clear();
 for(auto suffix:{"weapon_idle","weapon_fire","weapon_change_clip","weapon_change_clip_e","weapon_equip","weapon_put_down","weapon_run","ads_up","un_aiming_on"})add(suffix,20);
 cadence::codm_actions::prepare(s);CHECK(s.animations.size()==9);
 weapon::Profile aliases;cadence::codm_actions::populate(s,aliases,s.codmNativeWeaponStem);
 for(auto slot:{"idle","fire","ads_fire","reload","reload_empty","pullout","putaway","sprint_loop","ads_up","ads_down"})CHECK(aliases.animations.contains(slot));
 CHECK(aliases.animations.at("ads_down")==prefix+"un_aiming_on.cast");
 add("jump_start",12);add("jump_end",14);cadence::codm_actions::prepare(s);cadence::codm_actions::populate(s,aliases,s.codmNativeWeaponStem);
 CHECK(aliases.animations.at("jump_takeoff")==prefix+"jump_start.cast");CHECK(aliases.animations.at("jump_land")==prefix+"jump_end.cast");
 CHECK(s.animations.back().motion==scene::MotionRole::Land);
 CHECK(s.animations[8].action==scene::ActionRole::Aim);
 // New canonical exports replace stale aliases, but never a valid override.
 s.animations.clear();add("ads_down",11);add("reload",40);add("reload_quick",20);add("reload_empty",55);add("change_clip_long_e",80);add("reload_camera",40);
 weapon::Profile renamed;renamed.animations["ads_down"]=prefix+"un_aiming_on.cast";
 cadence::codm_actions::populate(s,renamed,s.codmNativeWeaponStem);
 CHECK(renamed.animations.at("ads_down")==prefix+"ads_down.cast");CHECK(renamed.animations.at("reload")==prefix+"reload.cast");CHECK(renamed.animations.at("reload_empty")==prefix+"reload_empty.cast");
 renamed.animations["reload"]=prefix+"reload_quick.cast";cadence::codm_actions::populate(s,renamed,s.codmNativeWeaponStem);CHECK(renamed.animations.at("reload")==prefix+"reload_quick.cast");
 s.animations.clear();add("reload_76c672badfffaaba",20);
 CHECK(cadence::codm_actions::find(s,prefix,"reload")!=nullptr);
 add("reload_aaaaaaaa",20);CHECK(cadence::codm_actions::find(s,prefix,"reload")==nullptr);
 add("reload",20);CHECK(cadence::codm_actions::find(s,prefix,"reload")->sourceName==prefix+"reload.cast");
 add("fire_bolt_76c672badfffaaba",20);CHECK(cadence::codm_actions::hipBolt(s,s.animations.size()-1));
 const std::vector<std::string> models{"viewmodel_ar_ak117","viewmodel_ar_ak117_dragon"};
 CHECK(cadence::codm_actions::belongsToWeapon("viewmodel_ar_ak117_reload","viewmodel_ar_ak117",models));
 CHECK(!cadence::codm_actions::belongsToWeapon("viewmodel_ar_ak117_dragon_reload","viewmodel_ar_ak117",models));
 CHECK(cadence::codm_actions::belongsToWeapon("viewmodel_ar_ak117_dragon_reload","viewmodel_ar_ak117_dragon",models));
}
