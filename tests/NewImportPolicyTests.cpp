#include "assets/ImportedGamePolicy.h"
#include "scene/ImportedDirtyBomb.h"
#include "scene/ImportedMuzzle.h"
#include "scene/BlackSquadActions.h"
#include <iostream>
int main(){using namespace assets;using namespace assets::imported;int failed{};const auto check=[&](bool ok){if(!ok)++failed;};
    {
        scene::Animation candidate;candidate.name="unchanged label";candidate.sourceName="unchanged_hash.cast";
        candidate.durationFrames=177;candidate.framerate=28.85869f;
        scene::Track t;t.frames={0,177};t.scalarValues={1,2};candidate.tracks={t};
        const std::string standing="Animation/GenericMale/GenericMale_2H_Stand_SF.upk:Stand_Idle_01@179";
        const std::string crouching="Animation/GenericMale/GenericMale_2H_Crouch_SF.upk:Crouch_Idle_01@42";
        const auto classify=[&](std::string_view source,std::string_view mode="source_absolute"){
            auto clip=candidate;scene::classifyAnimationName("pb_rifle_stand_idle",clip);
            const bool accepted=scene::imported::classifyDirtyBombBodyIdle(clip,source,mode);
            check(clip.name==candidate.name&&clip.sourceName==candidate.sourceName&&clip.durationFrames==177&&clip.framerate==candidate.framerate&&clip.tracks[0].scalarValues==t.scalarValues);
            if(!accepted)check(clip.domain==scene::AnimationDomain::Other&&clip.motion==scene::MotionRole::Unknown&&clip.contextual);
            return clip;
        };
        auto stand=classify(standing),crouch=classify(crouching);
        check(stand.domain==scene::AnimationDomain::PlayerBody&&stand.motion==scene::MotionRole::Idle&&stand.weapon==scene::WeaponClass::Rifle&&stand.stance==scene::Stance::Stand&&!stand.contextual);
        check(crouch.stance==scene::Stance::Crouch&&crouch.domain==scene::AnimationDomain::PlayerBody);
        auto menu=classify("Animation/GenericFemale/GenericFemale_MenuIdles_SF.upk:Menu_Idle_01@32");
        auto pistol=classify("Animation/GenericMale/GenericMale_1H_Stand_SF.upk:Stand_Idle_01@198");
        scene::CastScene rig;rig.animations={menu,pistol,crouch,stand};scene::AnimationQuery query;
        query.domain=scene::AnimationDomain::PlayerBody;query.motion=scene::MotionRole::Idle;query.weapon=scene::WeaponClass::Rifle;query.stance=scene::Stance::Stand;
        check(scene::findBestAnimation(rig,query)==3);query.stance=scene::Stance::Crouch;check(scene::findBestAnimation(rig,query)==2);
        for(const auto family:{"GenericMale","GenericHeavy","GenericFemale"}){
            // Female is convention-based coverage, not a claim of native visual validation.
            check(classify(std::string("Animation/")+family+"/"+family+"_2H_Crouch_SF.upk:Crouch_Idle_02@44").domain==scene::AnimationDomain::PlayerBody);
        }
        for(const auto label:{"Stand_Idle_01_Stunned","Stand_Idle_01_Part_01","ADD_Stand_Idle_01","Stand_Idle_01_Start","Stand_Idle_01_Loop","Stand_Idle_01_End","Crouch_Idle_01","Stand_Idle_x1","Stand_Idle_1","Stand_Idle_001","Stand_Fire_01"})
            check(classify(std::string("Animation/GenericMale/GenericMale_2H_Stand_SF.upk:")+label+"@1").domain==scene::AnimationDomain::Other);
        for(const auto family:{"Knife","Defibs_01","Pliers_01","Data_Core","Unknown"})
            check(classify(std::string("Animation/GenericMale/GenericMale_")+family+"_Stand_SF.upk:Stand_Idle_01@1").domain==scene::AnimationDomain::Other);
        for(const auto source:{"", "Animation/GenericMale/GenericMale_2H_Stand_SF.upk:Stand_Idle_01", "Animation/GenericMale/GenericMale_2H_Stand_SF.upk:Stand_Idle_01@bad"})
            check(classify(source).domain==scene::AnimationDomain::Other);
        for(const auto mode:{"", "baked_absolute", "additive"})check(classify(standing,mode).domain==scene::AnimationDomain::Other);
        for(const auto mode:{scene::TrackMode::Additive,scene::TrackMode::Relative}){auto clip=candidate;clip.tracks[0].mode=mode;check(!scene::imported::classifyDirtyBombBodyIdle(clip,standing,"source_absolute"));}
        auto empty=candidate;empty.tracks.clear();check(!scene::imported::classifyDirtyBombBodyIdle(empty,standing,"source_absolute"));
        auto malformed=candidate;malformed.tracks[0].scalarValues.pop_back();check(!scene::imported::classifyDirtyBombBodyIdle(malformed,standing,"source_absolute"));
    }
    {
        scene::Animation recoil;recoil.sourceName="SK_Char_CO01_Base_Stand_Fire_01_b27b304da99ea3e3.cast";
        recoil.name="native recoil";recoil.durationFrames=14;recoil.framerate=10;
        scene::Track track;track.frames={0,2,14};track.scalarValues={0,1,0};recoil.tracks={track};
        const std::string source="Animation/GenericMale/GenericMale_2H_Stand_SF.upk:Stand_Fire_01@245";
        check(scene::imported::classifyDirtyBombBodyFire(recoil,source,"source_absolute"));
        check(recoil.domain==scene::AnimationDomain::PlayerTorso&&recoil.action==scene::ActionRole::Fire&&recoil.weapon==scene::WeaponClass::Rifle&&recoil.stance==scene::Stance::Stand&&!recoil.ads&&!recoil.looping&&!recoil.contextual);
        check(recoil.sourceName=="SK_Char_CO01_Base_Stand_Fire_01_b27b304da99ea3e3.cast"&&recoil.durationFrames==14&&recoil.framerate==10&&recoil.tracks[0].scalarValues==track.scalarValues);
        scene::CastScene native;native.animations={recoil};scene::AnimationQuery query;
        query.domain=scene::AnimationDomain::PlayerTorso;query.action=scene::ActionRole::Fire;query.weapon=scene::WeaponClass::Rifle;query.stance=scene::Stance::Stand;
        check(scene::findBestAnimation(native,query)==0);
        query.ads=true;check(!scene::findBestAnimation(native,query));
        for(const auto mode:{"baked_absolute","additive",""})check(!scene::imported::classifyDirtyBombBodyFire(recoil,source,mode));
        for(const auto label:{"ADD_Stand_Fire_01","Stand_Fire_01_Part_01","Stand_Fire_01_Start","Stand_Fire_01_Loop","Stand_Fire_01_End","Crouch_Fire_01","Stand_Fire_03"})
            check(!scene::imported::classifyDirtyBombBodyFire(recoil,std::string("Animation/GenericMale/GenericMale_2H_Stand_SF.upk:")+label+"@1","source_absolute"));
        for(const auto family:{"1H","Knife","MPistol_04","Pliers_01","Defibs_01","Data_Core"})
            check(!scene::imported::classifyDirtyBombBodyFire(recoil,std::string("Animation/GenericMale/GenericMale_")+family+"_Stand_SF.upk:Stand_Fire_01@1","source_absolute"));
        check(!scene::imported::classifyDirtyBombBodyFire(recoil,source+"bad","source_absolute"));
        check(scene::imported::classifyDirtyBombBodyFire(recoil,"Animation/GenericHeavy/GenericHeavy_2H_Stand_SF.upk:Stand_Fire_02@197","source_absolute"));
        {
            scene::CastScene body;scene::Bone pelvis,spine,leg;pelvis.name="SkelBone_Pelvis";spine.name="SkelBone_Spine";spine.parent=0;leg.name="SkelBone_L_Thigh";leg.parent=0;
            body.skeleton.bones={pelvis,spine,leg};body.skeleton.boneByName={{pelvis.name,0},{spine.name,1},{leg.name,2}};
            auto fire=recoil;fire.tracks.clear();for(std::size_t b=0;b<3;++b){auto channel=track;channel.boneIndex=b;channel.scalarValues={0,10,0};fire.tracks.push_back(channel);}
            check(scene::imported::classifyDirtyBombBodyFire(fire,source,"source_absolute",body.skeleton));
            check(!fire.tracks[0].ownsLayer&&fire.tracks[1].ownsLayer&&!fire.tracks[2].ownsLayer);
            scene::Animation base;body.animations={base,fire};
            const auto posed=body.sampleLayeredPose(0,0,1,2,1,scene::LayerMode::Override,true);
            check(std::abs(posed[0].v[12])<.0001f&&std::abs(posed[2].v[12])<.0001f&&std::abs(posed[1].v[12]-10)<.0001f);
        }
        recoil.tracks[0].mode=scene::TrackMode::Additive;check(!scene::imported::classifyDirtyBombBodyFire(recoil,source,"source_absolute"));
        recoil.tracks.clear();check(!scene::imported::classifyDirtyBombBodyFire(recoil,source,"source_absolute"));
    }
    {
        scene::CastScene body;scene::Bone pelvis,spine,leg;
        pelvis.name="SkelBone_Pelvis";spine.name="SkelBone_Spine";spine.parent=0;leg.name="SkelBone_L_Thigh";leg.parent=0;
        body.skeleton.bones={pelvis,spine,leg};body.skeleton.boneByName={{pelvis.name,0},{spine.name,1},{leg.name,2}};
        scene::Animation candidate;candidate.sourceName="reload.cast";candidate.durationFrames=2;candidate.framerate=30;
        scene::classifyAnimationName("pb_rifle_stand_reload",candidate);
        for(std::size_t b=0;b<3;++b){scene::Track t;t.boneIndex=b;t.frames={0,2};t.scalarValues={0,10};candidate.tracks.push_back(t);}
        const std::string source="Animation/GenericMale/GenericMale_2H_Stand_SF.upk:Stand_Reload@248";
        for(const auto family:{"GenericMale","GenericHeavy","GenericFemale"})for(const auto hands:{"2H","1H"}){
            auto clip=candidate;
            check(scene::imported::classifyDirtyBombBodyReload(clip,std::string("Animation/")+family+"/"+family+"_"+hands+"_Stand_SF.upk:Stand_Reload@248","source_absolute",body.skeleton));
            check(clip.domain==scene::AnimationDomain::PlayerTorso&&clip.action==scene::ActionRole::Reload&&!clip.contextual&&!clip.looping);
            check(clip.weapon==(std::string_view(hands)=="2H"?scene::WeaponClass::Rifle:scene::WeaponClass::Pistol));
            check(!clip.tracks[0].ownsLayer&&clip.tracks[1].ownsLayer&&!clip.tracks[2].ownsLayer);
            check(clip.sourceName==candidate.sourceName&&clip.tracks[1].scalarValues==candidate.tracks[1].scalarValues);
            scene::Animation base;body.animations={base,clip};
            const auto pose=body.sampleLayeredPose(0,0,1,2,1,scene::LayerMode::Override,true);
            check(std::abs(pose[0].v[12])<.0001f&&std::abs(pose[2].v[12])<.0001f&&std::abs(pose[1].v[12]-10)<.0001f);
        }
        for(const auto mode:{"baked_absolute","additive",""}){auto c=candidate;check(!scene::imported::classifyDirtyBombBodyReload(c,source,mode,body.skeleton));}
        for(const auto label:{"ADD_Stand_Reload","Stand_Reload_Part_01","Stand_Reload_Start","Stand_Reload_Loop","Stand_Reload_End"}){
            auto c=candidate;check(!scene::imported::classifyDirtyBombBodyReload(c,std::string("Animation/GenericMale/GenericMale_2H_Stand_SF.upk:")+label+"@1","source_absolute",body.skeleton));
        }
        for(int fault=0;fault<4;++fault){auto c=candidate;
            if(fault==0)c.tracks.clear();if(fault==1)c.tracks[0].mode=scene::TrackMode::Additive;
            if(fault==2)c.tracks[0].scalarValues.pop_back();if(fault==3)c.tracks[0].frames.clear();
            check(!scene::imported::classifyDirtyBombBodyReload(c,source,"source_absolute",body.skeleton));
        }
        auto c=candidate;check(!scene::imported::classifyDirtyBombBodyReload(c,source+"bad","source_absolute",body.skeleton));
        c=candidate;check(!scene::imported::classifyDirtyBombBodyReload(c,source,"source_absolute",{}));
    }
    check(dewBodySemantic("masterchief_21000_crouch_rifle_idle_620.cast")=="pb_rifle_crouch_idle");
    check(dewBodySemantic("ignored.cast","crouch:rifle:idle")=="pb_rifle_crouch_idle");
    for(const auto direction:{"front","back","left","right"}){
        const auto mapped=std::string_view(direction)=="front"?"forward":std::string_view(direction)=="back"?"backward":direction;
        check(dewBodySemantic(std::string("masterchief_21000_crouch_rifle_walk_")+direction+"_634.cast")==std::string("pb_rifle_crouch_walk_")+mapped);
        check(dewBodySemantic("",std::string("crouch:rifle:move_")+direction)==std::string("pb_rifle_crouch_run_")+mapped);
    }
    check(dewBodySemantic("masterchief_21000_crouch_rifle_ar_reload_1_619.cast")=="pt_rifle_crouch_reload");
    check(dewBodySemantic("","crouch:rifle:ar:reload_1")=="pt_rifle_crouch_reload");
    check(dewBodySemantic("","crouch:pistol:idle:var0")=="pb_pistol_crouch_idle");
    check(dewBodySemantic("","crouch:dual:move_left")=="pb_dualwield_crouch_run_left");
    check(dewBodySemantic("","combat:rifle:idle")=="pb_rifle_stand_idle");
    check(dewBodySemantic("masterchief_21000_combat_rifle_walk_front_42.cast")=="pb_rifle_stand_walk_forward");
    for(const auto label:{"crouch:rifle:idle:2:combat:rifle:idle","act:guard:idle:2:crouch:rifle:idle","crouch:rifle:aim_still_up","crouch:rifle:grip","crouch:any:airborne_dead"})check(dewBodySemantic("",label).empty());
    for(const auto file:{"masterchief_21000_combat_rifle_idle_2_crouch_rifle_idle_42.cast","masterchief_21000_act_guard_crouch_rifle_idle_42.cast","masterchief_21000_crouch_rifle_idle_bad.cast"})check(dewBodySemantic(file).empty());
    check(dewBodyIdentity("masterchief_21000_combat_rifle_idle_353.cast")=="masterchief_21000");
    check(dewBodyIdentity("masterchief_21000_crouch_rifle_walk_front_634.cast")=="masterchief_21000");
    check(dewBodyIdentity("masterchief_21000_crouch_rifle_ar_reload_1_619.cast")=="masterchief_21000");
    check(dewBodyIdentity("MP_ELITE_26901_CROUCH_PISTOL_IDLE_42.CAST")=="mp_elite_26901");
    check(dewBodyIdentity("odst_recon_cheap_26675_combat_rifle_idle_var0_365.cast")=="odst_recon_cheap_26675");
    for(const auto file:{"masterchief_21000_combat_rifle_idle_2_crouch_rifle_idle_42.cast","masterchief_21000_act_guard_crouch_rifle_idle_42.cast","masterchief_21000_crouch_rifle_idle_bad.cast","masterchief_21000_crouch_rifle_grip_42.cast","masterchief_21000_vehicle_rifle_idle_42.cast","masterchief_crouch_rifle_idle_42.cast","crouch:rifle:idle"})check(dewBodyIdentity(file).empty());
    check(additionalAction("gitsfa","GIS_base_1st__set16__base_M200_1st_boltaction__2199ca9957cd2a74.cast")=="rechamber");
    check(matchesAnimation("gitsfa","weapon_SR_M200_1st_M200_Body__123","GIS_base_1st__set16__base_M200_1st_boltaction__123.cast"));
    check(additionalAction("blacksquad","SK_AK47_1P__mesh_123__Stare_V08__123.cast")=="inspect");
    check(additionalAction("blacksquad","SK_AK47_1P__mesh_123__Equip__123.cast")=="pullout");
    check(additionalAction("blacksquad","SK_Nimravus_1P__mesh_123__Run__123.cast")=="sprint_loop");
    check(additionalAction("gitsfa","GIS_base_1st__set16__base_M200_1st_sprint_jump_up1__123.cast")=="jump_takeoff");
    for(const auto weapon:{"M200","M4A1","44MG"}){
        check(additionalAction("gitsfa",std::string("GIS_base_1st__set16__base_")+weapon+"_1st_stand_run_forward__123.cast")=="sprint_loop");
        check(additionalAction("gitsfa",std::string("GIS_base_1st__set16__base_")+weapon+"_1st_sprint_jump_up1__123.cast")=="jump_takeoff");
    }
    check(role("gitsfa","weapon_AR_AK47_1st_AK47_Body__123__rig_42")==Role::ViewWeapon);
    check(role("gitsfa","gitsfa__ar_ak47__1p__123__att__magazine__ak47_ma")==Role::ViewAttachment);
    check(role("gitsfa","viewhands_ActorGhost_CommonHand_1stHand__123")==Role::ViewHands);
    check(role("dirtybomb","playerpart_SK_Char_CO01_Base_41c32d31eabc7a3c")==Role::PlayerModel);
    check(role("dirtybomb","worldmodel_SK_Pistol_05_World_564aff38241c4358")==Role::WorldWeapon);
    check(role("blacksquad","SK_VH_PM_Female10__123")==Role::ViewHands);
    check(role("blacksquad","SK_PM02__123")==Role::PlayerModel);
    check(additionalWeaponKey("dirtybomb","viewmodel_SK_Pistol_05_b244f32ce2e82c2b")==additionalWeaponKey("dirtybomb","worldmodel_SK_Pistol_05_World_564aff38241c4358"));
    check(additionalCategory("blacksquad","SK_Karambit_1P__123")=="knives");
    check(matchesAnimation("gitsfa","weapon_AR_M4A1_1st_M4A1_Body__123","GIS_base_1st__set0__base_M4A1_1st_reload__123.cast"));
    check(!matchesAnimation("gitsfa","weapon_AR_AK47_1st_AK47_Body__123","GIS_base_1st__set0__base_M4A1_1st_reload__123.cast"));
    check(matchesAnimation("blacksquad","SK_AK47_1P__123","SK_AK47_1P__mesh_123__Reload__123.cast"));
    check(matchesAnimation("blacksquad","SK_AK47_1P__123","SK_AK47_1P__mesh_123__ADD_Fire__123.cast")); // CAST additive tracks retain their authored mode.
    check(matchesAnimation("dirtybomb","viewmodel_SK_Pistol_05_b244f32ce2e82c2b","SK_Pistol_05_SK_ViewArms_CovertOps_01_Idle_b57e995f29f72377.cast"));
    check(!additionalBodySemantic("gitsfa","GIS_base_3rd__set0__base_M4A1_3rd_stand_run_forward__123.cast").empty());
    check(matchesAnimation("dirtybomb","viewmodel_SK_Wpn_SniperRifle_03_34475981618572b9","SK_Wpn_SniperRifle_03_Fire_01_Part_01_843b307835509004.cast"));
    check(additionalAction("dirtybomb","SK_Wpn_AssaultRifle_04_SK_ViewArms_CovertOps_01_Zoom_Idle_123456789abcdef0.cast")=="ads_idle");
    scene::CastScene rig;rig.viewHandsDriverGame="dirtybomb";scene::Bone gun,hand;gun.name="weapon";hand.name="SkelBone_R_Hand";rig.skeleton.bones={gun,hand};
    scene::Animation first,second,shared;first.sourceName="SK_Wpn_Test_Fire_01_Part_01_123456789abcdef0.cast";first.name="fire";first.durationFrames=10;
    scene::Track t;t.boneIndex=0;t.frames={0,10};t.scalarValues={0,1};first.tracks={t};
    second=first;second.sourceName="SK_Wpn_Test_Fire_01_Part_02_abcdef0123456789.cast";second.durationFrames=5;second.tracks[0].frames={0,5};second.tracks[0].scalarValues={1,0};
    shared.name="shared: fire";shared.sourceName="SK_Wpn_Other_Fire_123456789abcdef0.cast";shared.durationFrames=10;t.boneIndex=1;shared.tracks={t};rig.animations={first,second,shared};
    scene::imported::completeDirtyBombActions(rig);check(rig.animations[0].durationFrames==16);check(rig.animations[0].tracks.size()==2);check(rig.animations[0].tracks[0].frames.back()==16);
    scene::imported::completeDirtyBombActions(rig);check(rig.animations[0].durationFrames==16&&rig.animations[0].tracks.size()==2);
    {
        auto dense=second;dense.framerate=90;dense.durationFrames=15;dense.tracks[0].frames={0,1,2,15};dense.tracks[0].scalarValues={0,1,2,3};
        scene::CastScene stages;stages.viewHandsDriverGame="dirtybomb";stages.skeleton=rig.skeleton;stages.animations={first,dense};
        scene::imported::completeDirtyBombActions(stages);
        check(stages.animations[0].framerate==90&&stages.animations[0].durationFrames==48);
        const auto& frames=stages.animations[0].tracks[0].frames;check(frames.size()==6&&frames[2]==33&&frames[3]==34&&frames[4]==35&&frames[5]==48);
    }
    {
        auto constant=first;constant.tracks[0].property=scene::TrackProperty::TranslationX;
        constant.tracks[0].frames={0};constant.tracks[0].scalarValues={0};
        auto next=second;next.tracks[0].property=scene::TrackProperty::TranslationX;
        scene::CastScene stages;stages.viewHandsDriverGame="dirtybomb";stages.skeleton=rig.skeleton;stages.animations={constant,next};
        scene::imported::completeDirtyBombActions(stages);
        check(stages.animations[0].tracks[0].frames==std::vector<std::uint32_t>({0,10,11,16}));
        check(std::abs(stages.sampleLocalPose(0,5)[0].position.x)<.0001f);
        check(std::abs(stages.sampleLocalPose(0,10)[0].position.x)<.0001f);
        check(std::abs(stages.sampleLocalPose(0,11)[0].position.x-1)<.0001f);
    }
    scene::Animation ads=shared;ads.name="shared: ads_fire";rig.animations.push_back(ads);
    scene::imported::completeDirtyBombActions(rig);check(rig.animations.back().tracks.size()==2);check(rig.animations.back().durationFrames==16);
    scene::imported::completeDirtyBombActions(rig);check(rig.animations.back().tracks.size()==2);
    scene::CastScene hip;hip.viewHandsDriverGame="dirtybomb";hip.skeleton=rig.skeleton;auto nativeAds=first;nativeAds.sourceName="SK_Wpn_Test_Zoom_Fire_123456789abcdef0.cast";hip.animations={shared,nativeAds};
    scene::imported::completeDirtyBombActions(hip);check(hip.animations[0].tracks.size()==2);
    scene::imported::completeDirtyBombActions(hip);check(hip.animations[0].tracks.size()==2);
    scene::CastScene shortMechanism;shortMechanism.viewHandsDriverGame="dirtybomb";shortMechanism.skeleton=rig.skeleton;
    auto longHands=shared;longHands.durationFrames=30;auto shortSlide=nativeAds;shortSlide.durationFrames=2;
    shortMechanism.animations={longHands,shortSlide};scene::imported::completeDirtyBombActions(shortMechanism);
    check(shortMechanism.animations[0].durationFrames==30);
    {
        scene::CastScene split;split.viewHandsDriverGame="dirtybomb";split.skeleton=rig.skeleton;
        auto weapon=first;weapon.durationFrames=87;weapon.framerate=30;weapon.tracks[0].frames={0,87};
        auto hands=shared;hands.name="native hands";hands.sourceName="SK_Wpn_Test_SK_ViewArms_CovertOps_01_Fire_01_Part_01_recovered.cast";hands.durationFrames=27;hands.framerate=30;hands.tracks[0].frames={0,27};
        auto wrong=hands;wrong.sourceName="SK_Wpn_Other_SK_ViewArms_CovertOps_01_Fire_01_Part_01_recovered.cast";
        split.animations={weapon,wrong,hands};scene::imported::mergeDirtyBombSplitHands(split);
        check(split.animations[0].tracks.size()==2);
        check(std::abs(split.animations[0].durationFrames/split.animations[0].framerate-.9f)<.0001f);
        check(split.animations[0].tracks[1].frames.back()==87);
        scene::imported::mergeDirtyBombSplitHands(split);check(split.animations[0].tracks.size()==2);
        auto aimed=hands;aimed.sourceName="SK_Wpn_Test_SK_ViewArms_CovertOps_01_Zoom_Fire_01_Part_01_recovered.cast";
        split.animations.push_back(aimed);scene::imported::mergeDirtyBombSplitHands(split);
        check(split.animations.back().tracks.size()==2);
        check(std::abs(split.animations.back().durationFrames/split.animations.back().framerate-.9f)<.0001f);
        scene::imported::mergeDirtyBombSplitHands(split);check(split.animations.back().tracks.size()==2);
        check(scene::imported::dbSplitActionKey(weapon.sourceName)==scene::imported::dbSplitActionKey(hands.sourceName));
        for(const auto* suffix:{"Zoom_Fire_01_Part_01","Fire_02_Part_01","Fire_01_Part_02"})
            check(scene::imported::dbSplitActionKey(weapon.sourceName)!=scene::imported::dbSplitActionKey(std::string("SK_Wpn_Test_SK_ViewArms_CovertOps_01_")+suffix+"_recovered.cast"));
    }
    {
        scene::CastScene bs;bs.importedViewGame="blacksquad";scene::Bone b;b.restLocal.position={0,0,100};bs.skeleton.bones={b};
        scene::Animation idle;idle.sourceName="SK_Test_1P__mesh_123__Idle__123.cast";idle.durationFrames=30;idle.framerate=30;
        scene::Track p;p.property=scene::TrackProperty::TranslationZ;p.frames={0};p.scalarValues={-5};idle.tracks={p};
        scene::Animation run;run.sourceName="SK_Test_1P__mesh_123__ADD_Run__123.cast";run.durationFrames=10;run.framerate=30;p.mode=scene::TrackMode::Additive;p.frames={0,10};p.scalarValues={0,2};run.tracks={p};bs.animations={idle,run};
        scene::imported::completeBlackSquadActions(bs);check(std::abs(bs.sampleLocalPose(1,10)[0].position.z+3)<.0001f);
        const auto tracks=bs.animations[1].tracks.size();scene::imported::completeBlackSquadActions(bs);check(bs.animations[1].tracks.size()==tracks);
        check(bs.skeleton.bones[0].restLocal.position.z==100);
    }
    scene::CastScene units;scene::Bone root;root.restLocal.position={0,0,10};root.restGlobal=scene::translation(root.restLocal.position);root.inverseBind=scene::inverseAffine(root.restGlobal);units.skeleton.bones.push_back(root);
    auto legacyUnits=units;{scene::imported::ScopedImportRevision old(0);scene::imported::normalizeDirtyBomb(legacyUnits);check(legacyUnits.skeleton.bones[0].restGlobal.v[14]==10);}
    scene::imported::normalizeDirtyBomb(units);check(units.skeleton.bones[0].restGlobal.v[14]==20&&scene::imported::activeImportRevision==1);
    scene::CastScene muzzle;muzzle.importedTranslationScale=200;scene::Bone parent,socket;parent.name="Weapon_Offset";socket.name="b_Silencer";socket.parent=0;socket.restGlobal=scene::translation({0,0,10});muzzle.skeleton.bones={parent,socket};
    scene::Mesh mesh;scene::Vertex vertex;vertex.position={0,0,20};vertex.weights={1,0,0,0};mesh.vertices.push_back(vertex);vertex.position={99,0,200};mesh.vertices.push_back(vertex);muzzle.meshes.push_back(mesh);
    scene::imported::prepareMuzzleAnchors(muzzle,"exports/blacksquad/models/SK_AK47_3P__test.cast");check(muzzle.muzzleAnchors.size()==1&&muzzle.muzzleAnchors[0].local.z==20);
    scene::CastScene fx;scene::Bone grip,barrel,marker;grip.name="Grip";barrel.name="gitsfa_socket__barrel__1";barrel.parent=0;barrel.restGlobal=scene::translation({10,0,0});marker.name="gitsfa_socket__muzzle_fx__5";marker.parent=0;marker.restGlobal=scene::translation({38,0,10});fx.skeleton.bones={grip,barrel,marker};
    scene::imported::prepareMuzzleAnchors(fx,"exports/gitsfa/models/weapon_PL_Test_1st_Test.cast");check(fx.muzzleAnchors.size()==1&&fx.muzzleAnchors[0].bone==0&&fx.muzzleAnchors[0].local.x==38&&fx.muzzleAnchors[0].local.z==10);
    scene::CastScene bare;scene::Bone body,arm;body.name="b_Weapon_Root";arm.name="J_Shoulder_RI";arm.parent=0;bare.skeleton.bones={body,arm};scene::Mesh combined;
    for(const auto p:std::vector<scene::Vec3>{{0,0,8},{100,-1,8},{100,1,8}}){scene::Vertex v;v.position=p;v.bones={0,0,0,0};v.weights={1,0,0,0};combined.vertices.push_back(v);}
    scene::Vertex outlier;outlier.position={300,60,40};outlier.bones={1,0,0,0};outlier.weights={1,0,0,0};combined.vertices.push_back(outlier);bare.meshes={combined};
    scene::imported::prepareMuzzleAnchors(bare,"exports/blacksquad/models/SK_Test_1P__123.cast");check(bare.muzzleAnchors.size()==1&&bare.muzzleAnchors[0].bone==0&&bare.muzzleAnchors[0].local.x==100&&bare.muzzleAnchors[0].local.y==0&&bare.muzzleAnchors[0].local.z==8);
    std::cout<<"Policy failures: "<<failed<<'\n';return failed?1:0;
}
