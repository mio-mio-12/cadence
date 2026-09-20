#include "assets/AssetCatalog.h"
#include "assets/CharacterParts.h"

#include <iostream>
#include <algorithm>

namespace {bool expect(bool value,const char* message){if(!value)std::cerr<<"FAILED: "<<message<<'\n';return value;}}

int main(){int failures{};
    for(const auto* name:{"wpn_ak47_iw5_LOD0","wpn_model_1887_LOD0","weapon_model_1887_LOD0"})
        failures+=!expect(assets::classifyModelPath("mw3/models/flat.cast",name)==assets::Role::WorldWeapon,"MW3 world prefixes work without sorted directories");
    for(const auto* name:{"view_ak47_iw5_LOD0","viewmodel_model1887_iw5_LOD0","wpn_ak47_iw5_view_LOD0","wpn_ak47_iw5_viewmodel_LOD0"})
        failures+=!expect(assets::classifyModelPath("mw3/models/flat.cast",name)==assets::Role::ViewWeapon,"MW3 explicit view names override world prefixes");
    failures+=!expect(assets::classifyModelPath("mw3/models/flat.cast","wpn_ak47_scope_LOD0")==assets::Role::WorldAttachment,"MW3 wpn scope remains an attachment");
    failures+=!expect(assets::classifyModelPath("mw3/models/flat.cast","viewmodel_base_viewhands_LOD0")==assets::Role::ViewHands,"MW3 hands remain hands");
    failures+=!expect(assets::character::pointBlankIdentity("playermode_REBEL_ViperRed_Shadow__fb")==assets::character::pointBlankIdentity("viewmodel_REBEL_ViperRed_Shadow__hands"),"PB exact variant identity including doubled separator");
    failures+=!expect(assets::character::pointBlankFaction("playermode_Hide_Gign_fb")=="CT-Force","PB Hide stays CT rather than COD GIGN");
    failures+=!expect(assets::character::pointBlankFaction("viewmodel_D-Fox_hands")=="Free Rebels","PB D-Fox team");
    failures+=!expect(assets::character::pointBlankFaction("playermode_Bella_fb").empty(),"Unknown PB team is not guessed");
    failures+=!expect(assets::classifyModelPath("exported_files/pointblank/models/playermode_Chou_fb.cast","playermode_Chou_fb")==assets::Role::PlayerModel,"Flat PB body discovery");
    failures+=!expect(assets::classifyModelPath("exported_files/pointblank/models/viewmodel_Chou_hands.cast","viewmodel_Chou_hands")==assets::Role::ViewHands,"Flat PB hands discovery");
    using assets::character::Part;
    failures+=!expect(!assets::character::headCompatible("mw3","mp_body_delta_assault","head_delta_sniper_LOD0"),"MW3 sniper head rejected on assault body");
    failures+=!expect(assets::character::headCompatible("mw3","mp_body_delta_sniper","head_delta_sniper_LOD0"),"MW3 sniper head retained for sniper body");
    for(const auto* name:{"mp_body_fso_vest_c_dirty_LOD0","mp_view_glove_01a_LOD0","mp_exo_health_LOD0","mp_table_LOD0"})
        failures+=!expect(assets::character::awPart(name)==Part::None,"AW props, first-person parts and headless vest excluded from modular equipment");
    failures+=!expect(assets::classifyModelPath("aw/models/playermodels/mp_pants_m_01b_k/x.cast","mp_pants_m_01b_k_LOD0")==assets::Role::OtherModel,"AW pants are not complete character bases");
    failures+=!expect(assets::classifyModelPath("aw/models/playermodels/mp_top_m_a_01a/x.cast","mp_top_m_a_01a_LOD0")==assets::Role::PlayerModel,"AW torso is a modular character base");
    failures+=!expect(!assets::character::genderCompatible("mp_top_f_a_03a","mp_pants_m_01b_k"),"AW female top does not select male pants");
    failures+=!expect(assets::character::matchesSlot("aw","mp_exo_01a","Exo")&&!assets::character::matchesSlot("aw","mp_exo_health","Exo"),"AW exo picker excludes add-on modules");
    failures+=!expect(assets::classifyModelPath("bocw_sp/models/weapons/view/assault rifles/ar_damage/x.cast","wpn_t9_ar_damage_mag_view_LOD0")==assets::Role::ViewAttachment,"Cold War magazine in weapon directory stays an attachment");
    failures+=!expect(assets::classifyModelName("wpn_t9_sniper_standard_viewer_world_pc_LOD0")==assets::Role::WorldWeapon,"viewer cosmetic token is not first-person identity");
    failures+=!expect(assets::classifyModelPath("bocw_sp/models/weapons/world/snipers/x.cast","wpn_t9_sniper_standard_mag_world_LOD0")==assets::Role::WorldAttachment,"Cold War world magazine in weapon directory stays an attachment");
    {
        assets::Catalog catalog;assets::Asset weapon;weapon.game="bocw_sp";weapon.role=assets::Role::ViewWeapon;
        for(const char* name:{"wpn_t9_sniper_standard_scope_view_LOD0","wpn_t9_sniper_standard_carbon_fiber_scope_view_LOD0","wpn_t9_sniper_standard_scope_space_view_LOD0","wpn_t9_sniper_standard_scope_ads_space_view_LOD0"}){assets::Asset part;part.name=name;part.game=weapon.game;part.role=assets::Role::ViewAttachment;catalog.entries.push_back(part);}
        weapon.name="wpn_t9_sniper_standard_beachcomber_view_LOD0";
        failures+=!expect(assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[0]),"sniper cosmetic inherits exported family scope");
        weapon.name="wpn_t9_sniper_standard_carbon_fiber_view_LOD0";
        failures+=!expect(!assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[0])&&assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[1]),"sniper own cosmetic scope replaces fallback");
        weapon.name="wpn_t9_sniper_standard_space_view_LOD0";
        failures+=!expect(assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[2])&&!assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[3]),"sniper component-first cosmetic selects full scope not ADS insert");
        weapon.name="wpn_t9_sniper_cannon_variant_view_LOD0";
        failures+=!expect(!assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[0]),"no unrelated scope forced onto a family without exported default");
    }
    {
        assets::Catalog catalog;
        assets::Asset weapon;weapon.name="wpn_t9_ar_damage_czar_view_LOD0";weapon.game="bocw_sp";weapon.role=assets::Role::ViewWeapon;
        for(const char* name:{"wpn_t9_ar_damage_mag_view_LOD0","wpn_t9_ar_damage_mag_czar_view_LOD0","wpn_t9_ar_damage_achill_mag_view_LOD0","wpn_t9_ar_damage_scope_ads_view_LOD0"}){
            assets::Asset part;part.name=name;part.game=weapon.game;part.role=assets::Role::ViewAttachment;catalog.entries.push_back(part);
        }
        failures+=!expect(!assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[0]),"skin magazine takes precedence over base");
        failures+=!expect(assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[1]),"component-before-variant magazine selected");
        failures+=!expect(!assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[2]),"unrelated cosmetic rejected");
        failures+=!expect(!assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[3]),"ADS-only scope rejected");
        weapon.name="wpn_t9_ar_damage_achill_view_LOD0";
        failures+=!expect(assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[2]),"variant-before-component magazine selected");
        weapon.name="wpn_t9_ar_damage_nocturne_view_LOD0";
        failures+=!expect(assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[0]),"missing cosmetic magazine falls back to same-family base");
        weapon.name="wpn_t9_sniper_standard_view_LOD0";
        failures+=!expect(!assets::isPreferredColdWarRigPart(catalog,weapon,catalog.entries[0]),"family mismatch rejected");
    }
    for(const auto part:{"wpn_t9_sniper_standard_mag_view_LOD0","wpn_t9_sniper_standard_scope_view_LOD0"})
        failures+=!expect(assets::isDefaultColdWarRigPart("wpn_t9_sniper_standard_view_LOD0",part),"Cold War exact native magazine and scope autoload");
    for(const auto part:{"wpn_t9_sniper_standard_scope_ads_view_LOD0","wpn_t9_sniper_standard_scope_viewer_view_LOD0","wpn_t9_sniper_standard_scope_beachcomber_view_LOD0","wpn_t9_sniper_standard_mag_world_LOD0","wpn_t9_sniper_other_scope_view_LOD0"})
        failures+=!expect(!assets::isDefaultColdWarRigPart("wpn_t9_sniper_standard_view_LOD0",part),"Cold War automatic parts exclude alternate optics, cosmetics, and unrelated models");
    failures+=!expect(assets::classifyModelName("c_t9_cp_rus_pl_f_agent_nam_infil_combat_arms_mideast_LOD0")==assets::Role::ViewHands,"Cold War native arms classify");
    failures+=!expect(assets::classifyModelName("wpn_t9_sniper_standard_view_LOD0")==assets::Role::ViewWeapon,"Cold War native weapon classifies");
    failures+=!expect(assets::classifyModelName("wpn_t9_sniper_standard_beachcomber_world_LOD0")==assets::Role::WorldWeapon,"Cold War world variant classifies");
    failures+=!expect(assets::classifyModelName("wpn_t9_sniper_standard_scope_viewer_ads_view_pc_LOD0")==assets::Role::ViewAttachment,"Cold War scope stays out of weapon picker");
    failures+=!expect(assets::classifyModelName("wpn_t9_sniper_standard_mag_world_LOD0")==assets::Role::WorldAttachment,"Cold War magazine stays out of weapon picker");
    failures+=!expect(assets::classifyModelName("vfx_debris_rocks_3_g1_LOD0")==assets::Role::OtherModel,"Cold War debris not treated as weapons");
    failures+=!expect(assets::isDefaultIwRigPart("weapon_kb_m4_vm_camo_LOD0","weapon_kb_m4_vm_complex_1_LOD0"),"IW rifle accepts own-family candidate before geometry check");
    failures+=!expect(assets::isDefaultIwRigPart("weapon_emc_vm_camo_LOD0","weapon_emc_vm_complex_01_LOD0"),"IW pistol accepts padded own-family candidate before geometry check");
    failures+=!expect(!assets::isDefaultIwRigPart("weapon_emc_vm_camo_LOD0","weapon_dbl50_scope_vm_camo_complex_1_LOD0"),"camo token cannot mount an unrelated scope");
    failures+=!expect(!assets::isDefaultIwRigPart("weapon_kb_m4_vm_camo_LOD0","weapon_shotgun_sniper_scope_lgn_vm_camo_complex_1_LOD0"),"IW rifle rejects unrelated sniper assembly");
    failures+=!expect(!assets::isDefaultIwRigPart("weapon_emc_vm_camo_LOD0","weapon_emc_vm_complex_10_LOD0"),"complex_10 is not default complex_1");
    failures+=!expect(!assets::isDefaultIwRigPart("weapon_emc_vm_camo_LOD0","weapon_emc2_vm_complex_1_LOD0"),"IW family match requires token boundary");
    failures+=!expect(assets::classifyModelName("c_usa_mp_seal6_assault_fb_LOD0")==assets::Role::PlayerModel,"full-body player classifies");
    failures+=!expect(assets::classifyModelName("c_usa_mp_seal6_longsleeve_viewhands_LOD0")==assets::Role::ViewHands,"viewhands classify");
    failures+=!expect(assets::classifyModelName("t6_wpn_ar_an94_world_LOD0")==assets::Role::WorldWeapon,"world weapon classifies");
    failures+=!expect(assets::classifyModelName("t6_wpn_ar_an94_view_LOD0")==assets::Role::ViewWeapon,"view weapon classifies");
    failures+=!expect(assets::classifyModelName("t6_wpn_sniper_ballista_scope_view_LOD0")==assets::Role::ViewAttachment,"weapon-named sniper scope classifies as an attachment");
    failures+=!expect(assets::classifyModelName("t6_wpn_ar_hk416_grip_view_LOD0")==assets::Role::ViewAttachment,"weapon-named grip variant classifies as an attachment");
    failures+=!expect(assets::classifyModelName("t6_attach_fastmag_an94_world_LOD0")==assets::Role::WorldAttachment,"world attachment classifies");
    failures+=!expect(assets::classifyModelName("body_mp_usmc_assault_LOD0")==assets::Role::PlayerModel,"IW3 split player body classifies");
    failures+=!expect(assets::classifyModelName("mp_body_sas_urban_assault_LOD0")==assets::Role::PlayerModel,"IW5 multiplayer body classifies");
    failures+=!expect(assets::classifyModelName("mp_fullbody_ally_juggernaut_LOD0")==assets::Role::PlayerModel,"IW5 multiplayer full body classifies");
    failures+=!expect(assets::classifyModelName("viewmodel_ak47_mp_LOD0")==assets::Role::ViewWeapon,"IW3 viewmodel weapon classifies");
    failures+=!expect(assets::classifyModelName("t5_spectre_viewmodel_LOD0")==assets::Role::ViewWeapon,"T5 suffix-style viewmodel weapon classifies");
    failures+=!expect(assets::classifyModelName("viewmodel_acog_LOD0")==assets::Role::ViewAttachment,"IW5 universal optic classifies as a view attachment");
    failures+=!expect(assets::classifyModelName("viewmodel_remington_msr_scope_iw5_LOD0")==assets::Role::ViewAttachment,"IW5 weapon scope classifies as a view attachment");
    failures+=!expect(assets::classifyModelName("weapon_silencer_01_LOD0")==assets::Role::WorldAttachment,"IW5 silencer classifies as a world attachment");
    failures+=!expect(assets::classifyModelName("weapon_ak47_LOD0")==assets::Role::WorldWeapon,"IW3 world weapon classifies");
    failures+=!expect(assets::classifyModelName("vm_bal27_base_standard_LOD0")==assets::Role::ViewWeapon,"AW modular weapon classifies");
    failures+=!expect(assets::classifyModelName("vm_optics_acog2_LOD0")==assets::Role::ViewAttachment,"AW modular optic classifies");
    failures+=!expect(assets::classifyModelName("vm_view_arms_mech_mp_LOD0")==assets::Role::ViewHands,"AW mechanical view arms classify as hands");
    failures+=!expect(assets::classifyModelName("weapon_arx160iw7_vm_camo_LOD0")==assets::Role::ViewWeapon,"IW7 suffix-style view weapon classifies");
    failures+=!expect(assets::classifyModelName("weapon_ftlpistol_vm_LOD0")==assets::Role::ViewWeapon,"IW base without camo remains selectable with LOD suffix");
    failures+=!expect(assets::classifyModelName("weapon_kbcrb_vm_fallback_LOD0")==assets::Role::ViewWeapon,"IW fallback full weapon remains selectable");
    failures+=!expect(assets::classifyModelName("weapon_gaussgun_lgn_scope_vm_camo_LOD0")==assets::Role::ViewAttachment,"IW scope skeleton remains an attachment");
    failures+=!expect(assets::classifyModelName("weapon_arx160iw7_wm_camo_LOD0")==assets::Role::WorldWeapon,"IW7 suffix-style world weapon classifies");
    failures+=!expect(assets::classifyModelName("weapon_arx160iw7_vm_complex_3_LOD0")==assets::Role::ViewAttachment,"IW7 modular weapon piece classifies as a view attachment");
    failures+=!expect(assets::classifyModelName("attachment_elosight_vm_camo_LOD0")==assets::Role::ViewAttachment,"IW7 suffix-style optic classifies");
    failures+=!expect(assets::classifyModelName("mp_warfighter_body_1_1_LOD0")==assets::Role::PlayerModel,"IW7 modular player body classifies");
    const auto evoKeys=assets::compatibilityKeys("viewmodel_evopro_LOD0",assets::Role::ViewWeapon);
    failures+=!expect(std::find(evoKeys.begin(),evoKeys.end(),"evo")!=evoKeys.end()&&std::find(evoKeys.begin(),evoKeys.end(),"vector")==evoKeys.end(),"Ghosts evopro maps to evo, not Vector");
    const auto awKeys=assets::compatibilityKeys("vm_bal27_base_operator1_LOD0",assets::Role::ViewWeapon);failures+=!expect(awKeys.size()==1&&awKeys.front()=="bal27","AW variants collapse to their weapon-family compatibility key");
    assets::Catalog catalog;assets::Asset weapon;weapon.name="an94";weapon.game="bo2";weapon.role=assets::Role::WorldWeapon;weapon.compatibilityKeys=assets::compatibilityKeys("t6_wpn_ar_an94_world_LOD0",weapon.role);
    assets::Asset exact;exact.game="bo2";exact.role=assets::Role::WorldAttachment;exact.compatibilityKeys=assets::compatibilityKeys("t6_attach_fastmag_an94_world_LOD0",exact.role);
    assets::Asset category;category.game="bo2";category.role=assets::Role::WorldAttachment;category.compatibilityKeys=assets::compatibilityKeys("t6_attach_ar_silencer1_world_LOD0",category.role);
    assets::Asset wrong;wrong.game="bo2";wrong.role=assets::Role::WorldAttachment;wrong.compatibilityKeys=assets::compatibilityKeys("t6_attach_sniper_silencer1_world_LOD0",wrong.role);
    catalog.entries={weapon,exact,category,wrong};const auto matches=catalog.compatibleAttachments(0);
    failures+=!expect(assets::classifyModelName("head_mp_seal6_lods")==assets::Role::OtherModel,"player head model classifies as other model");
    failures+=!expect(assets::classifyModelName("mp_body_engineer_lods")==assets::Role::PlayerModel,"multiplayer body classifies as player model");
    failures+=!expect(assets::classifyModelName("mp_fullbody_pmc_lods")==assets::Role::PlayerModel,"multiplayer full body classifies as player model");
    failures+=!expect(assets::classifyModelName("viewmodel_reticle_reflex_lods")==assets::Role::ViewAttachment,"reticle model classifies as view attachment");
    failures+=!expect(assets::classifyModelName("weapon_an94_world")==assets::Role::WorldWeapon,"world weapon without prefix classifies as world weapon");
    const auto m14Keys=assets::compatibilityKeys("viewmodel_mk14_ebr",assets::Role::ViewWeapon);
    const auto iw6M14Keys=assets::compatibilityKeys("viewmodel_iw6_m14ebr_reload_empty",assets::Role::ViewWeapon);
    bool hasSharedKey=false;
    for(const auto& k1:m14Keys) for(const auto& k2:iw6M14Keys) if(k1==k2) hasSharedKey=true;
    failures+=!expect(hasSharedKey,"mk14 and m14ebr share compatibility alias key");
    assets::Catalog retargetCatalog;retargetCatalog.entries={
        {R"(D:\exports\cs2\models\weapons\ak47.cast)","ak47","cs2",assets::Role::ViewWeapon},
        {R"(D:\exports\bo2\models\viewhands\seal6.cast)","seal6","bo2",assets::Role::ViewHands}
    };
    failures+=!expect(assets::viewhandsGameForLoadedBase(retargetCatalog,1,retargetCatalog.entries[1].path)=="bo2","cross-game retarget target follows the loaded BO2 viewhands instead of the mounted CS2 weapon");
    failures+=!expect(assets::viewhandsGameForLoadedBase(retargetCatalog,99,retargetCatalog.entries[1].path)=="bo2","loaded viewhands path recovers the retarget target when selection is unavailable");
    failures+=!expect(assets::classifyModelName("worldmodel_smg_qq9_girlsfrontline")==assets::Role::WorldWeapon,"CODM leading worldmodel prefix");
    failures+=!expect(assets::classifyModelName("viewmodel_pistol_50gs_girlsfrontline")==assets::Role::ViewWeapon,"CODM view weapon remains a view weapon");
    failures+=!expect(assets::coldWarWeaponFamily("wpn_t9_bp_ar_damage_nocturne_view_LOD0")=="ar_damage","Cold War blueprint shares base animation family");
    failures+=!expect(assets::coldWarWeaponFamily("wpn_t9_loot_sniper_accurate_unicorn_view_LOD0")=="sniper_accurate","Cold War loot family");
    failures+=!expect(assets::coldWarWeaponVariant("wpn_t9_bp_ar_damage_nocturne_view_LOD0")=="nocturne","Cold War variant label");
    failures+=!expect(assets::coldWarWeaponVariant("wpn_t9_ar_damage_view_LOD0").empty(),"Cold War base label");
    failures+=!expect(assets::classifyModelName("wpn_t9_loot_pistol_shotgun_clip_view_LOD0")==assets::Role::ViewAttachment,"Cold War clip is not selectable gun");
    failures+=!expect(assets::classifyModelName("wpn_t9_loot_sniper_cannon_bullet_casing_view_LOD0")==assets::Role::ViewAttachment,"Cold War casing is not selectable gun");
    failures+=!expect(assets::classifyModelName("viewhands_sentinel_Default")==assets::Role::ViewHands,"native CODM viewhands prefix");
    failures+=!expect(assets::classifyModelName("codm_viewhands_sentinel_Default")==assets::Role::ViewHands,"existing CODM viewhands prefix");
    failures+=!expect(assets::classifyModelPath("exported_files/codm/models/special_weapon.cast","special_weapon")!=assets::Role::ViewHands,"special prefix alone is not hand provenance");
    if(!failures)std::cout<<"All asset catalog tests passed.\n";return failures?1:0;}
