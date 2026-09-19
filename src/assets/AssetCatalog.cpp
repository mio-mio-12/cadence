#include "assets/AssetCatalog.h"
#include "assets/CharacterParts.h"
#include "cast/CastDocument.h"
#include "scene/BoundedJson.h"
#include <fstream>

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace assets {
namespace {

std::string lower(std::string_view input){std::string value(input);std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
bool contains(std::string_view value,std::string_view part){return value.find(part)!=std::string_view::npos;}
std::vector<std::string> tokens(std::string_view input){std::vector<std::string> result;std::string token;
    for(const unsigned char c:input){if(std::isalnum(c))token.push_back(static_cast<char>(std::tolower(c)));else if(!token.empty()){result.push_back(std::move(token));token.clear();}}
    if(!token.empty())result.push_back(std::move(token));return result;}
bool lodAboveZero(std::string_view name){const auto position=name.rfind("_lod");if(position==std::string_view::npos)return false;const auto digit=position+4;return digit<name.size()&&name[digit]>='1'&&name[digit]<='9';}
std::string gameFromPath(const std::filesystem::path& path){
    for(const auto& part:path){const auto s=lower(part.string());if(s=="bocw"||s=="bocw_sp"||s=="t9")return s;}
    for(auto it=path.begin();it!=path.end();++it){
        const auto s=lower(it->string());
        if(s=="exported_files"||s=="saluki"||s=="greyhound"){
            auto next=std::next(it);
            while(next!=path.end()){
                const auto ns=lower(next->string());
                if(ns!="exported_files"&&ns!="saluki"&&ns!="greyhound"&&!ns.empty()){
                    return ns;
                }
                ++next;
            }
        }
    }
    for(const auto& part:path){
        const auto s=lower(part.string());
        if(s=="iw3"||s=="iw4"||s=="iw5"||s=="iw6"||s=="iw7"||s=="iw_sp"||s=="t5"||s=="t6"||s=="t7"||s=="s1"||s=="s2"||s=="aw"||s=="bo"||s=="bo2"||s=="bo3"||s=="mw"||s=="mw2"||s=="mw3"||s=="ghosts"||s=="mwr"||s=="h1"||s=="cs2")
            return s;
    }
    return {};
}
bool isModelPath(const std::filesystem::path& path){
    for(const auto& part:path){
        const auto p=lower(part.string());
        if(p=="models"||p=="xmodels"||p=="model")return true;
        if(p=="animations"||p=="anims"||p=="_anims"||p=="animtrees")return false;
    }
    return true;
}
bool intersects(const std::vector<std::string>& a,const std::vector<std::string>& b){for(const auto& left:a)if(std::find(b.begin(),b.end(),left)!=b.end())return true;return false;}

} // namespace

std::string viewhandsGameForLoadedBase(const Catalog& catalog,std::size_t selectedBaseAsset,const std::filesystem::path& loadedBaseModelPath){
    if(selectedBaseAsset<catalog.entries.size()){
        const auto& selected=catalog.entries[selectedBaseAsset];
        if(selected.role==Role::ViewHands)return lower(selected.game);
    }
    const auto loaded=lower(loadedBaseModelPath.lexically_normal().string());
    if(!loaded.empty())for(const auto& asset:catalog.entries)if(asset.role==Role::ViewHands&&lower(asset.path.lexically_normal().string())==loaded)return lower(asset.game);
    return {};
}

std::string coldWarWeaponFamily(std::string_view input){
    auto name=lower(input);
    if(!name.starts_with("wpn_t9_"))return {};
    name.erase(0,7);
    if(name.starts_with("bp_"))name.erase(0,3);
    else if(name.starts_with("loot_"))name.erase(0,5);
    const auto split=name.find('_');if(split==std::string::npos)return {};
    const auto category=name.substr(0,split);
    if(category!="ar"&&category!="smg"&&category!="lmg"&&category!="tr"&&category!="sniper"&&category!="pistol"&&category!="shotgun"&&category!="launcher")return {};
    const auto end=name.find('_',split+1);
    return name.substr(0,end);
}

std::string coldWarWeaponVariant(std::string_view input){
    auto name=lower(input);const auto family=coldWarWeaponFamily(name);if(family.empty())return {};
    name.erase(0,name.find(family)+family.size());
    if(name.ends_with("_lod0"))name.resize(name.size()-5);
    for(const auto marker:{"_view","_world"})if(const auto at=name.find(marker);at!=std::string::npos)name.erase(at,std::char_traits<char>::length(marker));
    if(name.starts_with('_'))name.erase(0,1);
    return name;
}

Role classifyModelName(std::string_view input){const auto name=lower(input);
    // Cold War's native Saluki naming differs from the T6 weapon/hand grammar.
    if(name.starts_with("c_t9_")&&contains(name,"_arms_"))return Role::ViewHands;
    if(name.starts_with("wpn_t9_")){
        const bool view=contains(name,"_view_")||name.ends_with("_view"),world=contains(name,"_world_")||name.ends_with("_world");
        const bool attachment=contains(name,"_scope")||contains(name,"_mag_")||contains(name,"_clip_")||contains(name,"_bullet_casing")||contains(name,"_optic")||contains(name,"_suppressor")||contains(name,"_silencer")||contains(name,"_foregrip")||contains(name,"_attachment");
        if(view)return attachment?Role::ViewAttachment:Role::ViewWeapon;
        if(world)return attachment?Role::WorldAttachment:Role::WorldWeapon;
        return Role::OtherModel;
    }
    if(name.starts_with("fx_")||contains(name,"_fx_")||contains(name,"white_fx")||contains(name,"ambient_fx"))return Role::OtherModel;
    if(name.starts_with("head_mp")||name.starts_with("mp_head")||contains(name,"_head_mp")||name.starts_with("head_")||name.ends_with("_head"))return Role::OtherModel;
    if(name.starts_with("mp_glove")||name.starts_with("mp_eyewear")||name.starts_with("mp_boot")||name.starts_with("mp_exo")||name.starts_with("mp_gear")||name.starts_with("mp_helmet")||name.starts_with("mp_kneepad")||name.starts_with("mp_vest"))return Role::OtherModel;
    if(contains(name,"viewhands")||name.starts_with("viewmodel_default")||name.starts_with("viewmodel_hands")||name.starts_with("vm_view_arms")||name.starts_with("viewhands_h1_"))return Role::ViewHands;
    if(name.starts_with("npc_")||name.starts_with("wm_")||contains(name,"npc_weapon")||contains(name,"_npc_")||contains(name,"_npc")||contains(name,"_wm")){
        const bool isAttachment=contains(name,"_ironsights")||contains(name,"_scope")||contains(name,"_caddy")||contains(name,"_carryrail")||contains(name,"_firerate")||contains(name,"_damage")||contains(name,"_mobility")||contains(name,"_range")||contains(name,"_handling")||contains(name,"_accuracy")||contains(name,"_clip");
        if(isAttachment)return Role::WorldAttachment;
        return Role::WorldWeapon;
    }
    // Reticle models are view attachments
    if(name.starts_with("viewmodel_reticle")||name.starts_with("vm_reticle")||name.starts_with("reticle_")||contains(name,"_reticle"))return Role::ViewAttachment;
    if(name.starts_with("attach_h1_")||name.starts_with("attach_")){
        if(contains(name,"_vm"))return Role::ViewAttachment;
        if(contains(name,"_npc")||contains(name,"_wm"))return Role::WorldAttachment;
    }
    if(name.starts_with("wpn_h1_")||name.starts_with("wpn_")){
        if(contains(name,"_vm"))return Role::ViewWeapon;
        if(contains(name,"_npc")||contains(name,"_wm"))return Role::WorldWeapon;
    }
    // CS2 / Source 2 model naming grammar
    if(name.ends_with("_model")||contains(name,"_model")){
        if(contains(name,"viewhands")||contains(name,"arms")||contains(name,"glove"))return Role::ViewHands;
        if(contains(name,"attachment")||contains(name,"scope")||contains(name,"silencer_attach")||contains(name,"mag_attach"))return Role::ViewAttachment;
        return Role::ViewWeapon;
    }
    if(name.starts_with("knife_")&&!contains(name,"_view")&&!contains(name,"_world"))return Role::ViewWeapon;
    // IW7 exports use a suffix grammar instead of the older viewmodel_ /
    // weapon_ split. The *_camo or base file is the selectable weapon, and the
    // numbered complex, barrel, stock, mag, and sight files are its modular pieces.
    if(name.starts_with("attachment_")){if(contains(name,"_vm"))return Role::ViewAttachment;if(contains(name,"_wm"))return Role::WorldAttachment;}
    if(name.starts_with("weapon_")&&contains(name,"_vm")){
        if(contains(name,"_complex_")||contains(name,"_scope_")||contains(name,"_silencer_")||contains(name,"_foregrip_")||contains(name,"_barrel_")||contains(name,"_stock_")||contains(name,"_mag_")||contains(name,"_sight_"))return Role::ViewAttachment;
        const bool isBase=contains(name,"_camo")||contains(name,"_base_")||name.ends_with("_base")||name.ends_with("_vm")||name.ends_with("_vm_lod0")||contains(name,"_vm_fallback");
        return isBase?Role::ViewWeapon:Role::ViewAttachment;
    }
    if(name.starts_with("weapon_")&&contains(name,"_wm")){if(contains(name,"_complex_")||contains(name,"_scope_")||contains(name,"_silencer_")||contains(name,"_foregrip_")||contains(name,"_barrel_")||contains(name,"_stock_")||contains(name,"_mag_")||contains(name,"_sight_"))return Role::WorldAttachment;return Role::WorldWeapon;}
    const bool iwViewAttachment=name.starts_with("viewmodel_")&&(contains(name,"_scope")||contains(name,"_silencer")||contains(name,"_foregrip")||contains(name,"_grip")||contains(name,"_m203")||contains(name,"_m320")||contains(name,"_gl")||contains(name,"thermal_scope")||name.starts_with("viewmodel_acog")||name.starts_with("viewmodel_eotech")||name.starts_with("viewmodel_reflex")||name.starts_with("viewmodel_magnifier")||name.starts_with("viewmodel_reticle")||contains(name,"_optic")||contains(name,"_sight"));
    const bool iwWorldAttachment=name.starts_with("weapon_")&&(contains(name,"_scope")||contains(name,"_silencer")||contains(name,"_foregrip")||contains(name,"_grip")||contains(name,"_clip")||contains(name,"_m203")||contains(name,"_m320")||contains(name,"_gl")||contains(name,"thermal_scope")||name.starts_with("weapon_acog")||name.starts_with("weapon_eotech")||name.starts_with("weapon_reflex")||name.starts_with("weapon_magnifier")||contains(name,"_optic")||contains(name,"_sight"));
    if(iwViewAttachment)return Role::ViewAttachment;if(iwWorldAttachment)return Role::WorldAttachment;
    // S1/AW exports modular first-person assets as vm_<family>_<variant>.
    // Keep shared optics and muzzle furniture out of the weapon picker, but
    // classify the family variants as weapons so their vm_<family> animation
    // set can be discovered dynamically.
    if(name.starts_with("vm_")){
        const bool attachment=contains(name,"vm_optics_")||contains(name,"vm_silencer_")||contains(name,"vm_foregrip")||contains(name,"vm_lasersight")||contains(name,"vm_damage_mod_")||contains(name,"_scope")||contains(name,"_reticle")||contains(name,"_optic")||contains(name,"_sight");
        return attachment?Role::ViewAttachment:Role::ViewWeapon;
    }
    if(name.starts_with("viewmodel_"))return Role::ViewWeapon;
    if(contains(name,"_viewmodel"))return Role::ViewWeapon;
    if(name.starts_with("worldmodel_")||contains(name,"_worldmodel"))return Role::WorldWeapon;
    if(name.starts_with("t6_attach_")||name.starts_with("t5_attach_")||contains(name,"_attach_")){
        if(contains(name,"_view"))return Role::ViewAttachment;if(contains(name,"_world"))return Role::WorldAttachment;return Role::OtherModel;}
    if((name.starts_with("t6_wpn_")||name.starts_with("t5_wpn_"))&&(contains(name,"_scope_")||contains(name,"_grip_"))){
        if(contains(name,"_view"))return Role::ViewAttachment;if(contains(name,"_world"))return Role::WorldAttachment;return Role::OtherModel;}
    if(name.starts_with("t6_wpn_")||name.starts_with("t5_wpn_")||contains(name,"_weapon_")){
        if(contains(name,"_view"))return Role::ViewWeapon;if(contains(name,"_world"))return Role::WorldWeapon;return Role::OtherModel;}
    if(name.starts_with("c_")&&contains(name,"_fb"))return Role::PlayerModel;
    if(name.starts_with("body_mp_")||name.starts_with("mp_body")||name.starts_with("mp_fullbody")||(name.starts_with("mp_")&&contains(name,"_body"))||
       name.starts_with("body_arab_")||name.starts_with("body_sas_")||name.starts_with("body_spetsnaz_")||name.starts_with("body_usmc_")||
       (name.starts_with("body_")&&(contains(name,"_mp")||contains(name,"woodland")||contains(name,"desert")||contains(name,"urban")||contains(name,"assault")||contains(name,"camo")))||
       name.starts_with("mp_marine")||name.starts_with("mp_atlas")||name.starts_with("mp_sentinel")||name.starts_with("mp_kva")||name.starts_with("fso_")||contains(name,"_fso_")||name.starts_with("mp_torso")||name.starts_with("mp_pants"))return Role::PlayerModel;
    if(name.starts_with("weapon_"))return Role::WorldWeapon;
    return Role::OtherModel;
}

std::string categoryFromPath(const std::filesystem::path& path){
    bool inWeapons=false;
    for(auto it=path.begin();it!=path.end();++it){
        const auto part=lower(it->string());
        if(part=="weapons"){inWeapons=true;continue;}
        if(inWeapons&&(part=="view"||part=="world")){
            auto next=std::next(it);
            if(next!=path.end()){
                auto afterNext=std::next(next);
                if(afterNext!=path.end())return lower(next->string());
            }
        }
    }
    return {};
}

Role classifyModelPath(const std::filesystem::path& path,std::string_view stem){
    // Temporary CODM hand names require both exporter provenance and real arm
    // joints. Never classify all special_* assets as hands.
    if(lower(stem).starts_with("special_")){
        auto sidecar=path;sidecar.replace_extension(".json");std::error_code ec;const auto size=std::filesystem::file_size(sidecar,ec);
        if(!ec&&size<64u*1024u*1024u)try{
            std::ifstream in(sidecar);auto meta=scene::codm::parseJson(std::string(std::istreambuf_iterator<char>(in),{}));
            if(meta.contains("t6Compatible")&&meta["t6Compatible"]==false&&meta.contains("source")&&meta["source"].is_object()&&meta["source"].contains("category")&&meta["source"]["category"]=="Viewhands"){
                const auto doc=cast::Document::load(path);std::unordered_set<std::string> names;
                const auto visit=[&](auto&& self,const cast::Node& node)->void{if(cast::nodeTypeName(node.identifier)=="Bone")if(const auto* p=node.findProperty("n");p&&p->stringValue)names.insert(*p->stringValue);for(const auto& child:node.children)self(self,child);};
                if(doc.valid())for(const auto& root:doc.roots())visit(visit,root);
                if(names.contains("b_LeftArm")&&names.contains("b_RightArm")&&names.contains("b_LeftHand")&&names.contains("b_RightHand"))return Role::ViewHands;
            }
        }catch(...){/* Missing/invalid metadata is not evidence of a hand rig. */}
    }
    const auto name=lower(stem);
    if(gameFromPath(path)=="pointblank"&&!character::pointBlankIdentity(name).empty())
        return name.starts_with("playermode_")?Role::PlayerModel:Role::ViewHands;
    // IW5 weapon_ exports are world meshes, including model_1887 names
    // that otherwise resemble Source's generic *_model grammar.
    if(gameFromPath(path)=="mw3"&&name.starts_with("weapon_")){
        const bool attachment=contains(name,"_scope")||contains(name,"_silencer")||contains(name,"_clip")||contains(name,"_grip")||contains(name,"_optic")||contains(name,"_sight")||name.starts_with("weapon_acog")||name.starts_with("weapon_eotech")||name.starts_with("weapon_reflex")||contains(name,"_m203")||contains(name,"_m320");
        return attachment?Role::WorldAttachment:Role::WorldWeapon;
    }
    // T9 magazines are exported alongside receivers under weapons/view, not
    // necessarily under attachments. Do not let the generic folder override
    // downgrade the native _mag_ component classification to a weapon.
    if(name.starts_with("wpn_t9_"))return classifyModelName(name);
    if(gameFromPath(path)=="iw_sp"){
        if(name=="weapon_erad_camo_lod0"||name=="weapon_revolver_camo_lod0")return Role::ViewWeapon;
        if(name.starts_with("weapon_erad_complex_")||name.starts_with("weapon_revolver_complex_"))return Role::ViewAttachment;
    }
    if(name.starts_with("fx_")||contains(name,"_fx_")||contains(name,"white_fx")||contains(name,"ambient_fx"))return Role::OtherModel;
    bool underWeapons=false,underView=false,underWorld=false,underViewhands=false,underPlayermodels=false,underAttachments=false;
    for(const auto& part:path){
        const auto p=lower(part.string());
        if(p=="weapons")underWeapons=true;
        else if(underWeapons&&p=="view")underView=true;
        else if(underWeapons&&p=="world")underWorld=true;
        else if(p=="viewhands")underViewhands=true;
        else if(p=="playermodels")underPlayermodels=true;
        else if(p=="attachments")underAttachments=true;
    }
    const auto isAttachmentName=[&](std::string_view n)->bool{
        return contains(n,"_ironsights")||contains(n,"_scope")||contains(n,"_caddy")||
               contains(n,"_carryrail")||contains(n,"_clip")||contains(n,"_silencer")||
               contains(n,"_foregrip")||contains(n,"_grip")||contains(n,"_m203")||
               contains(n,"_m320")||contains(n,"_gl")||contains(n,"thermal_scope")||
               contains(n,"_optic")||contains(n,"_sight")||contains(n,"_reticle")||
               n.starts_with("viewmodel_reticle")||n.starts_with("vm_reticle")||
               n.starts_with("reticle_")||n.starts_with("vm_optics_")||
               n.starts_with("vm_silencer_")||n.starts_with("vm_foregrip")||
               n.starts_with("vm_lasersight")||n.starts_with("vm_damage_mod_")||
               n.starts_with("viewmodel_acog")||n.starts_with("viewmodel_eotech")||
               n.starts_with("viewmodel_reflex")||n.starts_with("viewmodel_magnifier")||
               n.starts_with("weapon_acog")||n.starts_with("weapon_eotech")||
               n.starts_with("weapon_reflex")||n.starts_with("weapon_magnifier")||
               n.starts_with("attach_")||n.starts_with("attachment_")||
               contains(n,"_attach_")||contains(n,"t6_attach_")||contains(n,"t5_attach_");
    };
    if(underViewhands){
        if(name.starts_with("head_")||name.starts_with("mp_head")||contains(name,"_head"))return Role::OtherModel;
        return Role::ViewHands;
    }
    if(underPlayermodels){
        // AW exports partial clothing, equipment and even map props under this
        // directory. Only third-person tops are assembly bases, never pants,
        // first-person sleeves or the legacy full-body vest model.
        if(gameFromPath(path)=="aw")return character::awPart(name)==character::Part::Torso?Role::PlayerModel:Role::OtherModel;
        if(name.starts_with("head_mp")||name.starts_with("mp_head")||contains(name,"_head_mp")||
           name.starts_with("head_")||name.ends_with("_head")||name.starts_with("mp_glove")||
           name.starts_with("mp_eyewear")||name.starts_with("mp_boot")||name.starts_with("mp_gear")||
           name.starts_with("mp_helmet")||name.starts_with("mp_kneepad")||name.starts_with("mp_vest"))
            return Role::OtherModel;
        return Role::PlayerModel;
    }
    if(underWeapons&&underView)return isAttachmentName(name)?Role::ViewAttachment:Role::ViewWeapon;
    if(underWeapons&&underWorld)return isAttachmentName(name)?Role::WorldAttachment:Role::WorldWeapon;
    if(underAttachments){
        const bool view=contains(name,"_vm")||contains(name,"_view")||contains(name,"viewmodel")||contains(name,"vm_");
        return view?Role::ViewAttachment:Role::WorldAttachment;
    }
    return classifyModelName(stem);
}

std::vector<std::string> compatibilityKeys(std::string_view input,Role role){auto values=tokens(input);std::vector<std::string> result;
    const std::unordered_set<std::string> common{"t5","t6","t7","iw3","iw4","iw5","iw6","iw7","s1","s2","cod4","mw2","mw3","ghosts","cs2","model","models","wpn","weapon","viewmodel","vm","va","attach","view","world","lod0","lod1","lod2","lod3","base","standard","standard2","operator1","operator2","pro","pro2","royal","atlas","black","gold","mobility","damage","handling","accuracy","range","fire","rate","default","stnd","op01","op02","npc","wm","brock","bshdwl","bwmrpt","cmdtgr","stagger","autumn","blue","choco","hex","marine","multi","red","snake","snow","winter","arctic","woodland"};
    const std::unordered_set<std::string> attachmentTypes{"silencer","silencer1","silencer2","silencer3","silencer4","fastmag","mag","gl","grip","optic","acog","combo","dualband","mount","holo","ads","mms","rangefinder","reflex","rmr","specter","vzoom","bcpu","dbal","wlp","speedloader"};
    for(auto value:values){if(common.contains(value))continue;
        if((role==Role::WorldAttachment||role==Role::ViewAttachment)&&attachmentTypes.contains(value))continue;
        if(value=="scar")value="scarh";else if(value=="scorpion"||value=="evoskorpion")value="skorpion";
        else if(value=="saiga"){ if(std::find(result.begin(),result.end(),"saiga")==result.end())result.push_back("saiga"); value="saiga12"; }
        else if(value=="saiga12"){ if(std::find(result.begin(),result.end(),"saiga")==result.end())result.push_back("saiga"); }
        else if(value=="mk14"||value=="m14"||value=="m14ebr"||value=="mk14ebr"||value=="m14sd"){ if(std::find(result.begin(),result.end(),"m14")==result.end())result.push_back("m14"); value="m14ebr"; }
        else if(value=="arx"||value=="arx160"||value=="arx_160"){ if(std::find(result.begin(),result.end(),"arx")==result.end())result.push_back("arx"); value="arx160"; }
        else if(value=="honeybadger"||value=="honey_badger")value="honeybadger";
        else if(value=="remingtonr5"||value=="remington_r5"||value=="r5"||value=="r5rgp"||value=="remington_r5rgp"||value=="rm22"||value=="rm_22_ar"||value=="rm_22"){ if(std::find(result.begin(),result.end(),"r5rgp")==result.end())result.push_back("r5rgp"); value="remingtonr5"; }
        else if(value=="remington"||value=="remington700"||value=="r700")value="remington700";
        else if(value=="sc2010")value="sc2010";
        else if(value=="m27"||value=="m27_iar"||value=="m27iar"){ if(std::find(result.begin(),result.end(),"m27")==result.end())result.push_back("m27"); value="m27iar"; }
        else if(value=="cbj"||value=="cbjms"||value=="cbj_ms"){ if(std::find(result.begin(),result.end(),"cbj")==result.end())result.push_back("cbj"); value="cbjms"; }
        else if(value=="dragunovsvu"||value=="dragunov_svu"){ if(std::find(result.begin(),result.end(),"dragunov")==result.end())result.push_back("dragunov"); value="dragunovsvu"; }
        else if(value=="fad"||value=="fads"){ if(std::find(result.begin(),result.end(),"fad")==result.end())result.push_back("fad"); value="fads"; }
        else if(value=="mts"||value=="mts255"||value=="mts_255"){ if(std::find(result.begin(),result.end(),"mts")==result.end())result.push_back("mts"); value="mts255"; }
        else if(value=="panzerfaust3"||value=="panzerfaust"){ if(std::find(result.begin(),result.end(),"panzerfaust")==result.end())result.push_back("panzerfaust"); value="panzerfaust"; }
        else if(value=="ak47"||value=="ak_47"||value=="ak"||value=="ak47sd")value="ak47";
        else if(value=="ak74u"||value=="ak74usd"){ if(std::find(result.begin(),result.end(),"ak74usd")==result.end())result.push_back("ak74usd"); value="ak74u"; }
        else if(value=="g36c"||value=="g36"||value=="g36csd"){ if(std::find(result.begin(),result.end(),"g36c")==result.end())result.push_back("g36c"); value="g36"; }
        else if(value=="g36cm203"||value=="g36m203")value="g36m203";
        else if(value=="m4a4"||value=="m4a1"||value=="m4"||value=="m4sd")value="m4a4";
        else if(value=="g3sd"||value=="g3")value="g3";
        else if(value=="m16sd"||value=="m16")value="m16";
        else if(value=="mp5sd"||value=="mp5")value="mp5";
        else if(value=="miniuzi"||value=="mini_uzi")value="miniuzi";
        else if(value=="saw"||value=="m249saw"||value=="m249")value="m249";
        else if(value=="rpg7"||value=="rpg"){ if(std::find(result.begin(),result.end(),"rpg7")==result.end())result.push_back("rpg7"); value="rpg"; }
        else if(value=="skorpionsd"||value=="skorpion")value="skorpion";
        else if(value=="winchester1200"||value=="winchest1200"||value=="winchester"||value=="w1200"){ if(std::find(result.begin(),result.end(),"winchester")==result.end())result.push_back("winchester"); value="winchester1200"; }
        else if(value=="colt1911"||value=="colt45"||value=="m1911"){ if(std::find(result.begin(),result.end(),"colt45")==result.end())result.push_back("colt45"); value="colt1911"; }
        else if(value=="benelli"||value=="benellim4"||value=="super90"||value=="super_90"){ if(std::find(result.begin(),result.end(),"benellim4")==result.end())result.push_back("benellim4"); value="benelli"; }
        else if(value=="karambit"||value=="knife_karambit")value="karambit";
        else if(value=="butterfly"||value=="knife_butterfly")value="butterfly";
        else if(value=="bayonet"||value=="knife_bayonet")value="bayonet";
        else if(value=="cz805"||value=="cz_805_bren"||value=="bren"||value=="sa805")value="sa805";
        else if(value=="masada")value="acr";
        else if(value=="cheytac")value="intervention";
        else if(value=="rottweil72")value="olympia";
        else if(value=="ithaca"||value=="stakeout")value="ithaca37";
        else if(value=="insas")value="msmc";
        else if(value=="qcw05"||value=="chicom")value="chicom";
        else if(value=="kard"){ if(std::find(result.begin(),result.end(),"kard")==result.end())result.push_back("kard"); value="kap40"; }
        else if(value=="kap40"){ if(std::find(result.begin(),result.end(),"kard")==result.end())result.push_back("kard"); }
        else if(value=="fnp45"){ if(std::find(result.begin(),result.end(),"fnp45")==result.end())result.push_back("fnp45"); value="tac45"; }
        else if(value=="tac45"){ if(std::find(result.begin(),result.end(),"fnp45")==result.end())result.push_back("fnp45"); }
        else if(value=="mk48"){ if(std::find(result.begin(),result.end(),"mk48")==result.end())result.push_back("mk48"); value="mark48"; }
        else if(value=="mark48"){ if(std::find(result.begin(),result.end(),"mk48")==result.end())result.push_back("mk48"); }
        else if(value=="x95l"){ if(std::find(result.begin(),result.end(),"x95l")==result.end())result.push_back("x95l"); value="tavor"; }
        else if(value=="tavor"){ if(std::find(result.begin(),result.end(),"x95l")==result.end())result.push_back("x95l"); }
        else if(value=="ballistic_knife"||value=="ballisticknife"){ if(std::find(result.begin(),result.end(),"ballistic_knife")==result.end())result.push_back("ballistic_knife"); value="ballistic_knife_t6"; }
        else if(value=="crossbow"){ if(std::find(result.begin(),result.end(),"crossbow")==result.end())result.push_back("crossbow"); value="crossbow_t6"; }
        else if(value=="c4"){ if(std::find(result.begin(),result.end(),"c4")==result.end())result.push_back("c4"); value="c4_t6"; }
        else if(value=="pda"){ if(std::find(result.begin(),result.end(),"pda")==result.end())result.push_back("pda"); value="pda_hacker"; }
        else if(value=="shield"||value=="riotshield"){ if(std::find(result.begin(),result.end(),"shield")==result.end())result.push_back("shield"); value="riotshield"; }
        else if(value=="tac_insert"||value=="tacinsert"){ if(std::find(result.begin(),result.end(),"tac_insert")==result.end())result.push_back("tac_insert"); value="tac_insert_t6"; }
        else if(value=="taser_mine"||value=="tasermine"||value=="tazor_spike"){ if(std::find(result.begin(),result.end(),"taser_mine")==result.end())result.push_back("taser_mine"); value="tazor_spike"; }
        else if(value=="beretta93r"||value=="b2023r"||value=="b23r")value="b23r";
        else if(value=="fnfiveseven"||value=="fiveseven"||value=="fn57"){ if(std::find(result.begin(),result.end(),"fn57")==result.end())result.push_back("fn57"); if(std::find(result.begin(),result.end(),"fnfiveseven")==result.end())result.push_back("fnfiveseven"); value="fiveseven"; }
        else if(value=="coltanaconda"||value=="magum"||value=="magnum")value="magnum";
        else if(value=="l96a1"||value=="l96"){ if(std::find(result.begin(),result.end(),"l96")==result.end())result.push_back("l96"); value="l96a1"; }
        else if(value=="m82"||value=="barrett"||value=="barrett50cal"){ if(std::find(result.begin(),result.end(),"barrett")==result.end())result.push_back("barrett"); value="m82"; }
        else if(value=="sa80"||value=="sa80lmg"){ if(std::find(result.begin(),result.end(),"sa80lmg")==result.end())result.push_back("sa80lmg"); value="sa80"; }
        else if(value=="model1887"||value=="1887"){ if(std::find(result.begin(),result.end(),"model1887")==result.end())result.push_back("model1887"); value="1887"; }
        else if(value=="lightstick"||value=="light_stick")value="lightstick";
        else if(value=="m8garandscope"||value=="garand"||value=="m1garand"||value=="m1"){ if(std::find(result.begin(),result.end(),"m1")==result.end())result.push_back("m1"); value="garand"; }
        else if(value=="chainsaw"||value=="kac_chainsaw"||value=="chain_saw")value="chainsaw";
        else if(value=="fabarm_fp6"||value=="fp6")value="fp6";
        else if(value=="maul")value="bulldog";
        else if(value=="g28")value="mr28";
        else if(value=="gm6")value="lynx";
        else if(value=="l115a3")value="l115";
        else if(value=="vbr_pdw"||value=="vbr")value="pdw";
        else if(value=="evopro")value="evo";
        else if(value=="kriss")value="vector";
        else if(value=="crdb")value="karma45";
        else if(value=="chargeshot")value="ebr800";
        else if(value=="devastator")value="mauler";
        else if(value=="dbl50")value="oni";
        else if(value=="e_ak47")value="volk";
        else if(value=="cq300"||value=="cq_300")value="cq300";
        else if(value=="kbr32"||value=="kbar")value="kbar32";
        else if(value=="rprevo")value="rpr";
        else if(value=="spartansa3")value="spartan";
        else if(value=="stingerm7")value="stinger";
        else if(value=="atlas20mm")value="atlas20";
        else if(value=="tar21"||value=="mtarx"||value=="x95l")value="mtar";
        else if(value=="sig556")value="sig556";
        else if(value=="saritch")value="saritch";
        else if(value=="qbb95")value="qbb95";
        else if(value=="srm1216")value="m1216";
        else if(value=="870mcs")value="870mcs";
        else if(value=="ump45")value="ump";
        else if(value=="ak12")value="ak12";
        else if(value=="usr")value="usr";
        if(std::find(result.begin(),result.end(),value)==result.end())result.push_back(value);
    }
    const auto name=lower(input);
    if(contains(name,"mk14")||contains(name,"m14ebr")){
        if(std::find(result.begin(),result.end(),"m14ebr")==result.end())result.push_back("m14ebr");
        if(std::find(result.begin(),result.end(),"mk14")==result.end())result.push_back("mk14");
    }
    if(role==Role::WorldAttachment||role==Role::ViewAttachment){
        if(contains(name,"_attach_gl_")&&std::find(result.begin(),result.end(),"ar")==result.end())result.push_back("ar");
        if(contains(name,"_attach_speedloader")&&std::find(result.begin(),result.end(),"judge")==result.end())result.push_back("judge");
    }
    return result;
}

void Catalog::clear(){root.clear();entries.clear();counts.fill(0);skippedLods=scannedCastFiles=0;}

bool isDefaultColdWarRigPart(std::string_view weaponName,std::string_view partName){
    auto weapon=lower(weaponName),part=lower(partName);
    if(weapon.ends_with("_lod0"))weapon.resize(weapon.size()-5);
    if(part.ends_with("_lod0"))part.resize(part.size()-5);
    if(!weapon.starts_with("wpn_t9_")||!weapon.ends_with("_view"))return false;
    const auto family=weapon.substr(0,weapon.size()-5);
    // Exact family identity excludes duplicate ADS/viewer optics and skins.
    return part==family+"_mag_view"||part==family+"_scope_view";
}

bool isPreferredColdWarRigPart(const Catalog& catalog,const Asset& weapon,const Asset& part){
    if(part.game!=weapon.game||part.role!=Role::ViewAttachment)return false;
    const auto family=coldWarWeaponFamily(weapon.name);
    if(family.empty())return false;
    const auto variant=coldWarWeaponVariant(weapon.name);
    // Exported cosmetics occur on either side of the component token. Match
    // the complete identity; substring matching also admits upgraded extmags,
    // scopes for ADS/viewer passes, and unrelated skins.
    const auto match=[&](std::string_view input){
        auto name=lower(input);if(name.ends_with("_lod0"))name.resize(name.size()-5);
        const auto prefix="wpn_t9_"+family;
        for(int kind=0;kind<2;++kind){const std::string token=kind==0?"mag":"scope";
            if(!variant.empty()&&(name==prefix+"_"+variant+"_"+token+"_view"||name==prefix+"_"+token+"_"+variant+"_view"))return std::pair{kind,0};
            if(name==prefix+"_"+token+"_view")return std::pair{kind,1};
        }
        return std::pair{-1,99};
    };
    const auto selected=match(part.name);if(selected.first<0)return false;
    for(const auto& candidate:catalog.entries){
        if(candidate.game!=weapon.game||candidate.role!=Role::ViewAttachment)continue;
        const auto other=match(candidate.name);
        if(other.first==selected.first&&(other.second<selected.second||
            (other.second==selected.second&&lower(candidate.name)<lower(part.name))))return false;
    }
    return true;
}

bool isDefaultIwRigPart(std::string_view weaponName,std::string_view partName){
    const auto weapon=lower(weaponName),part=lower(partName);
    auto marker=weapon.find("_vm");
    if(marker==std::string::npos&&(weapon.starts_with("weapon_erad_camo")||weapon.starts_with("weapon_revolver_camo")))marker=weapon.find("_camo");
    if(marker==std::string::npos||!part.starts_with(weapon.substr(0,marker)+"_"))return false;
    // Cosmetic tokens such as camo are not weapon identity. Match the complete
    // family prefix, then a complete default-piece token (not complex_10).
    for(const auto* kind:{"_complex_","_mag_","_barrel_","_stock_"}){
        const auto at=part.find(kind);if(at==std::string::npos)continue;
        const auto first=at+std::string_view(kind).size();const auto end=part.find('_',first);
        const auto variant=part.substr(first,end==std::string::npos?end:end-first);
        if(variant=="1"||variant=="01")return true;
    }
    return false;
}

std::vector<std::size_t> Catalog::compatibleAttachments(std::size_t weaponIndex) const{
    std::vector<std::size_t> result;if(weaponIndex>=entries.size())return result;const auto& weapon=entries[weaponIndex];
    const auto attachmentRole=weapon.role==Role::ViewWeapon?Role::ViewAttachment:weapon.role==Role::WorldWeapon?Role::WorldAttachment:Role::Count;
    if(attachmentRole==Role::Count)return result;
    for(std::size_t i=0;i<entries.size();++i){const auto& candidate=entries[i];if(candidate.role!=attachmentRole||candidate.game!=weapon.game)continue;
        if(candidate.compatibilityKeys.empty()||intersects(candidate.compatibilityKeys,weapon.compatibilityKeys))result.push_back(i);}
    return result;
}

bool scan(const std::filesystem::path& scanRoot,Catalog& catalog,std::string& error){
    error.clear();Catalog imported;imported.root=scanRoot;std::error_code iterationError;
    if(!std::filesystem::is_directory(scanRoot,iterationError)){error="Asset-library root is not a directory";return false;}
    // A Saluki game export keeps models, animations, and textures as sibling
    // trees. Cataloging only needs model Casts; walking the much larger baked
    // animation tree caused heavy disk contention even though every file was
    // rejected later by isModelPath(). Preserve direct `models` folder scans.
    auto traversalRoot=scanRoot;const auto modelRoot=scanRoot/"models";
    if(std::filesystem::is_directory(modelRoot,iterationError))traversalRoot=modelRoot;iterationError.clear();
    for(std::filesystem::recursive_directory_iterator it(traversalRoot,std::filesystem::directory_options::skip_permission_denied,iterationError),end;it!=end;it.increment(iterationError)){
        if(iterationError){iterationError.clear();continue;}if(!it->is_regular_file(iterationError)||lower(it->path().extension().string())!=".cast")continue;++imported.scannedCastFiles;
        if(!isModelPath(it->path()))continue;const auto stem=lower(it->path().stem().string());if(lodAboveZero(stem)){++imported.skippedLods;continue;}
        Asset asset;asset.path=it->path();asset.name=it->path().stem().string();asset.game=gameFromPath(it->path());
        asset.category=categoryFromPath(it->path());
        asset.role=classifyModelPath(it->path(),stem);
        asset.compatibilityKeys=compatibilityKeys(stem,asset.role);
        ++imported.counts[static_cast<std::size_t>(asset.role)];imported.entries.push_back(std::move(asset));
    }
    std::sort(imported.entries.begin(),imported.entries.end(),[](const Asset& a,const Asset& b){if(a.game!=b.game)return a.game<b.game;if(a.role!=b.role)return a.role<b.role;return a.name<b.name;});
    catalog=std::move(imported);return true;
}

bool appendScan(const std::filesystem::path& scanRoot,std::string_view explicitGame,Catalog& catalog,std::string& error){
    error.clear();std::error_code iterationError;
    if(!std::filesystem::is_directory(scanRoot,iterationError)){error="Directory not found: "+scanRoot.string();return false;}
    auto traversalRoot=scanRoot;const auto modelRoot=scanRoot/"models";
    if(std::filesystem::is_directory(modelRoot,iterationError))traversalRoot=modelRoot;iterationError.clear();
    const auto xmodelRoot=scanRoot/"xmodels";
    if(std::filesystem::is_directory(xmodelRoot,iterationError))traversalRoot=xmodelRoot;iterationError.clear();
    std::unordered_set<std::string> existingPaths;
    for(const auto& entry:catalog.entries)existingPaths.insert(entry.path.lexically_normal().string());
    std::string defaultGame=explicitGame.empty()?scanRoot.stem().string():std::string(explicitGame);
    if(lower(defaultGame)=="exported_files"||lower(defaultGame)=="saluki"||lower(defaultGame)=="greyhound")defaultGame.clear();
    std::vector<Asset> newAssets;
    for(std::filesystem::recursive_directory_iterator it(traversalRoot,std::filesystem::directory_options::skip_permission_denied,iterationError),end;it!=end;it.increment(iterationError)){
        if(iterationError){iterationError.clear();continue;}if(!it->is_regular_file(iterationError)||lower(it->path().extension().string())!=".cast")continue;++catalog.scannedCastFiles;
        if(!isModelPath(it->path()))continue;const auto stem=lower(it->path().stem().string());if(lodAboveZero(stem)){++catalog.skippedLods;continue;}
        const auto normalPath=it->path().lexically_normal().string();
        if(existingPaths.contains(normalPath))continue;
        existingPaths.insert(normalPath);
        Asset asset;asset.path=it->path();asset.name=it->path().stem().string();
        auto detectedGame=gameFromPath(it->path());
        asset.game=detectedGame.empty()?defaultGame:detectedGame;
        asset.category=categoryFromPath(it->path());
        asset.role=classifyModelPath(it->path(),stem);
        asset.compatibilityKeys=compatibilityKeys(stem,asset.role);
        newAssets.push_back(std::move(asset));
    }
    for(auto& asset:newAssets){
        ++catalog.counts[static_cast<std::size_t>(asset.role)];
        catalog.entries.push_back(std::move(asset));
    }
    std::sort(catalog.entries.begin(),catalog.entries.end(),[](const Asset& a,const Asset& b){if(a.game!=b.game)return a.game<b.game;if(a.role!=b.role)return a.role<b.role;return a.name<b.name;});
    return true;
}

const char* roleName(Role role){switch(role){case Role::PlayerModel:return "Player models";case Role::ViewHands:return "Viewmodels / hands";case Role::WorldWeapon:return "World weapons";case Role::ViewWeapon:return "Viewmodel weapons";case Role::WorldAttachment:return "World attachments";case Role::ViewAttachment:return "Viewmodel attachments";case Role::OtherModel:return "Other models";default:return "Unknown";}}

} // namespace assets
