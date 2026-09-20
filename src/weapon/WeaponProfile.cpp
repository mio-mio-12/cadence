#include "weapon/WeaponProfile.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace weapon {
namespace {
std::string lower(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
bool contains(const std::string& value,const char* token){return value.find(token)!=std::string::npos;}
}

const char* archetypeName(Archetype value){switch(value){case Archetype::Rifle:return "rifle";case Archetype::Smg:return "smg";case Archetype::Lmg:return "lmg";case Archetype::Shotgun:return "shotgun";case Archetype::BoltSniper:return "bolt_sniper";case Archetype::BoltIndividualSniper:return "bolt_individual_sniper";case Archetype::SemiSniper:return "semi_sniper";case Archetype::Pistol:return "pistol";case Archetype::DualWield:return "dual_wield";case Archetype::Launcher:return "launcher";case Archetype::Equipment:return "equipment";default:return "unknown";}}
Archetype archetypeFromName(std::string value){value=lower(std::move(value));for(const auto type:{Archetype::Rifle,Archetype::Smg,Archetype::Lmg,Archetype::Shotgun,Archetype::BoltSniper,Archetype::BoltIndividualSniper,Archetype::SemiSniper,Archetype::Pistol,Archetype::DualWield,Archetype::Launcher,Archetype::Equipment})if(value==archetypeName(type))return type;return Archetype::Unknown;}
Archetype inferArchetype(std::string value){value=lower(std::move(value));const auto any=[&](std::initializer_list<const char*> names){return std::any_of(names.begin(),names.end(),[&](const char* name){return contains(value,name);});};
    if(any({"akimbo","sac3dw","xmgdw"})||(contains(value,"dual")&&!any({"dualmag","dual_mag","dualband","dual_band"})))return Archetype::DualWield;
    // ---- Launchers ----
    if(any({"launcher","rpg","javelin","stinger","maaws","smaw","at4","m79","thumper","xm25","m320","fhj18","usrpg",
        "kastet","panzerfaust","mk32","stingerm7","mahem",
        "m72_law","strela","china_lake","grim_reaper",
        "spartansa3","plaw","howitzer","venomx",
        "xm53","blackcell","l4siege","maxgl",
        "launcher_standard","launcher_lockonly","launcher_multi","launcher_ex41"
        }))return Archetype::Launcher;
    if(any({"equipment","c4","claymore","trophy","satchel","_ied","ied_"}))return Archetype::Equipment;
    // ---- Bolt-Action Individual Reload Snipers ----
    if(any({"m40a3","remington700","remington_700","r700"}))return Archetype::BoltIndividualSniper;
    // ---- Bolt-Action Magazine Snipers ----
    if(any({"dsr","ballista","bolt","l96","l96a1","mors","atlas20","atlas20mm","m200","kbsniper","dbl50","gaussgun","awp","ssg08",
        "intervention","cheytac","l118a","msr","vks",
        "kbs","longbow","tf141","trek50","widowmaker",
        "locus","svg100","rsa","dbsr50",
        "wpn_t9_sniper_standard","sniper_fastbolt","sniper_powerbolt","sniper_quickscope","sniper_double"
        })||(contains(value,"usr")&&!contains(value,"usrpg")))return Archetype::BoltSniper;
    // ---- Semi-Auto Snipers / Marksman ----
    if(any({"sniper","svu","xpr","dragunov","drakon","wa2000","psg1","barrett","as50","lynx","na45","udm45","scar20","g3sg1",
        "m82","m21","m14ebr","mk14","ia2","mr28","rsass",
        "svo","mavericksniper","maverick_sniper","d25s","sdmr","sieger300",
        "ebr800","dmr1","proteus","p06",
        "sniper_fastsemi","sniper_chargeshot","sniper_xpr50"
        }))return Archetype::SemiSniper;
    // ---- Shotguns ----
    if(any({"shotty","shotgun","870","ksg","m1216","srm1216","winchest1200","winchester","winchester1200","benelli","benellim4","tac19","bulldog","vm_s12",
        "spas12","model1887","striker","aa12","usas12","m1014",
        "fp6","fabarm","mts255","tac12","uts_15","uts15","maul",
        "rottweil72","olympia","ithaca","stakeout","hs10","ranger",
        "s12","blunderbuss","cauterizer","cel3","ohm",
        "kamchatka","trenchgun",
        "reaver","banshee","dcm8","rack9","m2187","sravage",
        "krm262","brecci","haymaker","argus","banshii",
        "shotgun_pump","shotgun_semiauto","shotgun_fullauto","shotgun_precision","shotgun_energy","shotgun_olympia"
        }))return Archetype::Shotgun;
    // ---- Pistols ----
    if(any({"pistol","judge","executioner","b2023r","fiveseven","fnfiveseven","fnp45","tac45","desert_eagle","deserteagle","beretta","colt45","m1911","viewmodel_usp","usp",
        "vm_rw1","rw1","vm_mp443","mp443","atlas45","weapon_emc","weapon_g18","weapon_nrgpistol",
        "coltanaconda","magnum","python","cz75","asp","makarov","p99","mp412","rex","kap40","kard",
        "p226","m9a1","m9","deagle",
        "prokolot","br9","deserteaglegold",
        "emc","oni","kendall44","hailstorm","udm","stallion44","hornet","dbl50",
        "mr6","rk5","lcar9","rifte9","marshal16",
        "pistol_standard","pistol_burst","pistol_fullauto","pistol_shotgun","pistol_energy","pistol_m1911",
        "glock","g18","beretta393","m93","tmp","pp2000","fmg9","mp9"
        }))return Archetype::Pistol;
    // ---- LMGs ----
    if(any({"lmg","weapon_saw","weapon_sdflmg","viewmodel_saw","viewmodel_rpd","viewmodel_m60","vm_em1","vm_ameli","vm_pytaek","vm_xmg","vm_epm3",
        "hamr","mk48","qbb95","lsat","m27iar","rpd","m60","m60e4","aug_hbar","m249","saw",
        "sa80","l86","mg4","m240","mg36","pkp","pecheneg","mk46",
        "hk21","rpk","stoner63",
        "em1","epm3","xmg","chainsaw","kac_chainsaw","ameli","pytaek","pkm",
        "raw","mauler","titan","auger","devastator",
        "brm","dingo","gorgon","dredge","ajax",
        "lmg_light","lmg_cqb","lmg_slowfire","lmg_heavy","lmg_infinite"
        }))return Archetype::Lmg;
    // ---- SMGs ----
    if(any({"smg","mp7","vector","weapon_ump","weapon_ripper","weapon_fmg","viewmodel_mp5","viewmodel_skorpion","viewmodel_ak74u","viewmodel_uzi","viewmodel_p90",
        "vm_asm1","vm_kf5","vm_mp11","vm_sn6","vm_sac3","vm_amr9",
        "pdw57","skorpion","evoskorpion","peacekeeper","vepr","cbjms","cbj_ms","bizon","k7","mtar","mtarx",
        "ump45","mp5","mp5k","p90","pp90m1","asm1","mp40","kf5","sn6","mp11","amr9",
        "ak74u","uzi","mini_uzi","mac11","mac10","pm63","mpl","spectre","kiparis","sten",
        "msmc","qcw05","chicom","ripper","vbr_pdw","pm9","kriss",
        "erad","fhr40","karma45","rprevo","rpr","hvr","trencher","vpr","mactav45",
        "kuda","vmp","weevil","vesper","pharo","razorback","ppsh","xmc","hlx4","nailgun",
        "fang45","bityug",
        "smg_standard","smg_versatile","smg_capacity","smg_fastfire","smg_burst","smg_longrange","smg_mp40","smg_ppsh","smg_ak74u","smg_msmc"
        }))return Archetype::Smg;
    // ---- Assault Rifles ----
    if(any({"ar_","rifle","assault","ak12","arx160","arx_160","honeybadger","remington_r5","remingtonr5","msbs","sc2010","sa805","an94","hk416","scar",
        "weapon_arx160","weapon_ar57","weapon_crdb","weapon_sdfar","weapon_m8",
        "viewmodel_ak47","viewmodel_m4","viewmodel_m16","viewmodel_g3","viewmodel_g36c","viewmodel_mp44",
        "m4a1","m4","m16","acr","fal","tar21","bal27","hbra3","imr",
        "vm_bal27","vm_hbra3","vm_ak12","vm_arx160","vm_imr","vm_mk14","vm_m16",
        "mp44","g3","g36c","masada","fn2000","f2000",
        "cm901","fad","xm8","sa58","saritch","sig556","tavor","type95",
        "enfield","galil","commando","car15","fnfal","aug",
        "maverick","stg44",
        "bos14","xmlar",
        "nv4","r3k","kbar32","kbar","type2","volk","rvn","xeon","grail","osa","m1garand",
        "kn44","xr2","hvk30","icr","manowar","sheiva","m8a7","ffar","kvk99m","basilisk",
        "ar_standard","ar_fastburst","ar_cqb","ar_accurate","ar_damage","ar_marksman","ar_longburst","ar_garand","ar_famas","ar_peacekeeper","ar_pulse","ar_m16","ar_galil","ar_an94"
        }))return Archetype::Rifle;
    return Archetype::Unknown;}

Stats defaultsFor(Archetype value){Stats s;s.hideWeaponOnAds=isSniper(value);switch(value){case Archetype::Rifle:s.fireTime=.095f;s.fullAuto=true;s.reloadTime=2.3f;s.reloadEmptyTime=2.8f;s.damage=35.0f;break;case Archetype::Smg:s.fireTime=.075f;s.fullAuto=true;s.reloadTime=2.0f;s.reloadEmptyTime=2.5f;s.raiseTime=.50f;s.moveSpeedScale=1.0f;s.damage=30.0f;break;case Archetype::Lmg:s.fireTime=.09f;s.fullAuto=true;s.reloadTime=5.5f;s.reloadEmptyTime=6.0f;s.raiseTime=.72f;s.moveSpeedScale=.92f;s.damage=40.0f;break;case Archetype::Shotgun:s.fireTime=.55f;s.reloadTime=.62f;s.rechamberTime=.72f;s.raiseTime=.62f;s.boltAction=true;s.damage=150.0f;break;case Archetype::BoltIndividualSniper:s.fireTime=.9f;s.reloadTime=.7f;s.reloadEmptyTime=4.2f;s.rechamberTime=1.05f;s.boltAction=true;s.individualReload=true;s.adsIn=.32f;s.adsOut=.25f;s.raiseTime=.75f;s.moveSpeedScale=.95f;s.adsFov=30.0f;s.damage=1000.0f;break;case Archetype::BoltSniper:s.fireTime=.9f;s.reloadTime=2.8f;s.reloadEmptyTime=3.3f;s.rechamberTime=1.05f;s.boltAction=true;s.adsIn=.32f;s.adsOut=.25f;s.raiseTime=.75f;s.moveSpeedScale=.95f;s.adsFov=30.0f;s.damage=1000.0f;break;case Archetype::SemiSniper:s.fireTime=.22f;s.reloadTime=2.7f;s.reloadEmptyTime=3.2f;s.adsIn=.30f;s.raiseTime=.72f;s.moveSpeedScale=.95f;s.adsFov=30.0f;s.damage=1000.0f;break;case Archetype::Pistol:s.fireTime=.12f;s.reloadTime=1.65f;s.reloadEmptyTime=2.0f;s.dropTime=.25f;s.raiseTime=.35f;s.quickDropTime=.18f;s.quickRaiseTime=.28f;s.moveSpeedScale=1.0f;s.damage=35.0f;break;case Archetype::DualWield:s.fireTime=.12f;s.reloadTime=2.5f;s.reloadEmptyTime=2.8f;s.adsFov=65.0f;s.damage=35.0f;break;case Archetype::Launcher:s.fireTime=.8f;s.reloadTime=3.0f;s.raiseTime=.8f;s.moveSpeedScale=.9f;s.damage=500.0f;break;case Archetype::Equipment:s.fireTime=.4f;s.raiseTime=.4f;s.adsFov=65.0f;s.damage=150.0f;break;default:break;}return s;}

Profile makeGenerated(std::string name,Archetype archetype,std::string animationPrefix){Profile result;result.name=std::move(name);result.internalName=lower(result.name);result.archetype=archetype;result.animationPrefix=std::move(animationPrefix);result.stats=defaultsFor(archetype);return result;}
std::vector<std::string> animationVariantsFor(const Profile& profile,const std::string& action,const std::string& pairing){
    const auto key=pairing.empty()?action:action+"."+pairing;
    if(const auto found=profile.animationVariants.find(key);found!=profile.animationVariants.end()&&!found->second.empty())return found->second;
    if(!pairing.empty()){
        if(const auto paired=profile.animations.find(key);paired!=profile.animations.end()&&!paired->second.empty())return {paired->second};
    }
    if(const auto found=profile.animationVariants.find(action);found!=profile.animationVariants.end()&&!found->second.empty())return found->second;
    if(const auto found=profile.animations.find(action);found!=profile.animations.end()&&!found->second.empty())return {found->second};
    return {};
}
std::optional<std::string> animationFor(const Profile& profile,const std::string& action,const std::string& pairing){const auto variants=animationVariantsFor(profile,action,pairing);return variants.empty()?std::nullopt:std::optional<std::string>{variants.front()};}
const AnimationOffset* offsetFor(const Profile& profile,const std::string& animation){const auto wanted=lower(animation);for(const auto& offset:profile.animationOffsets)if(lower(offset.animation)==wanted)return &offset;return nullptr;}

bool save(const Profile& p,const std::filesystem::path& path,std::string& error){std::error_code ec;if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),ec);std::ofstream out(path,std::ios::trunc);if(!out){error="Could not create profile";return false;}out<<"IWWEAPON "<<p.version<<'\n'<<"name "<<std::quoted(p.name)<<'\n'<<"internal "<<std::quoted(p.internalName)<<'\n'<<"source "<<std::quoted(p.source)<<'\n'<<"archetype "<<archetypeName(p.archetype)<<'\n'<<"animation_prefix "<<std::quoted(p.animationPrefix)<<'\n'<<"view_model "<<std::quoted(p.viewModelName)<<'\n'<<"world_model "<<std::quoted(p.worldModelName)<<'\n'<<"hand_model "<<std::quoted(p.handModelName)<<'\n'<<"base_model "<<std::quoted(p.baseModel)<<'\n';for(const auto& model:p.rigModels)out<<"rig_model "<<std::quoted(model)<<'\n';
    out<<"scope_overlay "<<std::quoted(p.scopeOverlayImage)<<'\n'<<"recoil_curve "<<p.stats.recoil<<'\n'<<"recoil_intensity "<<p.stats.recoil.intensity<<'\n';
    out<<"gun_position "<<p.gunPosition.x<<' '<<p.gunPosition.y<<' '<<p.gunPosition.z<<'\n'
       <<"ads_gun_position "<<p.separateAdsPosition<<' '<<p.adsGunPosition.x<<' '<<p.adsGunPosition.y<<' '<<p.adsGunPosition.z<<'\n'
       <<"material camo_luma "<<(p.materials.useBaseColorLumaMask?1:0)<<'\n'
       <<"material camo_invert "<<(p.materials.invertCamoMask?1:0)<<'\n'
       <<"material camo_alpha "<<p.materials.camoAlphaLow<<' '<<p.materials.camoAlphaHigh<<'\n'
       <<"material camo_luma_levels "<<p.materials.camoLumaLow<<' '<<p.materials.camoLumaHigh<<' '<<p.materials.camoLumaGamma<<' '<<p.materials.camoLumaContrast<<'\n'
       <<"material specular_multiplier "<<p.materials.specularMultiplier<<'\n'
       <<"material metalness_override "<<p.materials.overrideMetalness<<' '<<p.materials.metalness<<'\n'
       <<"material metalness_diffuse "<<p.materials.metalnessFromDiffuse<<' '<<p.materials.metalnessBlack<<' '<<p.materials.metalnessWhite<<' '<<p.materials.metalnessGamma<<'\n'
       <<"material cubemap_specular_intensity "<<p.materials.cubemapSpecularIntensity<<'\n'
       <<"material specular_color_levels "<<p.materials.specularColorLow.x<<' '<<p.materials.specularColorLow.y<<' '<<p.materials.specularColorLow.z<<' '<<p.materials.specularColorHigh.x<<' '<<p.materials.specularColorHigh.y<<' '<<p.materials.specularColorHigh.z<<'\n';
    const auto f=[&](const char* key,float value){out<<"stat "<<key<<' '<<std::setprecision(9)<<value<<'\n';};const auto i=[&](const char* key,int value){out<<"stat "<<key<<' '<<value<<'\n';};
    i("rechamber_delay_from_fire_end",p.stats.rechamberDelayFromFireEnd?1:0);i("can_fire_while_rechambering",p.stats.canFireWhileRechambering?1:0);f("rechamber_fire_unlock",p.stats.rechamberFireUnlock);f("rechamber_start_delay",p.stats.rechamberStartDelay);f("sprint_playback_scale",p.stats.sprintPlaybackScale);f("damage",p.stats.damage);f("fire_time",p.stats.fireTime);f("rechamber_time",p.stats.rechamberTime);f("ads_in",p.stats.adsIn);f("ads_out",p.stats.adsOut);f("drop_time",p.stats.dropTime);f("raise_time",p.stats.raiseTime);f("yy_return_scale",p.stats.yyReturnScale);f("reload_time",p.stats.reloadTime);f("reload_empty_time",p.stats.reloadEmptyTime);f("first_raise_time",p.stats.firstRaiseTime);f("melee_time",p.stats.meleeTime);f("sprint_in_time",p.stats.sprintInTime);f("sprint_loop_time",p.stats.sprintLoopTime);f("sprint_out_time",p.stats.sprintOutTime);f("quick_drop_time",p.stats.quickDropTime);f("quick_raise_time",p.stats.quickRaiseTime);f("ads_fov",p.stats.adsFov);f("move_speed_scale",p.stats.moveSpeedScale);f("ads_kick_pitch_min",p.stats.adsKickPitchMin);f("ads_kick_pitch_max",p.stats.adsKickPitchMax);f("ads_kick_yaw_min",p.stats.adsKickYawMin);f("ads_kick_yaw_max",p.stats.adsKickYawMax);f("ads_kick_center_speed",p.stats.adsKickCenterSpeed);f("hip_kick_pitch_min",p.stats.hipKickPitchMin);f("hip_kick_pitch_max",p.stats.hipKickPitchMax);f("hip_kick_yaw_min",p.stats.hipKickYawMin);f("hip_kick_yaw_max",p.stats.hipKickYawMax);f("hip_kick_center_speed",p.stats.hipKickCenterSpeed);i("burst_count",p.stats.burstCount);f("burst_delay",p.stats.burstDelay);i("full_auto",p.stats.fullAuto?1:0);i("bolt_action",p.stats.boltAction?1:0);i("individual_reload",p.stats.individualReload?1:0);i("hide_weapon_on_ads",p.stats.hideWeaponOnAds?1:0);
    for(const auto& [key,value]:p.animations)out<<"animation "<<std::quoted(key)<<' '<<std::quoted(value)<<'\n';
    for(const auto& [key,value]:p.animationFiles)out<<"animation_file "<<std::quoted(key)<<' '<<std::quoted(value)<<'\n';
    for(const auto& [key,values]:p.animationVariants)for(const auto& value:values)if(!value.empty())out<<"animation_variant "<<std::quoted(key)<<' '<<std::quoted(value)<<'\n';
    for(const auto& o:p.animationOffsets)out<<"offset "<<std::quoted(o.animation)<<' '<<o.position.x<<' '<<o.position.y<<' '<<o.position.z<<' '<<o.rotationDegrees.x<<' '<<o.rotationDegrees.y<<' '<<o.rotationDegrees.z<<' '<<o.scale<<'\n';
    for(const auto& m:p.rigMounts){
        if(m.attachedModel){
            out<<"attachment_mount "<<std::quoted(m.model)<<' '<<std::quoted(m.parentTag)<<' '<<m.position.x<<' '<<m.position.y<<' '<<m.position.z<<' '<<m.attachmentRotationDegrees.x<<' '<<m.attachmentRotationDegrees.y<<' '<<m.attachmentRotationDegrees.z<<' '<<m.attachmentScale.x<<' '<<m.attachmentScale.y<<' '<<m.attachmentScale.z<<'\n';
        }else
        out<<"mount "<<std::quoted(m.model)<<' '<<std::quoted(m.rootBone)<<' '<<std::quoted(m.parentTag)<<' '<<m.position.x<<' '<<m.position.y<<' '<<m.position.z<<' '<<m.rotation.x<<' '<<m.rotation.y<<' '<<m.rotation.z<<' '<<m.rotation.w<<'\n';
        if(m.hasCustomCamoLuma){
            out<<"mount_camo_luma "<<std::quoted(m.model)<<' '<<(m.materials.useBaseColorLumaMask?1:0)<<' '<<(m.materials.invertCamoMask?1:0)<<' '<<m.materials.camoLumaLow<<' '<<m.materials.camoLumaHigh<<' '<<m.materials.camoLumaGamma<<' '<<m.materials.camoLumaContrast<<'\n';
        }
    }
    for(const auto& tag:p.hiddenTags)out<<"hidden_tag "<<std::quoted(tag)<<'\n';out<<"lock_inspect_rig "<<(p.lockInspectRig?1:0)<<'\n';out<<"lock_inspect_grip "<<(p.lockInspectGrip?1:0)<<'\n';out<<"melee_weapon "<<(p.meleeWeapon?1:0)<<'\n';out<<"knife_settings "<<p.knifeRadiusCm<<' '<<p.knifeConeDegrees<<'\n';if(!out){error="Could not finish writing profile";return false;}error.clear();return true;}

bool load(const std::filesystem::path &path, Profile &p, std::string &error) {
  std::ifstream in(path);
  std::string magic;
  if (!(in >> magic) || magic != "IWWEAPON" || !(in >> p.version)) {
    error = "Not an IW weapon profile";
    return false;
  }
  Profile result;
  result.stats.rechamberStartDelay=-1.f;result.stats.rechamberDelayFromFireEnd=false;
  result.version = p.version;
  bool hasAdsVisibility=false;
  std::string line;
  std::getline(in, line);
  while (std::getline(in, line)) {
    std::istringstream row(line);
    std::string kind;
    if (!(row >> kind))
      continue;
    if (kind == "name")
      row >> std::quoted(result.name);
    else if (kind == "internal")
      row >> std::quoted(result.internalName);
    else if (kind == "source")
      row >> std::quoted(result.source);
    else if (kind == "archetype") {
      std::string value;
      row >> value;
      result.archetype = archetypeFromName(value);
    } else if (kind == "animation_prefix")
      row >> std::quoted(result.animationPrefix);
    else if (kind == "view_model")
      row >> std::quoted(result.viewModelName);
    else if (kind == "world_model")
      row >> std::quoted(result.worldModelName);
    else if (kind == "hand_model")
      row >> std::quoted(result.handModelName);
    else if (kind == "base_model")
      row >> std::quoted(result.baseModel);
    else if (kind == "rig_model") {
      std::string model;
      row >> std::quoted(model);
      if (row)
        result.rigModels.push_back(std::move(model));
    } else if (kind == "gun_position") {
      row >> result.gunPosition.x >> result.gunPosition.y >>
          result.gunPosition.z;
    } else if (kind == "ads_gun_position") {
      row >> result.separateAdsPosition >> result.adsGunPosition.x >> result.adsGunPosition.y >> result.adsGunPosition.z;
    } else if (kind == "lock_inspect_rig") {
      int value{};
      row >> value;
      result.lockInspectRig = value != 0;
    } else if (kind == "lock_inspect_grip") {
      int value{};
      row >> value;
      result.lockInspectGrip = value != 0;
    } else if (kind == "material") {
      std::string key;
      row >> key;
      if (key == "camo_luma") {
        int value{};
        row >> value;
        result.materials.useBaseColorLumaMask = value != 0;
      } else if (key == "camo_invert") {
        int value{};
        row >> value;
        result.materials.invertCamoMask = value != 0;
      } else if (key == "camo_alpha")
        row >> result.materials.camoAlphaLow >> result.materials.camoAlphaHigh;
      else if (key == "camo_luma_levels")
        row >> result.materials.camoLumaLow >> result.materials.camoLumaHigh >>
            result.materials.camoLumaGamma >> result.materials.camoLumaContrast;
      else if (key == "specular_multiplier")
        row >> result.materials.specularMultiplier;
      else if (key == "metalness_diffuse") {
        int enabled=0;float black=0,white=1,gamma=1;row>>enabled>>black>>white>>gamma;
        if(!row||enabled<0||enabled>1||!std::isfinite(black)||!std::isfinite(white)||!std::isfinite(gamma)||black<0||black>1||white<0||white>1||gamma<.05f||gamma>5){error="Invalid diffuse metalness levels";return false;}
        result.materials.metalnessFromDiffuse=enabled!=0;result.materials.metalnessBlack=black;result.materials.metalnessWhite=white;result.materials.metalnessGamma=gamma;
      }
      else if (key == "metalness_override") {
        int enabled=0;float value=0;row >> enabled >> value;
        if(!row||enabled<0||enabled>1||!std::isfinite(value)||value<0||value>1){error="Invalid weapon metalness override";return false;}
        result.materials.overrideMetalness=enabled!=0;result.materials.metalness=value;
      }
      else if (key == "cubemap_specular_intensity")
        row >> result.materials.cubemapSpecularIntensity;
      else if (key == "specular_color_levels")
        row >> result.materials.specularColorLow.x >>
            result.materials.specularColorLow.y >>
            result.materials.specularColorLow.z >>
            result.materials.specularColorHigh.x >>
            result.materials.specularColorHigh.y >>
            result.materials.specularColorHigh.z;
    } else if (kind == "animation") {
      std::string key, value;
      row >> std::quoted(key) >> std::quoted(value);
      if (row)
        result.animations[key] = value;
    } else if (kind == "animation_file") {
      std::string key,value;row >> std::quoted(key) >> std::quoted(value);
      if(!row||key.empty()||value.empty()){error="Invalid animation source dependency";return false;}
      result.animationFiles[key]=value;
    } else if (kind == "scope_overlay") { row >> std::quoted(result.scopeOverlayImage);
    } else if (kind == "recoil_curve") { const float intensity=result.stats.recoil.intensity;row >> result.stats.recoil;result.stats.recoil.intensity=intensity;if(!row){error="Invalid recoil curve";return false;}
    } else if (kind == "recoil_intensity") { row >> result.stats.recoil.intensity;result.stats.recoil.sanitize();
    } else if (kind == "animation_variant") {
      std::string key, value;
      row >> std::quoted(key) >> std::quoted(value);
      if (row && !value.empty())
        result.animationVariants[key].push_back(std::move(value));
    } else if (kind == "offset") {
      AnimationOffset o;
      row >> std::quoted(o.animation) >> o.position.x >> o.position.y >>
          o.position.z >> o.rotationDegrees.x >> o.rotationDegrees.y >>
          o.rotationDegrees.z >> o.scale;
      if (row)
        result.animationOffsets.push_back(std::move(o));
    } else if (kind == "attachment_mount") {
      RigMount m; m.attachedModel=true;
      row >> std::quoted(m.model) >> std::quoted(m.parentTag)
          >> m.position.x >> m.position.y >> m.position.z
          >> m.attachmentRotationDegrees.x >> m.attachmentRotationDegrees.y >> m.attachmentRotationDegrees.z
          >> m.attachmentScale.x >> m.attachmentScale.y >> m.attachmentScale.z;
      if(row)result.rigMounts.push_back(std::move(m));
    } else if (kind == "mount") {
      RigMount m;
      row >> std::quoted(m.model) >> std::quoted(m.rootBone) >>
          std::quoted(m.parentTag) >> m.position.x >> m.position.y >>
          m.position.z >> m.rotation.x >> m.rotation.y >> m.rotation.z >>
          m.rotation.w;
      if (row)
        result.rigMounts.push_back(std::move(m));
    } else if (kind == "mount_camo_luma") {
      std::string model;
      int useMask{}, invert{};
      float low{}, high{1.0f}, gamma{1.0f}, contrast{1.0f};
      row >> std::quoted(model) >> useMask >> invert >> low >> high >> gamma >> contrast;
      for (auto& m : result.rigMounts) {
        if (m.model == model) {
          m.hasCustomCamoLuma = true;
          m.materials.useBaseColorLumaMask = (useMask != 0);
          m.materials.invertCamoMask = (invert != 0);
          m.materials.camoLumaLow = low;
          m.materials.camoLumaHigh = high;
          m.materials.camoLumaGamma = gamma;
          m.materials.camoLumaContrast = contrast;
          break;
        }
      }
    } else if (kind == "hidden_tag") {
      std::string tag;
      row >> std::quoted(tag);
      if (row)
        result.hiddenTags.push_back(std::move(tag));
    } else if (kind == "knife_settings") {
      float radius{},cone{};
      if(!(row>>radius>>cone)||!std::isfinite(radius)||!std::isfinite(cone)){
        error="Invalid knife settings";return false;
      }
      result.knifeRadiusCm=std::clamp(radius,0.0f,1000.0f);
      result.knifeConeDegrees=std::clamp(cone,1.0f,80.0f);
    } else if (kind == "melee_weapon") {
      int value{};
      row >> value;
      result.meleeWeapon = value != 0;
    } else if (kind == "stat") {
      std::string key;
      float value{};
      row >> key >> value;
      if (!row)
        continue;
      auto &s = result.stats;
#define IW_STAT(name,field) if(key==name)s.field=value;else
            if(key=="burst_count")s.burstCount=std::max(1,static_cast<int>(value));else if(key=="full_auto")s.fullAuto=value!=0;else if(key=="bolt_action")s.boltAction=value!=0;else if(key=="individual_reload")s.individualReload=value!=0;else if(key=="hide_weapon_on_ads"){s.hideWeaponOnAds=value!=0;hasAdsVisibility=true;}else if(key=="rechamber_delay_from_fire_end")s.rechamberDelayFromFireEnd=value!=0;else if(key=="can_fire_while_rechambering")s.canFireWhileRechambering=value!=0;else IW_STAT("rechamber_fire_unlock",rechamberFireUnlock) IW_STAT("rechamber_start_delay",rechamberStartDelay) IW_STAT("sprint_playback_scale",sprintPlaybackScale) IW_STAT("damage",damage) IW_STAT("fire_time",fireTime) IW_STAT("rechamber_time",rechamberTime) IW_STAT("ads_in",adsIn) IW_STAT("ads_out",adsOut) IW_STAT("drop_time",dropTime) IW_STAT("raise_time",raiseTime) IW_STAT("yy_return_scale",yyReturnScale) IW_STAT("reload_time",reloadTime) IW_STAT("reload_empty_time",reloadEmptyTime) IW_STAT("first_raise_time",firstRaiseTime) IW_STAT("melee_time",meleeTime) IW_STAT("sprint_in_time",sprintInTime) IW_STAT("sprint_loop_time",sprintLoopTime) IW_STAT("sprint_out_time",sprintOutTime) IW_STAT("quick_drop_time",quickDropTime) IW_STAT("quick_raise_time",quickRaiseTime) IW_STAT("ads_fov",adsFov) IW_STAT("move_speed_scale",moveSpeedScale) IW_STAT("ads_kick_pitch_min",adsKickPitchMin) IW_STAT("ads_kick_pitch_max",adsKickPitchMax) IW_STAT("ads_kick_yaw_min",adsKickYawMin) IW_STAT("ads_kick_yaw_max",adsKickYawMax) IW_STAT("ads_kick_center_speed",adsKickCenterSpeed) IW_STAT("hip_kick_pitch_min",hipKickPitchMin) IW_STAT("hip_kick_pitch_max",hipKickPitchMax) IW_STAT("hip_kick_yaw_min",hipKickYawMin) IW_STAT("hip_kick_yaw_max",hipKickYawMax) IW_STAT("hip_kick_center_speed",hipKickCenterSpeed) IW_STAT("burst_delay",burstDelay) {}
#undef IW_STAT
    }
  }
  if(!hasAdsVisibility)result.stats.hideWeaponOnAds=isSniper(result.archetype);
  p = std::move(result);
  error.clear();
  return true;
}

bool importLegacyWeaponFile(Profile& profile,const std::filesystem::path& path,std::string& error){
    std::ifstream input(path,std::ios::binary);std::string data((std::istreambuf_iterator<char>(input)),{});
    constexpr std::string_view magic="WEAPONFILE\\";if(!data.starts_with(magic)){error="Not a classic IW weaponfile";return false;}
    std::vector<std::string> tokens;std::size_t begin=magic.size();
    while(begin<=data.size()){const auto end=data.find('\\',begin);tokens.push_back(data.substr(begin,end==std::string::npos?std::string::npos:end-begin));if(end==std::string::npos)break;begin=end+1;}
    std::unordered_map<std::string,std::string> fields;for(std::size_t i=0;i+1<tokens.size();i+=2)fields[tokens[i]]=tokens[i+1];
    const auto text=[&](const char* key){const auto found=fields.find(key);return found==fields.end()?std::string{}:found->second;};
    const auto number=[&](const char* key,float fallback){try{const auto value=text(key);return value.empty()?fallback:std::stof(value);}catch(...){return fallback;}};
    const auto truth=[&](const char* key,bool fallback=false){const auto value=lower(text(key));if(value.empty())return fallback;return value=="1"||value=="true"||value=="yes";};

    Profile result;result.name=path.filename().string();result.internalName=result.name;result.source=path.string();
    result.viewModelName=text("gunModel");result.worldModelName=text("worldModel");result.handModelName=text("handModel");
    const auto weaponClass=lower(text("weaponClass"));const bool dual=truth("dualWield")||contains(lower(result.internalName),"akimbo");const bool bolt=truth("boltAction");const auto identityArchetype=inferArchetype(result.internalName+" "+result.viewModelName);
    // T5 exports label L96A1/Dragunov/PSG1/WA2000 as weaponClass=rifle.
    // Their identity and boltAction field are authoritative for first-person
    // sniper presentation and Create-a-Class grouping.
    if(identityArchetype==Archetype::BoltIndividualSniper)result.archetype=Archetype::BoltIndividualSniper;else if(identityArchetype==Archetype::BoltSniper||identityArchetype==Archetype::SemiSniper)result.archetype=bolt?Archetype::BoltSniper:Archetype::SemiSniper;
    else if(dual)result.archetype=Archetype::DualWield;else if(weaponClass=="rifle")result.archetype=Archetype::Rifle;else if(weaponClass=="smg")result.archetype=Archetype::Smg;else if(weaponClass=="mg"||weaponClass=="lmg")result.archetype=Archetype::Lmg;else if(weaponClass=="spread"||weaponClass=="shotgun")result.archetype=Archetype::Shotgun;else if(weaponClass=="sniper")result.archetype=bolt?Archetype::BoltSniper:Archetype::SemiSniper;else if(weaponClass=="pistol")result.archetype=Archetype::Pistol;else if(weaponClass=="rocketlauncher"||weaponClass=="grenade")result.archetype=Archetype::Launcher;else result.archetype=identityArchetype;
    result.stats=defaultsFor(result.archetype);auto& stats=result.stats;
    stats.damage=number("damage",stats.damage);
    if(isSniper(result.archetype)&&stats.damage<200.0f)stats.damage=1000.0f;
    stats.fireTime=number("fireTime",stats.fireTime);stats.rechamberTime=number("rechamberTime",stats.rechamberTime);stats.adsIn=number("adsTransInTime",stats.adsIn);stats.adsOut=number("adsTransOutTime",stats.adsOut);stats.dropTime=number("dropTime",stats.dropTime);stats.raiseTime=number("raiseTime",stats.raiseTime);
    stats.reloadTime=number("reloadTime",stats.reloadTime);stats.reloadEmptyTime=number("reloadEmptyTime",stats.reloadEmptyTime>0?stats.reloadEmptyTime:stats.reloadTime);stats.firstRaiseTime=number("firstRaiseTime",stats.firstRaiseTime);stats.meleeTime=number("meleeTime",stats.meleeTime);stats.quickDropTime=number("quickDropTime",stats.quickDropTime);stats.quickRaiseTime=number("quickRaiseTime",stats.quickRaiseTime);
    stats.sprintInTime=number("sprintInTime",stats.sprintInTime);stats.sprintLoopTime=number("sprintLoopTime",stats.sprintLoopTime);stats.sprintOutTime=number("sprintOutTime",stats.sprintOutTime);stats.adsFov=number("adsZoomFov1",number("adsZoomFov",stats.adsFov));stats.moveSpeedScale=number("moveSpeedScale",stats.moveSpeedScale);
    const auto kick=[&](const char* key,float fallback){return number(key,fallback*100.0f)*0.01f;};stats.adsKickPitchMin=kick("adsViewKickPitchMin",stats.adsKickPitchMin);stats.adsKickPitchMax=kick("adsViewKickPitchMax",stats.adsKickPitchMax);stats.adsKickYawMin=kick("adsViewKickYawMin",stats.adsKickYawMin);stats.adsKickYawMax=kick("adsViewKickYawMax",stats.adsKickYawMax);stats.adsKickCenterSpeed=number("adsViewKickCenterSpeed",stats.adsKickCenterSpeed*100.0f)*0.01f;stats.hipKickPitchMin=kick("hipViewKickPitchMin",stats.hipKickPitchMin);stats.hipKickPitchMax=kick("hipViewKickPitchMax",stats.hipKickPitchMax);stats.hipKickYawMin=kick("hipViewKickYawMin",stats.hipKickYawMin);stats.hipKickYawMax=kick("hipViewKickYawMax",stats.hipKickYawMax);stats.hipKickCenterSpeed=number("hipViewKickCenterSpeed",stats.hipKickCenterSpeed*100.0f)*0.01f;
    const auto fireType=lower(text("fireType"));stats.fullAuto=contains(fireType,"full auto")||fireType=="auto";stats.burstCount=contains(fireType,"burst")?(contains(fireType,"3")?3:2):1;stats.burstDelay=number("burstFireDelay",stats.burstDelay);stats.boltAction=bolt;if(stats.boltAction&&stats.rechamberTime<0.25f)stats.rechamberTime=defaultsFor(result.archetype).rechamberTime;
    const std::pair<const char*,const char*> animationFields[]={{"idle","idleAnim"},{"empty_idle","emptyIdleAnim"},{"fire","fireAnim"},{"last_shot","lastShotAnim"},{"rechamber","rechamberAnim"},{"melee","meleeAnim"},{"reload","reloadAnim"},{"reload_empty","reloadEmptyAnim"},{"reload_start","reloadStartAnim"},{"reload_end","reloadEndAnim"},{"reload_quick","reloadQuickAnim"},{"pullout","raiseAnim"},{"putaway","dropAnim"},{"first_raise","firstRaiseAnim"},{"pullout_quick","quickRaiseAnim"},{"putaway_quick","quickDropAnim"},{"sprint_in","sprintInAnim"},{"sprint_loop","sprintLoopAnim"},{"sprint_out","sprintOutAnim"},{"ads_fire","adsFireAnim"},{"ads_last_shot","adsLastShotAnim"},{"ads_rechamber","adsRechamberAnim"},{"ads_up","adsUpAnim"},{"ads_down","adsDownAnim"}};
    for(const auto& [action,key]:animationFields){auto value=text(key);if(value.empty())continue;if(std::filesystem::path(value).extension().empty())value+=".cast";result.animations[action]=std::move(value);}
    if(const auto idle=result.animations.find("idle");idle!=result.animations.end()){auto stem=lower(std::filesystem::path(idle->second).stem().string());const auto marker=stem.rfind("_idle");if(marker!=std::string::npos)result.animationPrefix=stem.substr(0,marker+1);}
    profile=std::move(result);error.clear();return true;
}

} // namespace weapon
