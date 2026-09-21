#include "scene/PoseEvaluationScratch.h"
#include "scene/CastScene.h"
#include "scene/PointBlankNative.h"
#include "scene/T6Magazine.h"
#include "scene/T6SharedTextures.h"
#include "scene/NativeBoneNames.h"
#include "scene/ColdWarViewmodel.h"
#include "scene/ColdWarTextures.h"
#include "scene/ColdWarWorld.h"
#include <mutex>
#include "scene/CodmNative.h"
#include "scene/CodmWorldBody.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>

namespace scene {

bool isEmissiveMaterialIdentity(std::string_view value) {
    std::string canonical(value);
    std::transform(canonical.begin(),canonical.end(),canonical.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(canonical.find("tritium")!=std::string::npos||canonical.find("reticle")!=std::string::npos||canonical.find("emissive")!=std::string::npos||canonical.find("glow")!=std::string::npos)return true;
    std::string token;
    for(const unsigned char c:canonical){
        if(std::isalnum(c))token.push_back(static_cast<char>(c));
        else {if(token=="glo")return true;token.clear();}
    }
    return token=="glo";
}

namespace {

constexpr std::uint32_t kModel = 0x6C646F6D;
constexpr std::uint32_t kMesh = 0x6873656D;
constexpr std::uint32_t kSkeleton = 0x6C656B73;
constexpr std::uint32_t kBone = 0x656E6F62;
constexpr std::uint32_t kAnimation = 0x6D696E61;
constexpr std::uint32_t kCurve = 0x76727563;
constexpr std::uint32_t kNotification = 0x6669746E;
constexpr std::uint32_t kMaterial = 0x6C74616D;
constexpr std::uint32_t kColor = 0x726C6F63;
constexpr std::uint32_t kFile = 0x656C6966;

bool isOpticRoot(std::string_view name){
    return name.starts_with("tag_reflex")||name.starts_with("tag_eotech")||name.starts_with("tag_acog")||name.starts_with("tag_mms")||name.starts_with("tag_rmr")||name.starts_with("tag_holo")||name.starts_with("tag_scope")||
        name.starts_with("tag_rangefinder")||name.starts_with("tag_vzoom")||name.starts_with("tag_ir")||name.starts_with("tag_combo")||name.starts_with("tag_thermal_scope");
}

} // namespace

std::string canonicalName(std::string value) {
    const auto separator=value.find_last_of("|:");
    if(separator!=std::string::npos)value=value.substr(separator+1);
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return resolveNativeBoneName(std::move(value));
}

std::optional<std::size_t> rigSocketForRoot(const Skeleton& skeleton,const std::string& rootName) {
    if(const auto exact=skeleton.boneByCanonicalName.find(rootName);exact!=skeleton.boneByCanonicalName.end())return exact->second;
    const auto socketName=[&]() -> std::string_view {
        if(rootName=="j_gun1"&&skeleton.boneByCanonicalName.contains("tag_knife_attach"))return "tag_knife_attach";
        if(rootName=="j_gun"||rootName=="j_gun1"||rootName=="wpn"||rootName=="weapon"||rootName=="weapon_offset"||rootName=="root"||rootName=="root9"||rootName=="root_motion"||rootName=="bone"||rootName=="wpnpivot"){
            if(skeleton.boneByCanonicalName.contains("tag_weapon"))return "tag_weapon";
            if(skeleton.boneByCanonicalName.contains("tag_weapon_right"))return "tag_weapon_right";
            if(skeleton.boneByCanonicalName.contains("wpn"))return "wpn";
            if(skeleton.boneByCanonicalName.contains("j_gun"))return "j_gun";
            if(skeleton.boneByCanonicalName.contains("tag_origin"))return "tag_origin";
            if(skeleton.boneByCanonicalName.contains("j_wrist_ri"))return "j_wrist_ri";
            if(skeleton.boneByCanonicalName.contains("j_wrist_r"))return "j_wrist_r";
            if(skeleton.boneByCanonicalName.contains("hand_r"))return "hand_r";
            return "tag_weapon";
        }
        if(rootName=="tag_silencer")return "tag_flash";
        if(isOpticRoot(rootName)){
            if(skeleton.boneByCanonicalName.contains("tag_scope_rail"))return "tag_scope_rail";
            if(skeleton.boneByCanonicalName.contains("tag_rail"))return "tag_rail";
            if(skeleton.boneByCanonicalName.contains("tag_acog_2"))return "tag_acog_2";
            if(skeleton.boneByCanonicalName.contains("tag_acog"))return "tag_acog";
            if(skeleton.boneByCanonicalName.contains("tag_scope"))return "tag_scope";
            if(skeleton.boneByCanonicalName.contains("tag_red_dot"))return "tag_red_dot";
            if(skeleton.boneByCanonicalName.contains("tag_reflex"))return "tag_reflex";
            if(skeleton.boneByCanonicalName.contains("tag_eotech"))return "tag_eotech";
            // tag_sights is the front iron sight on several BO2 view weapons.
            // Keep optics on the gun and derive their receiver mount below.
            return skeleton.boneByCanonicalName.contains("tag_sights_front")?"tag_sights":"j_gun";
        }
        // Some BO2 view weapons (HK416, Ballista) deliberately omit a bind-pose
        // clip socket. Their animation creates tag_clip at the gun origin and
        // supplies its local motion, so it still belongs below the gun root.
        if(rootName=="tag_clip"||rootName=="tag_clip1")return "j_gun";
        return {};
    }();
    if(socketName.empty())return std::nullopt;
    const auto found=skeleton.boneByCanonicalName.find(std::string(socketName));
    return found==skeleton.boneByCanonicalName.end()?std::nullopt:std::optional<std::size_t>{found->second};
}

void installCrossGenerationViewmodelAliases(Skeleton& skeleton){
    const auto alias=[&](std::string_view alternate,std::string_view actual){
        const auto found=skeleton.boneByCanonicalName.find(canonicalName(std::string(actual)));
        if(found!=skeleton.boneByCanonicalName.end()){
            skeleton.boneByName.try_emplace(std::string(alternate),found->second);
            skeleton.boneByCanonicalName.try_emplace(canonicalName(std::string(alternate)),found->second);
        }
    };
    // CS2 root & weapon aliases
    const bool isCs2Skeleton = skeleton.boneByCanonicalName.contains("wpn") && !skeleton.boneByCanonicalName.contains("tag_weapon");
    if(isCs2Skeleton){
        alias("root_motion","tag_origin");alias("tag_origin","root_motion");
        alias("root","tag_origin");alias("tag_view","root_motion");
        alias("wpn","tag_weapon");
        alias("wpn","tag_weapon_right");
        alias("tag_weapon","wpn");
        alias("tag_weapon_right","wpn");
        alias("wpnhand_l","tag_weapon_left");alias("tag_weapon_left","wpnhand_l");
        alias("wpnhand_r","tag_weapon_right");alias("tag_weapon_right","wpnhand_r");

        alias("armUpperShoulder_L","j_shoulder_le");alias("j_shoulder_le","armUpperShoulder_L");
        alias("arm_upper_L","j_shoulder_le");
        alias("armUpperShoulder_R","j_shoulder_ri");alias("j_shoulder_ri","armUpperShoulder_R");
        alias("arm_upper_R","j_shoulder_ri");
        alias("arm_lower_L","j_elbow_le");alias("j_elbow_le","arm_lower_L");
        alias("arm_lower_R","j_elbow_ri");alias("j_elbow_ri","arm_lower_R");
        alias("hand_L","j_wrist_le");alias("j_wrist_le","hand_L");
        alias("hand_R","j_wrist_ri");alias("j_wrist_ri","hand_R");
        alias("clavicle_L","j_clavicle_le");alias("j_clavicle_le","clavicle_L");
        alias("clavicle_R","j_clavicle_ri");alias("j_clavicle_ri","clavicle_R");
    }

    alias("bolt","j_bolt");alias("j_bolt","bolt");
    alias("slide","j_slide");alias("j_slide","slide");
    alias("clip","tag_clip");alias("tag_clip","clip");alias("mag","tag_clip");
    alias("trigger","j_trigger");alias("j_trigger","trigger");

    for(const auto side:{std::string_view{"le"},std::string_view{"ri"}}){
        const auto suffix=std::string{"_"}+std::string(side);
        alias("j_metaindex"+suffix+"_1","j_index"+suffix+"_0");alias("j_index"+suffix+"_0","j_metaindex"+suffix+"_1");
        alias("j_metamid"+suffix+"_1","j_mid"+suffix+"_0");alias("j_mid"+suffix+"_0","j_metamid"+suffix+"_1");
        alias("j_metaring"+suffix+"_1","j_ringpalm"+suffix);alias("j_ringpalm"+suffix,"j_metaring"+suffix+"_1");
        alias("j_metapinky"+suffix+"_1","j_pinkypalm"+suffix);alias("j_pinkypalm"+suffix,"j_metapinky"+suffix+"_1");
        alias("j_wristfronttwist1"+suffix,"j_wristtwist"+suffix);alias("j_wristtwist"+suffix,"j_wristfronttwist1"+suffix);
    }
}

void installCrossGenerationPlayermodelAliases(Skeleton& skeleton){
    const auto alias=[&](std::string_view alternate,std::string_view actual){
        const auto found=skeleton.boneByCanonicalName.find(canonicalName(std::string(actual)));
        if(found!=skeleton.boneByCanonicalName.end()){
            skeleton.boneByName.try_emplace(std::string(alternate),found->second);
            skeleton.boneByCanonicalName.try_emplace(canonicalName(std::string(alternate)),found->second);
        }
    };
    // Playermodel bone aliases across BO2/MW/Ghosts/IW/AW/T7
    alias("pelvis","j_mainroot"); alias("j_mainroot","pelvis");
    alias("back_low","j_spinelower"); alias("j_spinelower","back_low");
    alias("back_mid","j_spineupper"); alias("j_spineupper","back_mid");
    alias("back_up","j_spine4"); alias("j_spine4","back_up");
    alias("neck","j_neck"); alias("j_neck","neck");
    alias("head","j_head"); alias("j_head","head");
    alias("shoulder","j_shoulder_ri"); alias("j_shoulder_ri","shoulder");
    alias("tag_torso","j_spinelower"); alias("j_spinelower","tag_torso");
    alias("root_motion","tag_origin"); alias("tag_origin","root_motion");
    alias("root","tag_origin"); alias("tag_origin","root");

    for(const auto side:{std::string_view{"le"},std::string_view{"ri"}}){
        const auto suffix=std::string{"_"}+std::string(side);
        alias("clavicle"+suffix,"j_clavicle"+suffix); alias("j_clavicle"+suffix,"clavicle"+suffix);
        alias("shoulder"+suffix,"j_shoulder"+suffix); alias("j_shoulder"+suffix,"shoulder"+suffix);
        alias("elbow"+suffix,"j_elbow"+suffix); alias("j_elbow"+suffix,"elbow"+suffix);
        alias("wrist"+suffix,"j_wrist"+suffix); alias("j_wrist"+suffix,"wrist"+suffix);
        alias("hip"+suffix,"j_hip"+suffix); alias("j_hip"+suffix,"hip"+suffix);
        alias("knee"+suffix,"j_knee"+suffix); alias("j_knee"+suffix,"knee"+suffix);
        alias("ankle"+suffix,"j_ankle"+suffix); alias("j_ankle"+suffix,"ankle"+suffix);
        alias("ball"+suffix,"j_ball"+suffix); alias("j_ball"+suffix,"ball"+suffix);
        alias("hiptwist"+suffix,"j_hiptwist"+suffix); alias("j_hiptwist"+suffix,"hiptwist"+suffix);
        alias("hip_twist"+suffix,"j_hiptwist"+suffix); alias("j_hiptwist"+suffix,"hip_twist"+suffix);
    }
}

bool isViewmodelSkeleton(const Skeleton& skeleton){
    return skeleton.boneByCanonicalName.contains("tag_view")||skeleton.boneByCanonicalName.contains("tag_weapon")||skeleton.boneByCanonicalName.contains("tag_weapon_right")||skeleton.boneByCanonicalName.contains("wpn")||skeleton.boneByCanonicalName.contains("j_gun");
}

namespace {

bool containsAny(const std::string& value,std::initializer_list<std::string_view> needles){
    return std::any_of(needles.begin(),needles.end(),[&](auto needle){return value.find(needle)!=std::string::npos;});
}

std::vector<std::string> nameTokens(const std::string& name){
    std::vector<std::string> tokens;std::string token;
    for(const unsigned char c:name){
        if(std::isalnum(c))token.push_back(static_cast<char>(std::tolower(c)));
        else if(!token.empty()){tokens.push_back(std::move(token));token.clear();}
    }
    if(!token.empty())tokens.push_back(std::move(token));
    return tokens;
}

bool tokenIs(const std::vector<std::string>& tokens,std::initializer_list<std::string_view> values){
    return std::any_of(tokens.begin(),tokens.end(),[&](const auto& token){
        return std::any_of(values.begin(),values.end(),[&](auto value){return token==value;});
    });
}

bool tokenContains(const std::vector<std::string>& tokens,std::initializer_list<std::string_view> values){
    return std::any_of(tokens.begin(),tokens.end(),[&](const auto& token){
        return std::any_of(values.begin(),values.end(),[&](auto value){return token.find(value)!=std::string::npos;});
    });
}

std::optional<int> angularToken(const std::vector<std::string>& tokens,std::string_view word,char abbreviation){
    for(const auto& token:tokens){
        const auto position=token.find(word);
        std::size_t end=position;
        if(position==std::string::npos&&token.size()>1&&token.back()==abbreviation)end=token.size()-1;
        if(end==std::string::npos||end==0)continue;
        std::size_t begin=end;while(begin>0&&std::isdigit(static_cast<unsigned char>(token[begin-1])))--begin;
        if(begin<end)return std::stoi(token.substr(begin,end-begin));
    }
    return std::nullopt;
}

std::string text(const cast::Node& node, std::string_view name, std::string fallback = {}) {
    const auto* property = node.findProperty(name);
    return property && property->stringValue ? *property->stringValue : std::move(fallback);
}

std::vector<float> floats(const cast::Document& document, const cast::Node& node, std::string_view name) {
    const auto* property = node.findProperty(name);
    return property ? document.floatValues(*property) : std::vector<float>{};
}

std::vector<std::uint64_t> uints(const cast::Document& document, const cast::Node& node, std::string_view name) {
    const auto* property = node.findProperty(name);
    return property ? document.unsignedValues(*property) : std::vector<std::uint64_t>{};
}

float scalar(const cast::Document& document, const cast::Node& node, std::string_view name, float fallback) {
    const auto values = floats(document, node, name);
    return values.empty() ? fallback : values[0];
}

Vec3 vec3(const cast::Document& document, const cast::Node& node, std::string_view name, Vec3 fallback) {
    const auto v = floats(document, node, name);
    return v.size() >= 3 ? Vec3{v[0],v[1],v[2]} : fallback;
}

Quat quat(const cast::Document& document, const cast::Node& node, std::string_view name, Quat fallback = {}) {
    const auto v = floats(document, node, name);
    return v.size() >= 4 ? normalize(Quat{v[0],v[1],v[2],v[3]}) : fallback;
}

Mat4 nodeTransform(const cast::Document& document, const cast::Node& node) {
    return trs(vec3(document,node,"p",{}), quat(document,node,"r"), vec3(document,node,"s",{1,1,1}));
}

const cast::Node* childOf(const cast::Node& node, std::uint32_t type) {
    const auto it = std::find_if(node.children.begin(), node.children.end(), [type](const cast::Node& n){ return n.identifier==type; });
    return it == node.children.end() ? nullptr : &*it;
}

const cast::Node* childHash(const cast::Node& node, std::uint64_t hash) {
    const auto it = std::find_if(node.children.begin(), node.children.end(), [hash](const cast::Node& n){ return n.hash==hash; });
    return it == node.children.end() ? nullptr : &*it;
}

Skeleton buildSkeleton(const cast::Document& document, const cast::Node& skeletonNode, std::vector<std::string>& warnings) {
    Skeleton skeleton;
    std::vector<const cast::Node*> sources;
    for (const auto& node : skeletonNode.children) {
        if (node.identifier != kBone) continue;
        Bone bone;
        bone.name = text(node, "n", "bone_" + std::to_string(skeleton.bones.size()));
        const auto parent = uints(document,node,"p");
        bone.parent = parent.empty() ? -1 : static_cast<std::int32_t>(static_cast<std::uint32_t>(parent[0]));
        bone.restLocal.position = vec3(document,node,"lp",{});
        bone.restLocal.rotation = quat(document,node,"lr");
        bone.restLocal.scale = vec3(document,node,"s",{1,1,1});
        skeleton.boneByName[bone.name] = skeleton.bones.size();
        skeleton.boneByCanonicalName.try_emplace(canonicalName(bone.name),skeleton.bones.size());
        skeleton.bones.push_back(std::move(bone));
        sources.push_back(&node);
    }
    for (std::size_t i=0;i<skeleton.bones.size();++i) {
        auto& bone=skeleton.bones[i];
        if (bone.parent >= static_cast<std::int32_t>(i) || bone.parent < -1) {
            warnings.push_back("Bone '"+bone.name+"' has an invalid or forward parent index; treating it as a root");
            bone.parent=-1;
        }
        const auto& source=*sources[i];
        const auto local=trs(bone.restLocal.position,bone.restLocal.rotation,bone.restLocal.scale);
        const auto wp=floats(document,source,"wp"), wr=floats(document,source,"wr");
        if (wp.size()>=3 && wr.size()>=4) {
            bone.restGlobal=trs({wp[0],wp[1],wp[2]},normalize(Quat{wr[0],wr[1],wr[2],wr[3]}),bone.restLocal.scale);
            const auto parentGlobal=bone.parent>=0?skeleton.bones[bone.parent].restGlobal:Mat4::identity();
            const auto derived=inverseAffine(parentGlobal)*bone.restGlobal;
            Vec3 derivedPosition{},derivedScale{};Quat derivedRotation{};
            decomposeAffine(derived,derivedPosition,derivedRotation,derivedScale);
            if (!source.findProperty("lp")) bone.restLocal.position=derivedPosition;
            if (!source.findProperty("lr")) bone.restLocal.rotation=derivedRotation;
            if (!source.findProperty("s")) bone.restLocal.scale=derivedScale;
        } else bone.restGlobal=bone.parent>=0?skeleton.bones[bone.parent].restGlobal*local:local;
        bone.inverseBind=inverseAffine(bone.restGlobal);
    }
    installCrossGenerationViewmodelAliases(skeleton);
    installCrossGenerationPlayermodelAliases(skeleton);
    return skeleton;
}

struct Surface { Vec4 color{0.52f,0.62f,0.72f,1}; std::filesystem::path albedo,normal,specular,metalness,roughness; std::string materialName; bool camoBlend{},camoUseAlpha{true},lens{},eyeOverlay{},emissive{},forceAlpha{},ignoreAlbedoAlpha{},excluded{}; };

Surface materialSurface(const cast::Document& document, const cast::Node& model, const cast::Node& mesh) {
    const auto refs=uints(document,mesh,"m");
    if (refs.empty()) return {};
const auto* material=childHash(model,refs[0]);
    if (!material || material->identifier!=kMaterial) return {};
    const auto materialName=text(*material,"n");const auto canonicalMaterial=canonicalName(materialName),sourceLower=canonicalName(document.sourceName());
    auto normalizedSource=document.sourceName();std::transform(normalizedSource.begin(),normalizedSource.end(),normalizedSource.begin(),[](unsigned char c){return c=='\\'?'/' : static_cast<char>(std::tolower(c));});
    const bool awViewWeapon=containsAny(sourceLower,{"/aw/models/vm_","\\aw\\models\\vm_"})&&!containsAny(sourceLower,{"vm_view_arms","viewhands"});
    const bool source2Weapon=normalizedSource.find("/cs2/models/weapons/")!=std::string::npos;
    const bool coldWarWeapon=std::filesystem::path(document.sourceName()).filename().string().starts_with("wpn_t9_");
    const bool weaponSource=source2Weapon||(containsAny(sourceLower,{"wpn_","weapon_","viewmodel"})||awViewWeapon)&&!containsAny(sourceLower,{"viewhands","view_hands"});
    Surface result;result.materialName=materialName;result.eyeOverlay=containsAny(canonicalMaterial,{"cornea","shiny_lense_eye","shiny_lens_eye","eye_overlay","eye_gloss","eye_wet","eye_clearcoat"});result.lens=containsAny(canonicalMaterial,{"lens","lense","glass"});result.emissive=isEmissiveMaterialIdentity(canonicalMaterial);result.forceAlpha=result.lens||result.emissive||result.eyeOverlay;result.ignoreAlbedoAlpha=source2Weapon&&!result.forceAlpha;result.excluded=weaponSource&&containsAny(canonicalMaterial,{"clan_tag","clantag","player_icon","emblem"});result.camoBlend=weaponSource&&!result.forceAlpha&&!result.excluded;result.camoUseAlpha=!source2Weapon;
    const auto resolveFile=[&](std::filesystem::path raw)->std::filesystem::path{
        if(raw.empty())return {};
        const auto modelFolder=std::filesystem::path(document.sourceName()).parent_path();
        auto direct=raw.is_relative()?modelFolder/raw:raw;if(coldWarWeapon)direct=coldwar_textures::accessible(direct);if(std::filesystem::exists(direct))return direct.lexically_normal();
        // Saluki sometimes stores only the image asset name in normal/specular
        // slots (notably DSR50) while the PNG lives in _images/mc/<material>/.
        auto folder=materialName;if(folder.rfind("mc_",0)==0)folder.erase(0,3);
        auto file=raw.filename();if(file.extension().empty())file.replace_extension(".png");
        auto exported=modelFolder/"_images"/"mc"/folder/file;
        if(std::filesystem::exists(exported))return exported.lexically_normal();
        auto assetFolder=raw.filename().stem().string();for(const std::string suffix:{"_nml","_normal"})if(assetFolder.ends_with(suffix)){assetFolder.erase(assetFolder.size()-suffix.size());break;}
        exported=modelFolder/"_images"/"mc"/assetFolder/file;if(std::filesystem::exists(exported))return exported.lexically_normal();
        // Preserve the original resolved reference for diagnostics and for
        // documents whose asset files are mounted after parsing.
        const auto shared=t6_shared_textures::resolve(std::filesystem::path(document.sourceName()),direct.lexically_normal());
        return coldWarWeapon?coldwar_textures::fallback(std::filesystem::path(document.sourceName()),materialName,shared):shared;
    };
    const auto mapPath=[&](const char* slot)->std::filesystem::path{const auto surfaceRefs=uints(document,*material,slot);if(surfaceRefs.empty())return {};const auto* value=childHash(*material,surfaceRefs[0]);if(!value||value->identifier!=kFile)return {};return resolveFile(std::filesystem::path(text(*value,"p")));};
    result.normal=mapPath("normal");result.specular=mapPath("specular");
    result.metalness=mapPath("metalness");result.roughness=mapPath("roughness");
    if(source2Weapon){
        // Source 2 material paths are virtual .vmat names; Saluki writes their
        // sibling maps under _images/<material stem>, not COD's _images/mc.
        const auto images=std::filesystem::path(document.sourceName()).parent_path()/"_images"/std::filesystem::path(materialName).stem();
        std::vector<std::filesystem::path> files;std::error_code ec;
        for(std::filesystem::directory_iterator it(images,ec),end;it!=end;it.increment(ec)){if(ec){ec.clear();continue;}if(it->is_regular_file(ec))files.push_back(it->path());}
        std::sort(files.begin(),files.end());
        for(const auto& path:files){const auto name=canonicalName(path.stem().string());
            if(name.ends_with("_metal")&&(result.metalness.empty()||!std::filesystem::exists(result.metalness)))result.metalness=path;
            if(name.ends_with("_rough")&&(result.roughness.empty()||!std::filesystem::exists(result.roughness)))result.roughness=path;
        }
    }
    const auto ensureExportedMaps=[&]{auto folder=materialName;if(folder.rfind("mc_",0)==0)folder.erase(0,3);const auto imageFolder=std::filesystem::path(document.sourceName()).parent_path()/"_images"/"mc"/folder;std::filesystem::path bestColor;int bestColorScore=-1;std::error_code error;for(std::filesystem::directory_iterator it(imageFolder,error),end;it!=end;it.increment(error)){if(error){error.clear();continue;}if(!it->is_regular_file(error)||canonicalName(it->path().extension().string())!=".png")continue;const auto filename=canonicalName(it->path().filename().string());if(containsAny(filename,{"nml","normal","_n.png"})){if(result.normal.empty()||!std::filesystem::exists(result.normal))result.normal=it->path();continue;}if(containsAny(filename,{"_metal","metalness","metallic"})){if(result.metalness.empty()||!std::filesystem::exists(result.metalness))result.metalness=it->path();continue;}if(containsAny(filename,{"_rough","roughness"})){if(result.roughness.empty()||!std::filesystem::exists(result.roughness))result.roughness=it->path();continue;}if(containsAny(filename,{"spc","spec"})||filename.starts_with("~-r")||filename.starts_with("~~-g")){if(result.specular.empty()||!std::filesystem::exists(result.specular))result.specular=it->path();continue;}if(containsAny(filename,{"radiant","default","ao","ambientocclusion"}))continue;const int score=containsAny(filename,{"_col","_color","_text","_glo"})?3:result.lens?2:1;if(score>bestColorScore){bestColorScore=score;bestColor=it->path();}}if((result.albedo.empty()||!std::filesystem::exists(result.albedo))&&!bestColor.empty())result.albedo=bestColor;};
    const auto finalizeSurface=[&]{
        // AW player emblems are runtime-customized cards. The rip references
        // emblem_temp_col without an image; an opaque fallback makes white
        // rectangles on otherwise complete loadouts/exos. Keep real artwork.
        if(normalizedSource.find("/aw/")!=std::string::npos&&canonicalMaterial=="m_mtl_player_emblem"&&
           (result.albedo.empty()||!std::filesystem::is_regular_file(result.albedo)))result.excluded=true;
        // H1 character faction emblems are optional overlay geometry. Some
        // rips contain the card and material, but not its artwork. Rendering
        // the missing image as opaque white hides the correctly textured sleeve.
        // Only omit that absent overlay; keep it when real artwork is supplied.
        if(normalizedSource.find("/mwr/")!=std::string::npos&&canonicalMaterial.starts_with("m_mtl_char_mp_patch_emblem_")&&
           (result.albedo.empty()||!std::filesystem::is_regular_file(result.albedo)))result.excluded=true;
        if(coldWarWeapon){
            // Semantic placeholders are transparent, even when material names
            // are hashed. Never promote heat/noise/ramp resources to albedo.
            auto folder=materialName;if(folder.starts_with("mc_"))folder.erase(0,3);
            const auto images=std::filesystem::path(document.sourceName()).parent_path()/"_images"/"mc"/folder;
            const auto colorName=canonicalName(result.albedo.stem().string());
            if(colorName=="$blacktransparent_color"||colorName=="$transparent")result.excluded=true;
            if(canonicalMaterial.find("_scope_stencil")!=std::string::npos&&colorName.ends_with("_ramp"))result.excluded=true;
            if(result.emissive||colorName.starts_with("$")||result.albedo.empty()){
                std::error_code ec;std::vector<std::filesystem::path> colors;
                for(std::filesystem::directory_iterator it(images,ec),end;it!=end&&!ec;it.increment(ec))
                    if(it->is_regular_file(ec)&&canonicalName(it->path().filename().string()).ends_with("_c.png"))colors.push_back(it->path());
                std::sort(colors.begin(),colors.end());if(colors.size()==1){result.albedo=colors.front();result.color={1,1,1,1};}
            }
        }
        // Some exports give the mesh a generic material name and expose the
        // tritium identity only through the color texture (or its mc folder).
        // Classify from both sources so the global tint reaches every weapon,
        // rather than relying on a per-model exception list.
        const auto colorIdentity=canonicalName(result.albedo.string());
        result.emissive=result.emissive||isEmissiveMaterialIdentity(colorIdentity);
        result.forceAlpha=result.lens||result.emissive||result.eyeOverlay;
        result.camoBlend=weaponSource&&!result.forceAlpha&&!result.excluded;
    };
    for (const auto slot : {"albedo","diffuse"}) {
        const auto surfaceRefs=uints(document,*material,slot);
        if (surfaceRefs.empty()) continue;
        const auto* value=childHash(*material,surfaceRefs[0]);
        if (!value) continue;
        if(value->identifier==kColor){const auto rgba=floats(document,*value,"rgba");if(rgba.size()>=4)result.color={rgba[0],rgba[1],rgba[2],rgba[3]};ensureExportedMaps();finalizeSurface();return result;}
        if(value->identifier==kFile){
            result.color={1,1,1,1};result.albedo=resolveFile(std::filesystem::path(text(*value,"p")));ensureExportedMaps();finalizeSurface();return result;
        }
    }
    ensureExportedMaps();finalizeSurface();return result;
}

void calculateNormals(Mesh& mesh) {
    for (auto& vertex:mesh.vertices) vertex.normal={};
    for (std::size_t i=0;i+2<mesh.indices.size();i+=3) {
        const auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];
        if (ia>=mesh.vertices.size()||ib>=mesh.vertices.size()||ic>=mesh.vertices.size()) continue;
        const auto n=cross(mesh.vertices[ib].position-mesh.vertices[ia].position,
                           mesh.vertices[ic].position-mesh.vertices[ia].position);
        mesh.vertices[ia].normal=mesh.vertices[ia].normal+n;
        mesh.vertices[ib].normal=mesh.vertices[ib].normal+n;
        mesh.vertices[ic].normal=mesh.vertices[ic].normal+n;
    }
    for (auto& vertex:mesh.vertices) vertex.normal=normalize(vertex.normal);
}

std::optional<Mesh> buildMesh(const cast::Document& document, const cast::Node& model,
                              const cast::Node& node, const Mat4& modelTransform,
                              std::size_t boneCount, std::vector<std::string>& warnings, std::unordered_map<std::uint64_t,Surface>& surfaces) {
    const auto positions=floats(document,node,"vp");
    const auto faces=uints(document,node,"f");
    if (positions.empty() || positions.size()%3!=0 || faces.empty() || faces.size()%3!=0) {
        warnings.push_back("Skipping mesh '"+node.displayName()+"': invalid position or face buffer");
        return std::nullopt;
    }
    Mesh mesh;
    mesh.name=node.displayName(); mesh.modelTransform=modelTransform;
    const auto materialRefs=uints(document,node,"m");
    Surface surface;
    if(!materialRefs.empty()){
        const auto found=surfaces.find(materialRefs.front());
        if(found!=surfaces.end())surface=found->second;
        else{surface=materialSurface(document,model,node);surfaces.emplace(materialRefs.front(),surface);}
    }
    if(surface.excluded)return std::nullopt;mesh.color=surface.color;mesh.albedoPath=surface.albedo;mesh.normalPath=surface.normal;mesh.specularPath=surface.specular;mesh.metalnessPath=surface.metalness;mesh.roughnessPath=surface.roughness;mesh.materialName=surface.materialName;mesh.camoBlend=surface.camoBlend;mesh.camoUseAlpha=surface.camoUseAlpha;mesh.lens=surface.lens;mesh.eyeOverlay=surface.eyeOverlay;mesh.emissive=surface.emissive;mesh.forceAlpha=surface.forceAlpha;mesh.ignoreAlbedoAlpha=surface.ignoreAlbedoAlpha;mesh.decal=surface.emissive;
    const auto source=canonicalName(document.sourceName());mesh.normalProfile=containsAny(source,{"/aw/","\\aw\\"})?4:containsAny(source,{"iw5","mw3"})?2:containsAny(source,{"/mw/","\\mw\\","iw3","cod4"})?5:containsAny(source,{"t5","black_ops","/bo/","\\bo\\"})?1:0;
    mesh.vertices.resize(positions.size()/3);
    for (std::size_t i=0;i<mesh.vertices.size();++i) mesh.vertices[i].position={positions[i*3],positions[i*3+1],positions[i*3+2]};
    const auto normals=floats(document,node,"vn");
    if (normals.size()==mesh.vertices.size()*3)
        for (std::size_t i=0;i<mesh.vertices.size();++i) mesh.vertices[i].normal=normalize(Vec3{normals[i*3],normals[i*3+1],normals[i*3+2]});
    const auto uvs=floats(document,node,"u0");
    if (uvs.size()==mesh.vertices.size()*2)
        // Preserve CoD's signed/tiled UVs exactly. The repeat sampler resolves
        // them natively; folding them into 0..1 breaks interpolation across
        // atlas islands and makes packed rifle textures appear scrambled.
        for (std::size_t i=0;i<mesh.vertices.size();++i) mesh.vertices[i].uv={uvs[i*2],uvs[i*2+1]};
    mesh.indices.reserve(faces.size());
    for (auto face:faces) {
        if (face>=mesh.vertices.size()) { warnings.push_back("Mesh '"+mesh.name+"' contains an out-of-range face index"); return std::nullopt; }
        mesh.indices.push_back(static_cast<std::uint32_t>(face));
    }
    if (normals.size()!=mesh.vertices.size()*3) calculateNormals(mesh);

    const auto weightBones=uints(document,node,"wb");
    const auto weightValues=floats(document,node,"wv");
    auto influences=static_cast<std::size_t>(scalar(document,node,"mi",0));
    if (!influences && !weightValues.empty() && weightValues.size()%mesh.vertices.size()==0) influences=weightValues.size()/mesh.vertices.size();
    if (influences && weightBones.size()==mesh.vertices.size()*influences && weightValues.size()==weightBones.size() && boneCount) {
        mesh.skinned=true;
        if(text(node,"sm","linear")=="quaternion") warnings.push_back("Mesh '"+mesh.name+"' requests dual-quaternion skinning; Stage 2 uses linear skinning");
        for (std::size_t v=0;v<mesh.vertices.size();++v) {
            std::array<std::pair<float,std::uint32_t>,4> best{};
            std::unordered_map<std::uint32_t,float> combined;
            for (std::size_t j=0;j<influences;++j) {
                const auto index=v*influences+j;
                const auto bone=static_cast<std::uint32_t>(weightBones[index]);
                if (bone>=boneCount) continue;
                combined[bone]+=weightValues[index];
            }
            for (const auto& [bone,weight] : combined) {
                const auto candidate=std::pair{weight,bone};
                auto where=std::min_element(best.begin(),best.end(),[](auto a,auto b){return a.first<b.first;});
                if (candidate.first>where->first) *where=candidate;
            }
            std::sort(best.begin(),best.end(),[](auto a,auto b){return a.first>b.first;});
            float total{}; for (auto [weight,bone]:best) total+=weight;
            if (total<=1e-8f) { best[0]={1.0f,0}; total=1.0f; }
            for (std::size_t j=0;j<4;++j) { mesh.vertices[v].weights[j]=best[j].first/total; mesh.vertices[v].bones[j]=best[j].second; }
        }
    }
    return mesh;
}

std::optional<TrackProperty> trackProperty(std::string_view property) {
    if(property=="tx")return TrackProperty::TranslationX; if(property=="ty")return TrackProperty::TranslationY;
    if(property=="tz")return TrackProperty::TranslationZ; if(property=="rq")return TrackProperty::Rotation;
    if(property=="sx")return TrackProperty::ScaleX; if(property=="sy")return TrackProperty::ScaleY;
    if(property=="sz")return TrackProperty::ScaleZ; return std::nullopt;
}

TrackMode trackMode(std::string_view mode) {
    if(mode=="relative")return TrackMode::Relative; if(mode=="additive")return TrackMode::Additive;
    return TrackMode::Absolute;
}

float scalarAt(const Track& track,float frame) {
    if(track.frames.size()==1||frame<=track.frames.front())return track.scalarValues.front();
    if(frame>=track.frames.back())return track.scalarValues.back();
    const auto upper=std::upper_bound(track.frames.begin(),track.frames.end(),frame);
    const auto b=static_cast<std::size_t>(upper-track.frames.begin()),a=b-1;
    const float t=(frame-track.frames[a])/static_cast<float>(track.frames[b]-track.frames[a]);
    return track.scalarValues[a]+(track.scalarValues[b]-track.scalarValues[a])*t;
}

Quat rotationAt(const Track& track,float frame) {
    if(track.frames.size()==1||frame<=track.frames.front())return track.rotationValues.front();
    if(frame>=track.frames.back())return track.rotationValues.back();
    const auto upper=std::upper_bound(track.frames.begin(),track.frames.end(),frame);
    const auto b=static_cast<std::size_t>(upper-track.frames.begin()),a=b-1;
    const float t=(frame-track.frames[a])/static_cast<float>(track.frames[b]-track.frames[a]);
    return slerp(track.rotationValues[a],track.rotationValues[b],t);
}

void applyScalar(float& target,float rest,float value,TrackMode mode,float weight) {
    if(mode==TrackMode::Absolute)target=value;
    else if(mode==TrackMode::Relative)target=rest+value;
    else target+=value*weight;
}

Animation buildAnimation(const cast::Document& document, const cast::Node& node, const Skeleton& skeleton,
                         std::vector<std::string>& warnings) {
    Animation animation;
    animation.name=text(node,"n","animation");animation.sourceName=std::filesystem::path(document.sourceName()).filename().string();
    animation.framerate=std::max(1.0f,scalar(document,node,"fr",30));
    animation.looping=scalar(document,node,"lo",0)>=1;
    const auto sourceStem=std::filesystem::path(animation.sourceName).stem().string();
    if(canonicalName(sourceStem).find("_loop")!=std::string::npos)animation.looping=true;
    classifyAnimationName(animation.name=="animation"||animation.name.empty()?sourceStem:animation.name+"_"+sourceStem,animation);
    // Filename heuristics cannot distinguish all action-first Source 2 clips
    // (reload_awp, idle1_karambit, light_backstab2_...). The export directory
    // is authoritative for domain; retain the existing CoD classification.
    auto sourcePath=canonicalName(document.sourceName());
    std::replace(sourcePath.begin(),sourcePath.end(),'\\','/');
    if(sourcePath.find("/cs2/animations/viewmodel/")!=std::string::npos)
        animation.domain=AnimationDomain::ViewModel;

    std::vector<std::string> missingBones;
    for (const auto& curve:node.children) {
        if(curve.identifier==kNotification){
            Animation::Notification notification;notification.name=text(curve,"n","event");
            for(const auto value:uints(document,curve,"kb"))notification.frames.push_back(static_cast<std::uint32_t>(value));
            if(!notification.frames.empty())animation.durationFrames=std::max(animation.durationFrames,*std::max_element(notification.frames.begin(),notification.frames.end()));
            animation.notifications.push_back(std::move(notification));continue;
        }
        if(curve.identifier!=kCurve)continue;
        ++animation.sourceCurveCount;
        const auto curveNodeName=text(curve,"nn");
        auto boneIt=skeleton.boneByName.find(curveNodeName);
        std::size_t boneIndex{};
        if(boneIt!=skeleton.boneByName.end())boneIndex=boneIt->second;
        else {
            const auto canonical=skeleton.boneByCanonicalName.find(canonicalName(curveNodeName));
            if(canonical==skeleton.boneByCanonicalName.end()){++animation.unmappedCurveCount;if(missingBones.size()<12&&std::find(missingBones.begin(),missingBones.end(),curveNodeName)==missingBones.end())missingBones.push_back(curveNodeName);continue;}
            boneIndex=canonical->second;
        }
        const auto property=trackProperty(text(curve,"kp"));
        if(!property)continue;
        const auto frames64=uints(document,curve,"kb");
        const auto values=floats(document,curve,"kv");
        if(frames64.empty())continue;
        Track track; track.boneIndex=boneIndex; track.property=*property;
        track.mode=trackMode(text(curve,"m","absolute")); track.additiveWeight=scalar(document,curve,"ab",1);
        if(track.property!=TrackProperty::Rotation&&track.property!=TrackProperty::ScaleX&&track.property!=TrackProperty::ScaleY&&track.property!=TrackProperty::ScaleZ&&skeleton.bones[boneIndex].translationTracksAreDeltas)track.mode=TrackMode::Relative;
        for(auto frame:frames64) track.frames.push_back(static_cast<std::uint32_t>(frame));
        if(track.property==TrackProperty::Rotation) {
            if(values.size()!=track.frames.size()*4){warnings.push_back("Skipping malformed rotation curve for '"+text(curve,"nn")+"'");continue;}
            for(std::size_t i=0;i<track.frames.size();++i)track.rotationValues.push_back(normalize(Quat{values[i*4],values[i*4+1],values[i*4+2],values[i*4+3]}));
        } else {
            if(values.size()!=track.frames.size()){warnings.push_back("Skipping malformed scalar curve for '"+text(curve,"nn")+"'");continue;}
            track.scalarValues=values;
            if(track.mode==TrackMode::Absolute){float offset{};if(track.property==TrackProperty::TranslationX)offset=skeleton.bones[boneIndex].absoluteTranslationOffset.x;else if(track.property==TrackProperty::TranslationY)offset=skeleton.bones[boneIndex].absoluteTranslationOffset.y;else if(track.property==TrackProperty::TranslationZ)offset=skeleton.bones[boneIndex].absoluteTranslationOffset.z;
                if(offset!=0)for(auto& value:track.scalarValues)value+=offset;}
        }
        animation.durationFrames=std::max(animation.durationFrames,*std::max_element(track.frames.begin(),track.frames.end()));
        animation.tracks.push_back(std::move(track));
    }
    if(animation.sourceCurveCount&&animation.tracks.empty())warnings.push_back("Animation '"+animation.name+"' has no transform curves matching the loaded skeleton");
    else if(animation.unmappedCurveCount){std::string names;for(const auto& name:missingBones){if(!names.empty())names+=", ";names+=name;}warnings.push_back("Animation '"+animation.name+"' skipped "+std::to_string(animation.unmappedCurveCount)+" curve(s) whose bones were not found; source="+std::filesystem::path(document.sourceName()).filename().string()+"; bones="+names);}
    if(sourceStem.starts_with("mp_")&&animation.domain==AnimationDomain::PlayerTorso){
        std::vector<bool> upper(skeleton.bones.size());
        for(size_t b=0;b<upper.size();++b){const auto n=canonicalName(skeleton.bones[b].name);const auto p=skeleton.bones[b].parent;upper[b]=n.starts_with("j_spine")||n.starts_with("spine")||n=="j_neck"||n.starts_with("j_clavicle")||(p>=0&&upper[p]);}
        for(auto& track:animation.tracks)track.ownsLayer=track.boneIndex<upper.size()&&upper[track.boneIndex];
    }
    return animation;
}

} // namespace

void classifyAnimationName(std::string_view input,Animation& animation) {
    std::string name(input);std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    const auto tokens=nameTokens(name);
    animation.role=AnimationRole::Unknown;animation.domain=AnimationDomain::Other;
    animation.motion=MotionRole::Unknown;animation.action=ActionRole::None;animation.weapon=WeaponClass::Any;
    animation.stance=Stance::Any;animation.direction=Direction::Any;
    animation.aimYawDegrees=animation.aimPitchDegrees=0;animation.hasAimYaw=animation.hasAimPitch=false;

    if(tokenIs(tokens,{"pb"}))animation.domain=AnimationDomain::PlayerBody;
    else if(tokenIs(tokens,{"pt"}))animation.domain=AnimationDomain::PlayerTorso;
    else if(name.starts_with("mp_")||tokenIs(tokens,{"mp"})){animation.domain=AnimationDomain::PlayerBody;}
    else if(tokenIs(tokens,{"viewmodel","vm","va"})||name.starts_with("viewmodel")||name.starts_with("h1_wpn_")||containsAny(name,{"shoot1_","draw_","idle_","lookat01_","lookat02_","lookat03_","heavy_hit1_","light_hit1_","heavy_miss1_","light_miss1_"}))animation.domain=AnimationDomain::ViewModel;

    if(tokenContains(tokens,{"crouch","duck"})||(animation.domain==AnimationDomain::PlayerBody&&tokenIs(tokens,{"cr"})))animation.stance=Stance::Crouch;
    else if(tokenContains(tokens,{"prone","crawl"}))animation.stance=Stance::Prone;
    else if(tokenContains(tokens,{"stand"})||animation.domain==AnimationDomain::PlayerBody||animation.domain==AnimationDomain::PlayerTorso)animation.stance=Stance::Stand;

    // A numbered aim variant (mp_*_aim_2) is not a stance transition.
    bool standaloneTransition=false;
    for(std::size_t i=0;i<tokens.size();++i)if(tokens[i]=="2"&&(i==0||tokens[i-1]!="aim"))standaloneTransition=true;
    const bool transition=tokenIs(tokens,{"to"})||standaloneTransition||tokenContains(tokens,{"2idle","2stand","2crouch","2prone","2run","2walk","2sprint","run2idle","stand_run2idle"});
    if(transition)animation.motion=MotionRole::Transition;
    else if(tokenContains(tokens,{"stumble"}))animation.motion=MotionRole::Stumble;
    else if(tokenContains(tokens,{"slide"}))animation.motion=MotionRole::Slide;
    else if(tokenContains(tokens,{"dive"}))animation.motion=MotionRole::Dive;
    else if(tokenContains(tokens,{"ladder"})||name.find("ladder")!=std::string::npos)animation.motion=MotionRole::Ladder;
    else if(tokenContains(tokens,{"climb","mantle","vault"}))animation.motion=MotionRole::Climb;
    else if(tokenContains(tokens,{"land"}))animation.motion=MotionRole::Land;
    else if(tokenContains(tokens,{"jump","takeoff","standjump"}))animation.motion=MotionRole::Jump;
    else if(tokenContains(tokens,{"turn"}))animation.motion=MotionRole::Turn;
    else if(tokenContains(tokens,{"sprint"}))animation.motion=MotionRole::Sprint;
    else if(tokenContains(tokens,{"crawl"}))animation.motion=MotionRole::Crawl;
    else if(tokenContains(tokens,{"run","jog","combatrun","run_fast"}))animation.motion=MotionRole::Run;
    else if(tokenContains(tokens,{"walk","shuffle"}))animation.motion=MotionRole::Walk;
    else if(tokenContains(tokens,{"idle"})||tokenContains(tokens,{"stand","hold","loop"}))animation.motion=MotionRole::Idle;

    if(name.find("standjump_boost_takeoff_knife")!=std::string::npos){
        animation.motion=MotionRole::Jump;
        animation.weapon=WeaponClass::Knife;
    }

    if(tokenContains(tokens,{"death","dead"})||tokenIs(tokens,{"die"}))animation.action=ActionRole::Death;
    else if(tokenContains(tokens,{"flinch"}))animation.action=ActionRole::Flinch;
    else if(tokenContains(tokens,{"shellshock"}))animation.action=ActionRole::Shellshock;
    else if(tokenContains(tokens,{"reload","rechamber"}))animation.action=ActionRole::Reload;
    else if(tokenContains(tokens,{"riotshield"})&&tokenContains(tokens,{"deploy"}))animation.action=ActionRole::Deploy;
    else if(tokenContains(tokens,{"fire","shoot","shoot1"}))animation.action=ActionRole::Fire;
    else if(tokenContains(tokens,{"melee","bash","attack","stab","swipe","thrust"})||containsAny(name,{"heavy_hit","light_hit","heavy_miss","light_miss"}))animation.action=ActionRole::Melee;
    else if(tokenContains(tokens,{"bombplant","plant"}))animation.action=ActionRole::Plant;
    else if(tokenContains(tokens,{"deploy"}))animation.action=ActionRole::Deploy;
    else if(tokenContains(tokens,{"putaway2shield"}))animation.action=ActionRole::SwitchWeapon;
    else if(tokenContains(tokens,{"putaway","holster"}))animation.action=ActionRole::Unequip;
    else if(tokenContains(tokens,{"firstraise"})||(tokenIs(tokens,{"first"})&&(tokenIs(tokens,{"raise"})||tokenIs(tokens,{"pullout"}))))animation.action=ActionRole::FirstRaise;
    else if(tokenContains(tokens,{"pullout","draw"})||tokenIs(tokens,{"raise","equip"}))animation.action=ActionRole::Equip;
    else if(tokenContains(tokens,{"pullpin"})||tokenIs(tokens,{"prime"}))animation.action=ActionRole::GrenadePrep;
    else if(tokenIs(tokens,{"throw"}))animation.action=ActionRole::Throw;
    else if(tokenContains(tokens,{"lookat01","lookat02","lookat03","lookat","inspect"}))animation.action=ActionRole::Inspect;
    else if(tokenContains(tokens,{"alert"})&&animation.domain==AnimationDomain::Other)animation.action=ActionRole::Alert;
    else if(tokenContains(tokens,{"dance","gesture"}))animation.action=ActionRole::Gesture;
    else if(tokenContains(tokens,{"aim","ads"})&&animation.motion==MotionRole::Idle&&tokenContains(tokens,{"gunner","mg42"}))animation.action=ActionRole::Aim;
    if(animation.domain==AnimationDomain::ViewModel&&tokenIs(tokens,{"ads"})&&(tokenIs(tokens,{"up","down"})||tokenContains(tokens,{"adsfire"})))animation.action=ActionRole::Aim;

    if(animation.motion==MotionRole::Unknown&&(animation.domain==AnimationDomain::PlayerBody||animation.domain==AnimationDomain::PlayerTorso))animation.motion=MotionRole::Idle;

    if(tokenContains(tokens,{"riotshield"})||tokenIs(tokens,{"riot"})||((animation.domain==AnimationDomain::PlayerBody||animation.domain==AnimationDomain::PlayerTorso)&&tokenIs(tokens,{"shield"})))animation.weapon=WeaponClass::RiotShield;
    else if(name.starts_with("pb_mini")||tokenContains(tokens,{"minigun"}))animation.weapon=WeaponClass::Minigun;
    else if(tokenContains(tokens,{"mg42gunner","sawgunner"})||tokenIs(tokens,{"saw"}))animation.weapon=WeaponClass::Heavy;
    else if(tokenIs(tokens,{"b"})&&tokenContains(tokens,{"knife"}))animation.weapon=WeaponClass::BallisticKnife;
    else if(tokenIs(tokens,{"dw","dualwield","akimbo"}))animation.weapon=WeaponClass::DualWield;
    else if(tokenContains(tokens,{"crossbow"}))animation.weapon=WeaponClass::Crossbow;
    else if(tokenContains(tokens,{"m1216"}))animation.weapon=WeaponClass::M1216;
    else if(tokenContains(tokens,{"judge"}))animation.weapon=WeaponClass::Judge;
    else if(tokenContains(tokens,{"g11"}))animation.weapon=WeaponClass::G11;
    else if(tokenContains(tokens,{"sniper"}))animation.weapon=WeaponClass::Sniper;
    else if(tokenIs(tokens,{"smg"}))animation.weapon=WeaponClass::Automatic;
    else if(tokenIs(tokens,{"shot"})||tokenContains(tokens,{"shotgun"}))animation.weapon=WeaponClass::Shotgun;
    else if(tokenIs(tokens,{"lmg"})||((animation.domain==AnimationDomain::PlayerBody||animation.domain==AnimationDomain::PlayerTorso)&&tokenIs(tokens,{"mg"})))animation.weapon=WeaponClass::LMG;
    else if(tokenContains(tokens,{"pistol"}))animation.weapon=WeaponClass::Pistol;
    else if(tokenContains(tokens,{"knife","karambit","bayonet","butterfly"}))animation.weapon=WeaponClass::Knife;
    else if(tokenContains(tokens,{"grenade","tomahawk"}))animation.weapon=WeaponClass::Grenade;
    else if(tokenContains(tokens,{"rpg","launcher"}))animation.weapon=WeaponClass::Launcher;
    else if(tokenContains(tokens,{"tablet"}))animation.weapon=WeaponClass::Tablet;
    else if(tokenContains(tokens,{"radio"}))animation.weapon=WeaponClass::Radio;
    else if(tokenContains(tokens,{"briefcase"}))animation.weapon=WeaponClass::Briefcase;
    else if(tokenIs(tokens,{"rc"})||tokenContains(tokens,{"rcxd"}))animation.weapon=WeaponClass::RC;
    else if(tokenContains(tokens,{"hold","satchel","claymore","c4","trophy"}))animation.weapon=WeaponClass::Equipment;
    else if(tokenContains(tokens,{"rifle"}))animation.weapon=WeaponClass::Rifle;
    else if(tokenIs(tokens,{"auto"})||tokenContains(tokens,{"mp40"}))animation.weapon=WeaponClass::Automatic;
    if(animation.domain==AnimationDomain::ViewModel&&animation.weapon==WeaponClass::Any){
        if(containsAny(name,{"beretta2023r","b2023r","fiveseven","tac45","kap40","python"}))animation.weapon=WeaponClass::Pistol;
        else if(containsAny(name,{"an94","xm8","sa58","saritch","scar_h","sig556","tavor","type95_ar","peacekeeper_ar","m4a4","m4a1"})||name.ends_with("_ak")||name.ends_with("_ak.cast")||name.find("_ak_")!=std::string::npos)animation.weapon=WeaponClass::Rifle;
        else if(containsAny(name,{"mp7","pdw57","vector","skorpion_evo","peacekeeper"}))animation.weapon=WeaponClass::Automatic;
        else if(containsAny(name,{"870mcs","saiga12","ksg"}))animation.weapon=WeaponClass::Shotgun;
        else if(containsAny(name,{"hamr","mk48","qbb95","type95_lmg"}))animation.weapon=WeaponClass::LMG;
        else if(containsAny(name,{"ballista","dsr50","svu_as","xpr50","awp","ssg08","remington","remington700","m40a3"}))animation.weapon=WeaponClass::Sniper;
        else if(containsAny(name,{"fhj18","smaw","usrpg","rpg"}))animation.weapon=WeaponClass::Launcher;
        else if(containsAny(name,{"ballistic_knife"}))animation.weapon=WeaponClass::BallisticKnife;
        else if(containsAny(name,{"crossbow"}))animation.weapon=WeaponClass::Crossbow;
        else if(containsAny(name,{"knife_","karambit","bayonet","butterfly"}))animation.weapon=WeaponClass::Knife;
    }
    if(animation.domain==AnimationDomain::PlayerTorso&&(animation.action==ActionRole::Fire||animation.action==ActionRole::Reload)&&animation.weapon==WeaponClass::Any){
        if(tokenContains(tokens,{"crouchwalk"})||tokenContains(tokens,{"shoot"})||tokenContains(tokens,{"reload"}))animation.weapon=WeaponClass::Rifle;
    }
    if(animation.domain==AnimationDomain::PlayerBody&&animation.weapon==WeaponClass::Any){
        if(tokenContains(tokens,{"alert"})||((tokenContains(tokens,{"aim"})||tokenContains(tokens,{"ads"}))&&!tokenContains(tokens,{"pistol","rpg","gunner","mg42"}))||
           (tokenContains(tokens,{"combatrun"})&&!tokenContains(tokens,{"grenade","pistol","rpg"}))||
           (tokenContains(tokens,{"sprint"})&&!tokenContains(tokens,{"unarmed","pistol","rpg","grenade","knife","dw","smg","lmg","riotshield","minigun","hold"}))||
           (tokenIs(tokens,{"runjump"})&&!tokenContains(tokens,{"pistol"}))||
           (tokenContains(tokens,{"crouch"})&&tokenIs(tokens,{"run"})&&!tokenContains(tokens,{"grenade","pistol","rpg","unarmed"}))||
           (tokenContains(tokens,{"prone"})&&tokenContains(tokens,{"crawl"})&&!tokenContains(tokens,{"pistol","rpg","dw","knife","grenade"})))animation.weapon=WeaponClass::Rifle;
    }
    if(animation.action==ActionRole::Fire&&animation.domain==AnimationDomain::PlayerBody&&animation.weapon==WeaponClass::Any&&!tokenContains(tokens,{"unarmed"}))animation.weapon=WeaponClass::Rifle;
    if(name.starts_with("mp_")&&(animation.action==ActionRole::Reload||animation.action==ActionRole::Fire)&&(animation.motion==MotionRole::Idle||animation.motion==MotionRole::Unknown)){
        // IW6/H1 use mp_ for both body locomotion and upper-body gun actions.
        // Their ordinary reload/fire clips must be discoverable by torso layers.
        if(animation.weapon==WeaponClass::Any&&!tokenContains(tokens,{"unarmed"}))animation.weapon=WeaponClass::Rifle;
        animation.domain=AnimationDomain::PlayerTorso;
    }
    animation.ads=tokenIs(tokens,{"ads"});
    bool aimGrid=false;
    for(size_t i=1;i<tokens.size();++i)if(tokens[i-1]=="aim"&&!tokens[i].empty()&&std::all_of(tokens[i].begin(),tokens[i].end(),[](unsigned char c){return std::isdigit(c)!=0;}))aimGrid=true;
    const bool worldDomain=animation.domain==AnimationDomain::PlayerBody||animation.domain==AnimationDomain::PlayerTorso;
    animation.contextual=tokenContains(tokens,{"slope","lowwall","flare"})||(worldDomain&&(aimGrid||tokenContains(tokens,{"laststand","wpnswp"})||name.find("wpn_swp")!=std::string::npos));
    if(worldDomain&&name.starts_with("mp_")&&(tokenIs(tokens,{"bomb"})||(tokenIs(tokens,{"rpg"})&&tokenIs(tokens,{"knife"}))))animation.contextual=true;
    if(animation.action==ActionRole::Reload){
        if(tokenIs(tokens,{"gl"}))animation.reloadStyle=ReloadStyle::GL;
        else if(tokenContains(tokens,{"handleclip"}))animation.reloadStyle=ReloadStyle::HandleClip;
        else if(tokenContains(tokens,{"rearclip"}))animation.reloadStyle=ReloadStyle::RearClip;
        else if(tokenContains(tokens,{"mp40"}))animation.reloadStyle=ReloadStyle::MP40;
        else animation.reloadStyle=ReloadStyle::Standard;
    }

    const bool diagFL=containsAny(name,{"_fl.","_fl_","_forward_left","_front_left"})||name.ends_with("_fl");
    const bool diagFR=containsAny(name,{"_fr.","_fr_","_forward_right","_front_right"})||name.ends_with("_fr");
    const bool diagBL=containsAny(name,{"_bl.","_bl_","_backward_left","_back_left"})||name.ends_with("_bl");
    const bool diagBR=containsAny(name,{"_br.","_br_","_backward_right","_back_right"})||name.ends_with("_br");

    const bool shortForward=containsAny(name,{"_run_f","_walk_f","_crawl_f","_shuffle_f","_stumble_f","_f."})||name.ends_with("_f")||(tokenIs(tokens,{"f"})&&animation.motion!=MotionRole::Idle);
    const bool shortBackward=containsAny(name,{"_run_b","_walk_b","_crawl_b","_shuffle_b","_stumble_b","_b."})||name.ends_with("_b")||(tokenIs(tokens,{"b"})&&animation.motion!=MotionRole::Idle);
    const bool shortLeft=containsAny(name,{"_run_l","_walk_l","_crawl_l","_shuffle_l","_stumble_l","_l."})||name.ends_with("_l")||(tokenIs(tokens,{"l"})&&animation.motion!=MotionRole::Idle);
    const bool shortRight=containsAny(name,{"_run_r","_walk_r","_crawl_r","_shuffle_r","_stumble_r","_r."})||name.ends_with("_r")||(tokenIs(tokens,{"r"})&&animation.motion!=MotionRole::Idle);

    if(diagFL)animation.direction=Direction::ForwardLeft;
    else if(diagFR)animation.direction=Direction::ForwardRight;
    else if(diagBL)animation.direction=Direction::BackwardLeft;
    else if(diagBR)animation.direction=Direction::BackwardRight;
    else if(tokenIs(tokens,{"forward","front","fwd","fw"})||tokenContains(tokens,{"forward","onfront","frontspin"})||shortForward)animation.direction=Direction::Forward;
    else if(tokenIs(tokens,{"back","backward","bwd","bk"})||shortBackward)animation.direction=Direction::Backward;
    else if(tokenIs(tokens,{"left","lft"})||tokenContains(tokens,{"left"})||shortLeft)animation.direction=Direction::Left;
    else if(tokenIs(tokens,{"right","rgt"})||tokenContains(tokens,{"right"})||shortRight)animation.direction=Direction::Right;
    else if(animation.domain==AnimationDomain::PlayerBody&&animation.motion==MotionRole::Sprint)animation.direction=Direction::Forward;

    if(const auto right=angularToken(tokens,"right",'r')){animation.aimYawDegrees=*right;animation.hasAimYaw=true;}
    else if(const auto left=angularToken(tokens,"left",'l')){animation.aimYawDegrees=-*left;animation.hasAimYaw=true;}
    if(const auto up=angularToken(tokens,"up",'u')){animation.aimPitchDegrees=*up;animation.hasAimPitch=true;}
    else if(const auto down=angularToken(tokens,"down",'d')){animation.aimPitchDegrees=-*down;animation.hasAimPitch=true;}
    else if(tokenIs(tokens,{"level","center"})){animation.aimPitchDegrees=0;animation.hasAimPitch=true;}

    if(animation.action==ActionRole::Death)animation.role=AnimationRole::Death;
    else if(animation.action==ActionRole::Reload)animation.role=AnimationRole::Reload;
    else if(animation.action==ActionRole::Melee)animation.role=AnimationRole::Melee;
    else if(animation.action==ActionRole::Fire)animation.role=AnimationRole::Fire;
    else switch(animation.motion){
        case MotionRole::Idle:animation.role=AnimationRole::Idle;break;
        case MotionRole::Walk:case MotionRole::Crawl:animation.role=AnimationRole::Walk;break;
        case MotionRole::Run:animation.role=AnimationRole::Run;break;
        case MotionRole::Sprint:animation.role=AnimationRole::Sprint;break;
        case MotionRole::Jump:case MotionRole::Land:animation.role=AnimationRole::Jump;break;
        default:break;
    }
}

CastScene buildScene(const cast::Document& document,bool prepareViewmodel) {
    CastScene result;
    if(!document.valid()){result.warnings.push_back("Cannot build a scene from an invalid Cast document");return result;}
    for(const auto& root:document.roots()) for(const auto& model:root.children) {
        if(model.identifier!=kModel)continue;
        const auto* skeletonNode=childOf(model,kSkeleton);
        if(skeletonNode && result.skeleton.bones.empty()) result.skeleton=buildSkeleton(document,*skeletonNode,result.warnings);
        const auto transform=nodeTransform(document,model);
        // Lifetime is one model load: no stale paths after assets are re-exported.
        std::unordered_map<std::uint64_t,Surface> surfaces;
        for(const auto& node:model.children) if(node.identifier==kMesh) {
            if(auto mesh=buildMesh(document,model,node,transform,result.skeleton.bones.size(),result.warnings,surfaces)) result.meshes.push_back(std::move(*mesh));
        }
    }
    // Authored *_camo meshes overlap the solid receiver. Custom camos belong
    // on the solid base surface; retaining only the alpha-bearing duplicate
    // made weapons such as the DSR50 appear transparent.
    if(containsAny(canonicalName(document.sourceName()),{"juggernaut","jugg"}))if(const auto head=result.skeleton.boneByCanonicalName.find("j_head");head!=result.skeleton.boneByCanonicalName.end())for(auto& mesh:result.meshes)if(containsAny(canonicalName(mesh.materialName),{"jugg_head","headgear"}))for(auto& vertex:mesh.vertices){vertex.bones={static_cast<std::uint32_t>(head->second),0,0,0};vertex.weights={1,0,0,0};}
    if(prepareViewmodel)prepareColdWarViewmodel(result,std::filesystem::path(document.sourceName()).filename().string());
    if(pointblank::exportedByPb2cast(document))pointblank::normalize(result);
    codm::prepareWorldBody(result,std::filesystem::path(document.sourceName()));
    if(containsAny(canonicalName(std::filesystem::path(document.sourceName()).generic_string()),{"/codm/"})){
        std::string nativeError;
        if(codm::nativeMetres(document.sourceName(),nativeError))
            for(auto& mesh:result.meshes)mesh.specularGlossiness=true;
    }
    appendAnimations(document,result);
    for(const auto& mesh:result.meshes) for(const auto& vertex:mesh.vertices) {
        const auto p=transformPoint(mesh.modelTransform,vertex.position);
        if(!result.bounds.valid){result.bounds.minimum=result.bounds.maximum=p;result.bounds.valid=true;}
        else {
            result.bounds.minimum={std::min(result.bounds.minimum.x,p.x),std::min(result.bounds.minimum.y,p.y),std::min(result.bounds.minimum.z,p.z)};
            result.bounds.maximum={std::max(result.bounds.maximum.x,p.x),std::max(result.bounds.maximum.y,p.y),std::max(result.bounds.maximum.z,p.z)};
        }
    }
    return result;
}

// Native world reference is independent of the first-person hierarchy rewrite.
// Model discovery and binding happen only during import, never during sampling.
static void attachColdWarWorldPoses(const cast::Document& document,CastScene& target,std::size_t first){
    if(first>=target.animations.size())return;
    const auto path=std::filesystem::path(document.sourceName());
    const auto stem=path.stem().string();
    const bool source2Rig=std::any_of(target.skeleton.bones.begin(),target.skeleton.bones.end(),[](const auto& bone){return bone.name=="root_motion"||bone.name=="armUpperShoulder_L";});
    if((!stem.starts_with("pb_")&&!stem.starts_with("pt_"))||target.skeleton.boneByCanonicalName.contains("tag_view")||source2Rig||target.skeleton.boneByName.contains(nativeBoneHash("tag_origin")))return;
    std::filesystem::path root;
    for(auto p=path.parent_path();!p.empty();){const auto name=p.filename().string();if(name=="bocw_sp"||name=="bocw"||name=="t9"){root=p;break;}const auto parent=p.parent_path();if(parent==p)break;p=parent;}
    if(root.empty())return;
    for(const auto* name:{"j_mainroot","j_shoulder_le","j_shoulder_ri","j_wrist_le","j_wrist_ri","j_knee_le","j_knee_ri"})
        if(!target.skeleton.boneByCanonicalName.contains(name))return;
    static std::mutex mutex;
    static std::unordered_map<std::string,std::shared_ptr<const Skeleton>> references;
    std::shared_ptr<const Skeleton> reference;
    {std::lock_guard lock(mutex);auto found=references.find(root.string());
        if(found==references.end()){
            std::filesystem::path model;std::error_code ec;
            for(std::filesystem::recursive_directory_iterator it(root/"models/viewhands",std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
                if(ec){ec.clear();continue;}if(it->path().filename().string().ends_with("arms_black2_LOD0.cast")){model=it->path();break;}
            }
            std::shared_ptr<const Skeleton> loaded;
            if(!model.empty()){auto native=buildScene(cast::Document::load(model),false);addColdWarWorldAliases(native.skeleton);
                bool complete=true;for(const auto* name:{"j_mainroot","j_hip_le","j_hip_ri","j_knee_le","j_knee_ri","j_ankle_le","j_ankle_ri","j_wrist_le","j_wrist_ri"})complete&=native.skeleton.boneByCanonicalName.contains(name);
                if(complete)loaded=std::make_shared<const Skeleton>(std::move(native.skeleton));
            }
            found=references.emplace(root.string(),std::move(loaded)).first;
        }reference=found->second;
    }
    if(!reference){target.warnings.push_back("Cold War world adapter unavailable: export the native arms_black2 body skeleton. Rejected clip: "+path.filename().string());target.animations.resize(first);return;}
    auto native=std::make_shared<CastScene>();native->skeleton=*reference;
    for(const auto& r:document.roots())for(const auto& node:r.children)if(node.identifier==kAnimation)native->animations.push_back(buildAnimation(document,node,native->skeleton,native->warnings));
    for(std::size_t i=first;i<target.animations.size()&&i-first<native->animations.size();++i){
        auto adapter=makeColdWarWorldPose(target.skeleton,native,i-first);if(!adapter)continue;
        auto& animation=target.animations[i];animation.coldWarWorldPose=adapter;
        const auto obsolete="Animation '"+animation.name+"' skipped ";
        std::erase_if(target.warnings,[&](const auto& warning){return warning.starts_with(obsolete)&&warning.find("curve(s) whose bones were not found; source="+path.filename().string()+";")!=std::string::npos;});
        animation.durationFrames=native->animations[i-first].durationFrames;
        animation.framerate=native->animations[i-first].framerate;
        animation.looping=native->animations[i-first].looping;
        animation.unmappedCurveCount=native->animations[i-first].unmappedCurveCount;
        // Tracks here are channel-ownership metadata only. Runtime samples the
        // unchanged native curves; normal layer masking still sees driven joints.
        std::vector<bool> driven(native->skeleton.bones.size());
        for(const auto&t:native->animations[i-first].tracks)if(t.boneIndex<driven.size())driven[t.boneIndex]=true;
        for(std::size_t b=0;b<driven.size();++b){const auto p=native->skeleton.bones[b].parent;if(p>=0&&driven[p])driven[b]=true;}
        animation.tracks.clear();
        std::vector<bool> upper(target.skeleton.bones.size());
        for(std::size_t b=0;b<upper.size();++b){const auto name=canonicalName(target.skeleton.bones[b].name);const auto p=target.skeleton.bones[b].parent;upper[b]=name.starts_with("j_spine")||name=="j_neck"||name=="j_clavicle_le"||name=="j_clavicle_ri"||(p>=0&&upper[p]);}
        for(std::size_t b=0;b<adapter->sourceBones.size();++b){const auto mapped=adapter->sourceBones[b];if(mapped<0)continue;
            Track track;track.boneIndex=b;track.property=TrackProperty::Rotation;track.frames={0};track.rotationValues={target.skeleton.bones[b].restLocal.rotation};track.ownsLayer=driven[mapped]&&(!stem.starts_with("pt_")||upper[b]);animation.tracks.push_back(track);
            const auto name=canonicalName(target.skeleton.bones[b].name);
            if(target.skeleton.bones[b].parent<0||name=="j_mainroot"||name.starts_with("tag_"))for(int axis=0;axis<3;++axis){track.property=static_cast<TrackProperty>(axis);track.rotationValues.clear();track.scalarValues={0};animation.tracks.push_back(track);}
        }
        target.warnings.push_back("Cold War world runtime adapter: "+path.filename().string()+"; native unmapped curves="+std::to_string(animation.unmappedCurveCount));
    }
}

std::size_t appendAnimations(const cast::Document& document,CastScene& scene) {
    const auto before=scene.animations.size();
    if(scene.codmNativeCentimetres){
        bool hasAnimation=false;for(const auto& root:document.roots())for(const auto& node:root.children)hasAnimation|=node.identifier==kAnimation;
        if(!hasAnimation)return 0;
        const auto path=std::filesystem::path(document.sourceName());const auto meta=codm::metadata(path);
        if(!meta.is_object()||!meta.contains("t6Compatible")||meta["t6Compatible"]!=false||meta.contains("t6Compatibility")||(!scene.codmNativeWeaponStem.empty()&&!path.stem().string().starts_with(scene.codmNativeWeaponStem+"_"))){scene.warnings.push_back("Rejected incompatible native CODM animation: "+path.filename().string());return 0;}
        if(meta.contains("targetKind")&&meta["targetKind"]=="camera")return 0; // Paired with its action below, never a competing action.
        if(meta.contains("omittedBindings")&&meta["omittedBindings"].is_array()&&!meta["omittedBindings"].empty())scene.warnings.push_back("CODM source omitted bindings ("+std::to_string(meta["omittedBindings"].size())+"): "+path.filename().string());
    }
    for(const auto& root:document.roots()) for(const auto& node:root.children)
        if(node.identifier==kAnimation) scene.animations.push_back(buildAnimation(document,node,scene.skeleton,scene.warnings));
    if(scene.codmNativeCentimetres){
        for(std::size_t i=before;i<scene.animations.size();++i){auto& a=scene.animations[i];for(auto& t:a.tracks)if(t.property<=TrackProperty::TranslationZ)for(auto& v:t.scalarValues)v*=scene.codmTranslationFactor;if(a.viewmodelCameraReference)for(int k:{12,13,14})a.viewmodelCameraReference->v[k]*=scene.codmTranslationFactor;}
        const auto path=std::filesystem::path(document.sourceName());const auto cameraPath=path.parent_path()/(path.stem().string()+"_camera.cast");std::error_code ec;
        if(std::filesystem::exists(cameraPath,ec)&&scene.skeleton.boneByName.contains("codm_camera_motion")){
            const auto meta=codm::metadata(cameraPath);
            if(!meta.is_object()||!meta.contains("t6Compatible")||meta["t6Compatible"]!=false||!meta.contains("targetKind")||meta["targetKind"]!="camera")scene.warnings.push_back("Invalid CODM camera metadata: "+cameraPath.filename().string());
            else {const auto cameraDoc=cast::Document::load(cameraPath);for(const auto& root:cameraDoc.roots())for(const auto& node:root.children)if(node.identifier==kAnimation){
                auto camera=buildAnimation(cameraDoc,node,scene.skeleton,scene.warnings);
                if(scene.animations.size()!=before+1||camera.framerate!=scene.animations[before].framerate){scene.warnings.push_back("CODM camera/action FPS mismatch: "+cameraPath.filename().string());continue;}
                if(camera.durationFrames!=scene.animations[before].durationFrames)scene.warnings.push_back("CODM camera/action length differs; preserve FPS and hold camera endpoint: "+cameraPath.filename().string());
                if(!scene.codmNativeCameraCalibrated){
                    // Camera exports carry a controller-facing absolute baseline
                    // unlike the weapon hierarchy. Keep their tracks untouched;
                    // align only our synthetic viewer basis to the first sample.
                    auto source=buildScene(cameraDoc);codm::normalizeToCentimetres(source,scene.codmTranslationFactor/100.f);
                    if(!source.animations.empty()&&source.skeleton.bones.size()==1){
                        const auto local=source.sampleLocalPose(0,0).front();
                        auto& motion=scene.skeleton.bones[scene.skeleton.boneByName.at("codm_camera_motion")];motion.restLocal=local;motion.restGlobal=trs(local.position,local.rotation,local.scale);motion.inverseBind=inverseAffine(motion.restGlobal);
                        auto& viewer=scene.skeleton.bones[scene.skeleton.boneByName.at("tag_camera")];const auto relative=inverseAffine(motion.restGlobal)*viewer.restGlobal;decomposeAffine(relative,viewer.restLocal.position,viewer.restLocal.rotation,viewer.restLocal.scale);
                        scene.codmNativeCameraCalibrated=true;scene.warnings.push_back("CODM viewer camera neutral aligned to first camera sample; raw camera tracks retained: "+cameraPath.filename().string());
                    }
                }
                auto& action=scene.animations[before];for(auto track:camera.tracks)if(track.boneIndex==scene.skeleton.boneByName.at("tag_camera")){track.boneIndex=scene.skeleton.boneByName.at("codm_camera_motion");if(track.property<=TrackProperty::TranslationZ)for(auto& v:track.scalarValues)v*=scene.codmTranslationFactor;action.tracks.push_back(std::move(track));}
            }}
        }
    }
    if(scene.pointBlankNativeCentimetres&&pointblank::animationPath(document))for(size_t i=before;i<scene.animations.size();++i){
        auto& action=scene.animations[i];pointblank::classifyPlayer(action);for(auto& t:action.tracks)if(t.property<=TrackProperty::TranslationZ)for(auto& v:t.scalarValues)v*=100.f;
        if(pointblank::body(scene.skeleton))for(auto& t:action.tracks)if(t.boneIndex<scene.skeleton.bones.size()&&scene.skeleton.bones[t.boneIndex].parent<0){
            if(t.property==TrackProperty::TranslationX){t.property=TrackProperty::TranslationY;for(auto& v:t.scalarValues)v=-v;}
            else if(t.property==TrackProperty::TranslationY)t.property=TrackProperty::TranslationX;
            else if(t.property==TrackProperty::Rotation)for(auto& q:t.rotationValues)q=multiply(fromEulerRadians({0,0,-kPi*.5f}),q);
        }
        const auto path=std::filesystem::path(document.sourceName());const auto stem=path.stem().string();
        const auto prefix=scene.pointBlankWeaponStem+"_";
        if(!scene.pointBlankWeaponStem.empty()&&stem.starts_with(prefix)){
            const auto suffix=stem.substr(prefix.size());
            // Dual Desert Eagle exports pair fire with IDLE weapon mechanics.
            // Its single export contains the same mechanism's ATTACK resource.
            // Use only matching, animated child channels with equal idle binds;
            // never transfer the other model's gun root or hand placement.
            if(scene.pointBlankWeaponStem=="viewmodel_pistol_DesertEagle-Dual"&&(suffix=="Attack_Left"||suffix=="Attack_Right")){
                const auto loadMechanism=[&](const char* name)->std::optional<Animation>{
                    const auto file=path.parent_path()/name;std::error_code ec;if(!std::filesystem::is_regular_file(file,ec))return {};
                    const auto doc=cast::Document::load(file);for(const auto&r:doc.roots())for(const auto&n:r.children)if(n.identifier==kAnimation)return buildAnimation(doc,n,scene.skeleton,scene.warnings);return {};
                };
                const auto donor=loadMechanism("viewmodel_pistol_DesertEagle_Weapon _ ATTACK.cast"),idle=loadMechanism("viewmodel_pistol_DesertEagle_Weapon _ IDLE.cast");
                bool merged=false;
                if(donor&&idle&&donor->framerate==action.framerate){
                    for(auto t:donor->tracks){const auto& name=scene.skeleton.bones[t.boneIndex].name;
                        if(!name.starts_with("pb2cast_weapon__")||name=="pb2cast_weapon__GunDummy"||name.starts_with("pb2cast_weapon__left__"))continue;
                        if(t.property==TrackProperty::Rotation)continue; // only the verified linear slide stroke
                        if(t.property>TrackProperty::TranslationZ||t.scalarValues.size()<2)continue;
                        const auto [lo,hi]=std::minmax_element(t.scalarValues.begin(),t.scalarValues.end());if(*hi-*lo<.00001f)continue;
                        const auto ref=std::find_if(idle->tracks.begin(),idle->tracks.end(),[&](const auto&v){return v.boneIndex==t.boneIndex&&v.property==t.property;});if(ref==idle->tracks.end()||ref->scalarValues.empty())continue;
                        if(suffix=="Attack_Left"){auto b=scene.skeleton.boneByName.find("pb2cast_weapon__left__"+name.substr(16));if(b==scene.skeleton.boneByName.end())continue;t.boneIndex=b->second;}
                        const auto old=std::find_if(action.tracks.begin(),action.tracks.end(),[&](const auto&v){return v.boneIndex==t.boneIndex&&v.property==t.property;});if(old==action.tracks.end()||old->scalarValues.empty())continue;
                        const float base=old->scalarValues.front();if(std::abs(base-ref->scalarValues.front()*100.f)>.002f||std::any_of(old->scalarValues.begin(),old->scalarValues.end(),[&](float v){return std::abs(v-base)>.002f;}))continue;
                        for(auto&v:t.scalarValues)v*=100.f;*old=std::move(t);merged=true;
                    }
                }
                if(merged)scene.warnings.push_back("Point Blank dual Desert Eagle: matching ATTACK slide mechanism restored for "+suffix);
            }
            // These dual M9 exports share the dual-knife arm clips but contain
            // only static mechanism channels. Borrow the installed Kunai spin
            // relative to its idle reference, never its placement or arm roots.
            if((scene.pointBlankWeaponStem=="viewmodel_knife_M-9_Dual"||scene.pointBlankWeaponStem=="viewmodel_knife_M-9_Dual_PBNC")&&(suffix=="Change"||suffix=="Attack_A"||suffix=="Attack_B")){
                auto folder=path.parent_path();while(!folder.empty()&&folder.filename()!="animations"&&folder!=folder.root_path())folder=folder.parent_path();
                const auto donorFolder=folder/"viewmodel"/"kunai";const std::string donorStem="viewmodel_knife_Kunai_Dual_";
                const auto loadClip=[&](const std::filesystem::path& file)->std::optional<Animation>{std::error_code ec;if(!std::filesystem::is_regular_file(file,ec))return {};const auto doc=cast::Document::load(file);for(const auto&r:doc.roots())for(const auto&n:r.children)if(n.identifier==kAnimation)return buildAnimation(doc,n,scene.skeleton,scene.warnings);return {};};
                auto donor=loadClip(donorFolder/(donorStem+suffix+".cast")),donorIdle=loadClip(donorFolder/(donorStem+"AttackIdle.cast")),targetIdle=loadClip(path.parent_path()/(prefix+"AttackIdle.cast"));
                if(donor&&donorIdle&&targetIdle&&donor->durationFrames==action.durationFrames&&donor->framerate==action.framerate){
                    const auto findTrack=[](const Animation&a,size_t bone,TrackProperty prop)->const Track*{for(const auto&t:a.tracks)if(t.boneIndex==bone&&t.property==prop)return &t;return nullptr;};bool used=false;
                    for(const auto name:{"pb2cast_weapon__AnimationDummy","pb2cast_weapon__left__AnimationDummy"})if(const auto it=scene.skeleton.boneByName.find(name);it!=scene.skeleton.boneByName.end()){
                        const auto bone=it->second;const auto old=findTrack(action,bone,TrackProperty::Rotation),motion=findTrack(*donor,bone,TrackProperty::Rotation),from=findTrack(*donorIdle,bone,TrackProperty::Rotation),to=findTrack(*targetIdle,bone,TrackProperty::Rotation);
                        if(!old||!motion||!from||!to)continue;const auto first=rotationAt(*old,0);bool staticRotation=true;for(const auto&q:old->rotationValues)if(std::abs(first.x*q.x+first.y*q.y+first.z*q.z+first.w*q.w)<.99999f){staticRotation=false;break;}if(!staticRotation)continue;
                        const auto ref=rotationAt(*from,0),target=rotationAt(*to,0);const auto basis=multiply(target,Quat{-ref.x,-ref.y,-ref.z,ref.w});
                        auto replacement=*motion;for(auto&q:replacement.rotationValues)q=normalize(multiply(basis,q));
                        std::erase_if(action.tracks,[&](const auto&t){return t.boneIndex==bone&&t.property==TrackProperty::Rotation;});action.tracks.push_back(std::move(replacement));used=true;
                    }
                    if(used)scene.warnings.push_back("Point Blank M9: Kunai-relative spin fallback (original placement retained): "+suffix);
                }
            }
            // Some pb2cast action files were paired with Idle mechanics even
            // though an exact first-person mechanism companion was exported.
            // Only replace child-weapon channels: baked hand/socket roots stay authoritative.
            if(suffix.starts_with("Reload"))for(const auto& ending:{"_Male_1PV","_1PV","_Male"}){
                const auto companion=path.parent_path()/(prefix+"Weapon _ "+suffix+ending+".cast");std::error_code ec;
                if(!std::filesystem::is_regular_file(companion,ec))continue;
                const auto extra=cast::Document::load(companion);bool merged=false;
                for(const auto& root:extra.roots())for(const auto& node:root.children)if(node.identifier==kAnimation){
                    auto mechanics=buildAnimation(extra,node,scene.skeleton,scene.warnings);
                    if(mechanics.framerate!=action.framerate){scene.warnings.push_back("Point Blank mechanism FPS mismatch: "+companion.filename().string());continue;}
                    for(auto track:mechanics.tracks){const auto& b=scene.skeleton.bones[track.boneIndex];if(b.parent<0||!b.name.starts_with("pb2cast_weapon__"))continue;
                        if(track.property<=TrackProperty::TranslationZ)for(auto&v:track.scalarValues)v*=100.f;
                        std::erase_if(action.tracks,[&](const auto&t){return t.boneIndex==track.boneIndex&&t.property==track.property;});action.tracks.push_back(std::move(track));merged=true;
                    }
                }
                if(merged){scene.warnings.push_back("Point Blank paired mechanism: "+companion.filename().string());break;}
            }
            if(suffix=="AttackIdle"||suffix=="AttackIdle_Right"||suffix=="AttackIdle_Left"){
                // Rigid mesh nodes have baked mesh-space placement in these
                // exports, but often identity bind locals. Their authored idle
                // local is the skinning reference, not an extra mesh offset.
                const auto reference=scene.sampleLocalPose(i,0);
                std::vector<bool> rigid(scene.skeleton.bones.size(),false);
                for(const auto&m:scene.meshes)if(m.viewmodelWeapon)for(const auto&v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>.999f&&v.bones[k]<rigid.size())rigid[v.bones[k]]=true;
                for(size_t b=0;b<rigid.size();++b)if(rigid[b]&&scene.skeleton.bones[b].name.starts_with("pb2cast_weapon__")){
                    const bool left=scene.skeleton.bones[b].name.starts_with("pb2cast_weapon__left__");
                    if((suffix=="AttackIdle_Right"&&left)||(suffix=="AttackIdle_Left"&&!left))continue;
                    auto& bone=scene.skeleton.bones[b];const auto& ref=reference[b];
                    if(bone.parent<0||std::min({std::abs(ref.scale.x),std::abs(ref.scale.y),std::abs(ref.scale.z)})<.001f)continue;
                    bone.restLocal=ref;bone.restGlobal=scene.skeleton.bones[bone.parent].restGlobal*trs(ref.position,ref.rotation,ref.scale);bone.inverseBind=inverseAffine(bone.restGlobal);
                }
            }
        }
    }
    attachColdWarWorldPoses(document,scene,before);
    return scene.animations.size()-before;
}

void retargetAnimationRange(CastScene& scene,std::size_t firstAnimation,const Skeleton& sourceSkeleton){
    if(sourceSkeleton.bones.empty()||firstAnimation>=scene.animations.size())return;
    const bool isCs2Source = sourceSkeleton.boneByName.contains("armUpperShoulder_L") ||
                             sourceSkeleton.boneByName.contains("root_motion") ||
                             sourceSkeleton.boneByName.contains("wpn");

    const auto alignVectors = [](Vec3 u, Vec3 v) -> Quat {
        const float lu = length(u), lv = length(v);
        if(lu < 1e-4f || lv < 1e-4f) return Quat{0,0,0,1};
        u = u * (1.0f / lu); v = v * (1.0f / lv);
        const float d = u.x*v.x + u.y*v.y + u.z*v.z;
        if(d > 0.9999f) return Quat{0,0,0,1};
        if(d < -0.9999f) return Quat{0,0,1,0};
        const Vec3 c = { u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x };
        return normalize(Quat{ c.x, c.y, c.z, 1.0f + d });
    };

    for(std::size_t animationIndex=firstAnimation;animationIndex<scene.animations.size();++animationIndex)for(auto& track:scene.animations[animationIndex].tracks){
        if(scene.animations[animationIndex].coldWarWorldPose)continue;
        if(track.mode!=TrackMode::Absolute||track.boneIndex>=scene.skeleton.bones.size())continue;const auto& target=scene.skeleton.bones[track.boneIndex];
        const auto boneName=canonicalName(target.name);
        const bool isWeaponBone=boneName=="tag_weapon"||boneName=="tag_weapon_right"||boneName=="j_gun"||boneName=="j_gun1"||boneName.starts_with("jnt_")||boneName.starts_with("tag_clip")||boneName.starts_with("j_bolt")||boneName.starts_with("j_slide")||boneName.starts_with("j_pump")||boneName.starts_with("tag_brass")||boneName.starts_with("tag_flash");
        
        auto sourceIt=sourceSkeleton.boneByName.find(target.name);
        if(sourceIt==sourceSkeleton.boneByName.end()){
            const auto canonical=sourceSkeleton.boneByCanonicalName.find(boneName);
            if(canonical==sourceSkeleton.boneByCanonicalName.end())continue;
            sourceIt=sourceSkeleton.boneByName.find(sourceSkeleton.bones[canonical->second].name);
        }
        if(sourceIt==sourceSkeleton.boneByName.end())continue;
        const auto& source=sourceSkeleton.bones[sourceIt->second];

        // CS2 cross-gen: pin shoulder translations to target rest
        if(isCs2Source && (boneName=="j_shoulder_le"||boneName=="j_shoulder_ri")){
            if(track.property==TrackProperty::TranslationX) { track.scalarValues = { target.restLocal.position.x }; continue; }
            if(track.property==TrackProperty::TranslationY) { track.scalarValues = { target.restLocal.position.y }; continue; }
            if(track.property==TrackProperty::TranslationZ) { track.scalarValues = { target.restLocal.position.z }; continue; }
        }

        if(track.property==TrackProperty::Rotation){
            const Quat inverseSource{-source.restLocal.rotation.x,-source.restLocal.rotation.y,-source.restLocal.rotation.z,source.restLocal.rotation.w};
            for(auto& value:track.rotationValues){
                Quat r = normalize(multiply(target.restLocal.rotation,multiply(inverseSource,value)));
                if(isCs2Source && (boneName=="j_shoulder_le"||boneName=="j_shoulder_ri")){
                    Vec3 targetLimb{0,0,0}, sourceLimb{0,0,0};
                    for(const auto& b:scene.skeleton.bones) if(b.parent==static_cast<std::int32_t>(track.boneIndex)){ targetLimb=b.restLocal.position; break; }
                    for(const auto& b:sourceSkeleton.bones) if(b.parent==static_cast<std::int32_t>(sourceIt->second)){ sourceLimb=b.restLocal.position; break; }
                    if(length(targetLimb)>0.01f && length(sourceLimb)>0.01f){
                        r = normalize(multiply(alignVectors(targetLimb, sourceLimb), r));
                    }
                }
                value = r;
            }
            continue;
        }

        if(isWeaponBone && !isCs2Source) continue;

        if(track.scalarValues.empty())continue;
        float sourceValue{},targetValue{};
        switch(track.property){
            case TrackProperty::TranslationX:sourceValue=source.restLocal.position.x;targetValue=target.restLocal.position.x;break;
            case TrackProperty::TranslationY:sourceValue=source.restLocal.position.y;targetValue=target.restLocal.position.y;break;
            case TrackProperty::TranslationZ:sourceValue=source.restLocal.position.z;targetValue=target.restLocal.position.z;break;
            case TrackProperty::ScaleX:sourceValue=source.restLocal.scale.x;targetValue=target.restLocal.scale.x;break;
            case TrackProperty::ScaleY:sourceValue=source.restLocal.scale.y;targetValue=target.restLocal.scale.y;break;
            case TrackProperty::ScaleZ:sourceValue=source.restLocal.scale.z;targetValue=target.restLocal.scale.z;break;
            default:continue;
        }
        if(isCs2Source && isWeaponBone){
            for(auto& value:track.scalarValues) value = targetValue + (value - sourceValue);
            continue;
        }
        const bool scale=track.property==TrackProperty::ScaleX||track.property==TrackProperty::ScaleY||track.property==TrackProperty::ScaleZ;
        if(scale){
            const float ratio=targetValue/std::max(std::abs(sourceValue),1e-8f);
            for(auto& value:track.scalarValues)value*=ratio;
        }else{
            float motionScale=1.0f;
            const bool limb=boneName.starts_with("j_")&&containsAny(boneName,{"shoulder","elbow","wrist","thumb","index","mid","ring","pinky","webbing"});
            if(limb){
                const float sourceLength=length(source.restLocal.position),targetLength=length(target.restLocal.position);
                if(std::abs(sourceValue)>.01f&&std::abs(targetValue)>.01f){
                    const float signedRatio=targetValue/sourceValue;
                    motionScale=std::copysign(std::clamp(std::abs(signedRatio),.2f,5.0f),signedRatio);
                }else if(sourceLength>.01f&&targetLength>.01f)
                    motionScale=std::clamp(targetLength/sourceLength,.2f,5.0f);
            }
            for(auto& value:track.scalarValues)value=targetValue+(value-sourceValue)*motionScale;
        }
    }
}

struct MayaConstraintMapping {
    const char* driven;
    const char* driver;
    Vec3 t;
    Vec3 r;
};

static const MayaConstraintMapping kMayaConstraints[] = {
    {"camera_reference_meshes", "tag_view", {0.270165f, 0.000003f, -163.372406f}, {0.000000f, 0.000000f, 0.000000f}},
    {"j_elbow_le", "j_elbow_le", {-2.224869f, -0.002519f, -0.129647f}, {-3.070637f, 0.001668f, 0.015550f}},
    {"j_elbow_ri", "j_elbow_ri", {-2.233506f, 0.003864f, 0.126361f}, {0.071031f, 3.140120f, 0.015760f}},
    {"j_index_le_0", "j_index_le_1", {-0.383541f, -0.413714f, -0.343222f}, {0.059682f, -0.050520f, 0.063765f}},
    {"j_index_le_1", "j_index_le_2", {-0.462186f, 0.086389f, -0.117278f}, {0.086852f, -0.091844f, -0.037470f}},
    {"j_index_le_2", "j_index_le_3", {0.163465f, -0.139682f, 0.012896f}, {0.036243f, -0.059741f, 0.037463f}},
    {"j_index_ri_0", "j_index_ri_1", {-0.396616f, -0.418828f, 0.329254f}, {0.059733f, -0.050772f, -3.077979f}},
    {"j_index_ri_1", "j_index_ri_2", {-0.476571f, 0.087172f, 0.102503f}, {0.086810f, -0.092098f, 3.103978f}},
    {"j_index_ri_2", "j_index_ri_3", {0.151891f, -0.131010f, -0.028432f}, {0.036046f, -0.059906f, -3.104267f}},
    {"j_mid_le_0", "j_mid_le_1", {-0.343015f, -0.238707f, -0.010671f}, {0.042664f, 0.016166f, -0.076555f}},
    {"j_mid_le_1", "j_mid_le_2", {-0.839884f, 0.140871f, -0.090715f}, {-0.002062f, -0.034316f, -0.162767f}},
    {"j_mid_le_2", "j_mid_le_3", {-0.347654f, -0.340328f, 0.036641f}, {-0.081625f, -0.003496f, 0.171131f}},
    {"j_mid_ri_0", "j_mid_ri_1", {-0.355848f, -0.244769f, -0.003463f}, {0.042697f, 0.015951f, 3.064837f}},
    {"j_mid_ri_1", "j_mid_ri_2", {-0.852941f, 0.147765f, 0.075953f}, {-0.002209f, -0.034477f, 2.978629f}},
    {"j_mid_ri_2", "j_mid_ri_3", {-0.356953f, -0.329094f, -0.051970f}, {-0.081843f, -0.003503f, -2.970663f}},
    {"j_pinky_le_0", "j_pinky_le_1", {-0.875038f, 0.112373f, -0.360658f}, {0.074442f, -0.075558f, -0.157339f}},
    {"j_pinky_le_1", "j_pinky_le_2", {-0.783496f, 0.109566f, -0.089067f}, {0.062850f, -0.080220f, -0.241933f}},
    {"j_pinky_le_2", "j_pinky_le_3", {-0.675227f, -0.090631f, 0.091439f}, {-0.072395f, -0.052405f, 0.113233f}},
    {"j_pinky_ri_0", "j_pinky_ri_1", {-0.906877f, 0.111040f, 0.156321f}, {0.082224f, -0.012125f, 2.988975f}},
    {"j_pinky_ri_1", "j_pinky_ri_2", {-0.788500f, 0.132959f, 0.112999f}, {0.106440f, -0.038851f, 2.940231f}},
    {"j_pinky_ri_2", "j_pinky_ri_3", {-0.600182f, -0.006145f, 0.025468f}, {-0.011021f, -0.061204f, -3.018688f}},
    {"j_pinkypalm_le", "j_metapinky_le_1", {0.126760f, 0.355500f, -0.726299f}, {0.225462f, -0.114937f, -0.204285f}},
    {"j_pinkypalm_ri", "j_metapinky_ri_1", {0.118828f, 0.345078f, 0.712517f}, {0.330587f, -0.141134f, 2.930054f}},
    {"j_ring_le_0", "j_ring_le_1", {-0.488878f, -0.507123f, -0.358616f}, {0.116293f, 0.009390f, -0.058140f}},
    {"j_ring_le_1", "j_ring_le_2", {-0.785584f, -0.265260f, -0.399138f}, {0.107358f, -0.073082f, -0.010819f}},
    {"j_ring_le_2", "j_ring_le_3", {-0.164385f, -0.254925f, -0.142080f}, {0.024052f, -0.033415f, 0.180423f}},
    {"j_ring_ri_0", "j_ring_ri_1", {-0.533911f, -0.549029f, 0.484853f}, {0.003318f, -0.076016f, 3.060199f}},
    {"j_ring_ri_1", "j_ring_ri_2", {-0.933290f, -0.313114f, 0.156569f}, {-0.031827f, -0.039594f, -3.141562f}},
    {"j_ring_ri_2", "j_ring_ri_3", {-0.288703f, -0.187563f, 0.017300f}, {-0.053510f, -0.015006f, -3.010894f}},
    {"j_ringpalm_le", "j_metaring_le_1", {0.334608f, 0.255780f, -0.545856f}, {0.368020f, 0.025807f, -0.112681f}},
    {"j_ringpalm_ri", "j_metaring_ri_1", {0.326617f, 0.244296f, 0.533169f}, {0.367762f, 0.047170f, 3.022175f}},
    {"j_shoulder_le", "j_shoulder_le", {-2.903568f, 0.826454f, -1.706336f}, {-2.056588f, -0.051319f, 0.026046f}},
    {"j_shoulder_ri", "j_shoulder_ri", {-2.904110f, 0.828856f, 1.706707f}, {1.084916f, 3.193095f, 0.026261f}},
    {"j_thumb_le_0", "j_thumb_le_1", {0.487192f, -1.950192f, -1.061925f}, {2.682098f, -0.181567f, 0.291676f}},
    {"j_thumb_le_1", "j_thumb_le_2", {-0.316723f, -0.514288f, -0.317292f}, {2.991630f, -0.195700f, 0.099687f}},
    {"j_thumb_le_2", "j_thumb_le_3", {0.480672f, -0.186218f, 0.182032f}, {2.922476f, 0.025157f, 0.130932f}},
    {"j_thumb_ri_0", "j_thumb_ri_1", {0.469669f, -1.952540f, 1.064162f}, {2.682156f, -0.181558f, -2.849636f}},
    {"j_thumb_ri_1", "j_thumb_ri_2", {-0.331425f, -0.504733f, 0.321840f}, {2.991594f, -0.195774f, -3.041613f}},
    {"j_thumb_ri_2", "j_thumb_ri_3", {0.470629f, -0.170933f, -0.177409f}, {2.922415f, 0.025103f, -3.010376f}},
    {"j_wrist_le", "j_wrist_le", {-0.883195f, 0.454046f, -0.178026f}, {-2.970538f, -0.151172f, -0.198075f}},
    {"j_wrist_ri", "j_wrist_ri", {-0.891933f, 0.467417f, 0.170187f}, {-2.970421f, -0.151347f, 2.943710f}},
    {"j_wristtwist_le", "j_wristfronttwist1_le", {-7.547511f, 0.332980f, -0.165067f}, {-3.070637f, 0.001668f, 0.015550f}},
    {"j_wristtwist_ri", "j_wristfronttwist1_ri", {-7.556365f, 0.344717f, 0.158802f}, {0.071031f, 3.140120f, 0.015760f}},
    {"tag_ads", "tag_ads", {0.000000f, 0.000000f, 0.000000f}, {0.000000f, 0.000000f, 0.000000f}},
    {"tag_cambone", "tag_cambone", {0.000000f, 0.000000f, 0.000000f}, {0.000000f, 0.000000f, 0.000000f}},
    {"tag_camera", "tag_camera", {0.000000f, 0.000000f, 0.000000f}, {0.000000f, 0.000000f, 0.000000f}},
    {"tag_torso", "tag_torso", {0.000000f, 0.000000f, 0.000000f}, {0.000000f, 0.000000f, 0.000000f}},
    {"tag_view", "tag_view", {0.000000f, 0.000000f, 0.000000f}, {0.000000f, 0.000000f, 0.000000f}},
    {"tag_weapon", "tag_weapon", {0.000000f, 0.000000f, 0.000000f}, {0.000000f, 0.000000f, 0.000000f}},
    {"wristtwist_back_le", "j_elbow_le", {13.970004f, 0.000022f, -0.000035f}, {-1.585016f, 0.002693f, -0.005999f}},
    {"wristtwist_back_ri", "j_elbow_ri", {-13.969899f, 0.000040f, 0.000013f}, {-1.556385f, -0.002602f, 3.135652f}},
};

static Mat4 makeMayaOffset(Vec3 t, Vec3 rRad) {
    const float cx = std::cos(rRad.x * 0.5f), sx = std::sin(rRad.x * 0.5f);
    const float cy = std::cos(rRad.y * 0.5f), sy = std::sin(rRad.y * 0.5f);
    const float cz = std::cos(rRad.z * 0.5f), sz = std::sin(rRad.z * 0.5f);
    const Quat qx{sx, 0, 0, cx}, qy{0, sy, 0, cy}, qz{0, 0, sz, cz};
    const Quat q = normalize(multiply(qz, multiply(qy, qx)));
    return trs(t, q, {1,1,1});
}

bool composeIwSprintOffset(CastScene& scene,std::size_t animation,std::size_t idle){
    if(animation>=scene.animations.size()||idle>=scene.animations.size()||animation==idle)return false;
    auto& clip=scene.animations[animation];
    if(clip.sourceName.find("sprint_offset")==std::string::npos)return false;
    const auto offsets=clip.tracks;const auto base=scene.sampleLocalPose(idle,0);
    const auto duration=std::max(1u,scene.animations[idle].durationFrames);
    std::vector<std::vector<Transform>> baseFrames;for(std::uint32_t f=0;f<=duration;++f)baseFrames.push_back(scene.sampleLocalPose(idle,static_cast<float>(f)));
    std::vector<Track> tracks;
    for(std::size_t bone=0;bone<base.size();++bone){
        for(const auto property:{TrackProperty::TranslationX,TrackProperty::TranslationY,TrackProperty::TranslationZ,TrackProperty::Rotation}){
            Track t;t.boneIndex=bone;t.property=property;
            for(std::uint32_t frame=0;frame<=duration;++frame){
                auto local=baseFrames[frame][bone];
                for(const auto& offset:offsets)if(offset.boneIndex==bone){
                    if(offset.property==TrackProperty::Rotation)local.rotation=normalize(multiply(local.rotation,rotationAt(offset,static_cast<float>(frame))));
                    else if(offset.property==TrackProperty::TranslationX)local.position.x+=scalarAt(offset,static_cast<float>(frame));
                    else if(offset.property==TrackProperty::TranslationY)local.position.y+=scalarAt(offset,static_cast<float>(frame));
                    else if(offset.property==TrackProperty::TranslationZ)local.position.z+=scalarAt(offset,static_cast<float>(frame));
                }
                t.frames.push_back(frame);
                if(property==TrackProperty::Rotation)t.rotationValues.push_back(local.rotation);
                else t.scalarValues.push_back(property==TrackProperty::TranslationX?local.position.x:property==TrackProperty::TranslationY?local.position.y:local.position.z);
            }
            tracks.push_back(std::move(t));
        }
    }
    clip.tracks=std::move(tracks);clip.durationFrames=duration;clip.framerate=scene.animations[idle].framerate;clip.looping=scene.animations[idle].durationFrames==0||scene.animations[idle].looping;return true;
}

void retargetAnimationRange(CastScene& scene, std::size_t firstAnimation, const CastScene& sourceScene, std::size_t firstSourceAnimation,bool inverseIwCalibration) {
    if (scene.skeleton.bones.empty() || sourceScene.skeleton.bones.empty() || firstAnimation >= scene.animations.size() || firstSourceAnimation >= sourceScene.animations.size()) return;

    std::unordered_map<std::string_view, const MayaConstraintMapping*> mappingByName;
    // First inverse match wins: elbow is the primary driver, not a secondary twist helper.
    for (const auto& m : kMayaConstraints) {
        if(inverseIwCalibration){
            if(std::string_view(m.driven)=="camera_reference_meshes")continue;
            mappingByName.try_emplace(m.driver,&m);
        }
        else mappingByName[m.driven] = &m;
    }
    if (auto it = mappingByName.find("tag_weapon"); !inverseIwCalibration&&it != mappingByName.end()) {
        mappingByName["tag_weapon_right"] = it->second;
    }

    const std::size_t animCount = std::min(scene.animations.size() - firstAnimation, sourceScene.animations.size() - firstSourceAnimation);
    for (std::size_t a = 0; a < animCount; ++a) {
        auto& targetAnim = scene.animations[firstAnimation + a];
        if(targetAnim.coldWarWorldPose)continue;
        const auto& srcAnim = sourceScene.animations[firstSourceAnimation + a];
        const std::uint32_t duration = srcAnim.durationFrames;

        struct TargetBoneTracks {
            Track tx, ty, tz, rot, sx, sy, sz;
            bool active{};
        };
        std::vector<TargetBoneTracks> baked(scene.skeleton.bones.size());
        for (std::size_t i = 0; i < scene.skeleton.bones.size(); ++i) {
            auto& bt = baked[i];
            bt.tx.boneIndex = bt.ty.boneIndex = bt.tz.boneIndex = bt.rot.boneIndex = i;
            bt.tx.property = TrackProperty::TranslationX;
            bt.ty.property = TrackProperty::TranslationY;
            bt.tz.property = TrackProperty::TranslationZ;
            bt.rot.property = TrackProperty::Rotation;
            bt.sx.boneIndex=bt.sy.boneIndex=bt.sz.boneIndex=i;
            bt.sx.property=TrackProperty::ScaleX;bt.sy.property=TrackProperty::ScaleY;bt.sz.property=TrackProperty::ScaleZ;
            bt.sx.mode=bt.sy.mode=bt.sz.mode=TrackMode::Absolute;
            bt.tx.mode = bt.ty.mode = bt.tz.mode = bt.rot.mode = TrackMode::Absolute;
            bt.tx.additiveWeight = bt.ty.additiveWeight = bt.tz.additiveWeight = bt.rot.additiveWeight = 1.0f;
        }

        std::vector<bool> boneRetargeted(scene.skeleton.bones.size(), false);
        std::vector<int> sourceDrivers(scene.skeleton.bones.size(), -1);

        for (std::uint32_t f = 0; f <= duration; ++f) {
            const auto srcGlobal = sourceScene.samplePose(firstSourceAnimation + a, static_cast<float>(f));
            std::vector<Mat4> targetGlobal(scene.skeleton.bones.size());

            for (std::size_t i = 0; i < scene.skeleton.bones.size(); ++i) {
                const auto& bone = scene.skeleton.bones[i];
                auto mapIt = mappingByName.find(bone.name);
                if (mapIt == mappingByName.end()) {
                    mapIt = mappingByName.find(canonicalName(bone.name));
                }
                if (mapIt != mappingByName.end()) {
                    auto srcBoneIt = sourceScene.skeleton.boneByName.find(inverseIwCalibration?mapIt->second->driven:mapIt->second->driver);
                    if (srcBoneIt != sourceScene.skeleton.boneByName.end()) {
                        const auto offset=makeMayaOffset(mapIt->second->t, mapIt->second->r);
                        targetGlobal[i] = srcGlobal[srcBoneIt->second] * (inverseIwCalibration?inverseAffine(offset):offset);
                        boneRetargeted[i] = true;
                        sourceDrivers[i] = static_cast<int>(srcBoneIt->second);
                        continue;
                    }
                }
                auto srcIt = sourceScene.skeleton.boneByName.find(bone.name);
                if (srcIt == sourceScene.skeleton.boneByName.end()) {
                    const auto canon = canonicalName(bone.name);
                    const auto canonIt = sourceScene.skeleton.boneByCanonicalName.find(canon);
                    if (canonIt != sourceScene.skeleton.boneByCanonicalName.end()) {
                        srcIt = sourceScene.skeleton.boneByName.find(sourceScene.skeleton.bones[canonIt->second].name);
                    }
                }
                // Generic body aliases (e.g. j_spinelower -> tag_torso) do not
                // represent equivalent IW deform frames. Keep their native chain.
                if (srcIt != sourceScene.skeleton.boneByName.end() &&
                    (!inverseIwCalibration||canonicalName(sourceScene.skeleton.bones[srcIt->second].name)==canonicalName(bone.name))) {
                    targetGlobal[i] = srcGlobal[srcIt->second];
                    boneRetargeted[i] = true;
                    sourceDrivers[i] = static_cast<int>(srcIt->second);
                    continue;
                }
                const Mat4 l = trs(bone.restLocal.position, bone.restLocal.rotation, bone.restLocal.scale);
                targetGlobal[i] = (bone.parent >= 0) ? (targetGlobal[bone.parent] * l) : l;
            }

            if(inverseIwCalibration){
                // Legacy viewhands have no clavicle track. Recover that parent
                // from the solved shoulder and its native bind relation, rather
                // than stretching weighted collar vertices toward a static torso.
                for(const auto side:{"le","ri"}){
                    const auto clavicle=scene.skeleton.boneByName.find(std::string("j_clavicle_")+side);
                    const auto shoulder=scene.skeleton.boneByName.find(std::string("j_shoulder_")+side);
                    if(clavicle==scene.skeleton.boneByName.end()||shoulder==scene.skeleton.boneByName.end())continue;
                    const auto& child=scene.skeleton.bones[shoulder->second];
                    if(child.parent!=static_cast<std::int32_t>(clavicle->second))continue;
                    targetGlobal[clavicle->second]=targetGlobal[shoulder->second]*inverseAffine(trs(child.restLocal.position,child.restLocal.rotation,child.restLocal.scale));
                    boneRetargeted[clavicle->second]=true;
                    sourceDrivers[clavicle->second]=sourceDrivers[shoulder->second];
                }
                for(std::size_t i=0;i<scene.skeleton.bones.size();++i)if(!boneRetargeted[i]){
                    const auto& b=scene.skeleton.bones[i];const auto local=trs(b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);
                    targetGlobal[i]=b.parent>=0?targetGlobal[b.parent]*local:local;
                    // Bake these too: an imported alias track must not overwrite
                    // the native helper/spine relation after solving the globals.
                    baked[i].active=true;
                }
            }

            for (std::size_t i = 0; i < scene.skeleton.bones.size(); ++i) {
                if (!boneRetargeted[i]&&!inverseIwCalibration) continue;
                const auto& bone = scene.skeleton.bones[i];
                const Mat4 localM = (bone.parent >= 0) ? (inverseAffine(targetGlobal[bone.parent]) * targetGlobal[i]) : targetGlobal[i];
                Vec3 pos, scale;
                Quat rot;
                decomposeAffine(localM, pos, rot, scale);

                auto& bt = baked[i];
                bt.active = true;
                bt.tx.frames.push_back(f); bt.tx.scalarValues.push_back(pos.x);
                bt.ty.frames.push_back(f); bt.ty.scalarValues.push_back(pos.y);
                bt.tz.frames.push_back(f); bt.tz.scalarValues.push_back(pos.z);
                bt.rot.frames.push_back(f); bt.rot.rotationValues.push_back(rot);
                if(inverseIwCalibration){
                    bt.sx.frames.push_back(f);bt.sx.scalarValues.push_back(scale.x);
                    bt.sy.frames.push_back(f);bt.sy.scalarValues.push_back(scale.y);
                    bt.sz.frames.push_back(f);bt.sz.scalarValues.push_back(scale.z);
                }
            }
        }

        std::vector<Track> finalTracks;
        {
            std::vector<bool> authored(sourceScene.skeleton.bones.size(),false);
            for(const auto& track:srcAnim.tracks)if(track.ownsLayer&&track.boneIndex<authored.size())authored[track.boneIndex]=true;
            for(std::size_t i=0;i<baked.size();++i){
                // A target local transform depends on the source paths between
                // its driver and its parent's driver. Shared ancestors cancel.
                // This also includes an IW clavicle omitted by legacy rigs.
                int parent=scene.skeleton.bones[i].parent;
                while(parent>=0&&sourceDrivers[parent]<0)parent=scene.skeleton.bones[parent].parent;
                std::vector<int> childPath,parentPath;
                for(int j=sourceDrivers[i];j>=0;j=sourceScene.skeleton.bones[j].parent)childPath.push_back(j);
                for(int j=parent>=0?sourceDrivers[parent]:-1;j>=0;j=sourceScene.skeleton.bones[j].parent)parentPath.push_back(j);
                while(!childPath.empty()&&!parentPath.empty()&&childPath.back()==parentPath.back()){childPath.pop_back();parentPath.pop_back();}
                bool owns=false;
                for(const auto j:childPath)owns=owns||authored[j];
                for(const auto j:parentPath)owns=owns||authored[j];
                auto& b=baked[i];b.tx.ownsLayer=b.ty.ownsLayer=b.tz.ownsLayer=b.rot.ownsLayer=b.sx.ownsLayer=b.sy.ownsLayer=b.sz.ownsLayer=owns;
            }
        }
        for (auto& track : targetAnim.tracks) {
            if (!inverseIwCalibration && track.boneIndex < boneRetargeted.size() && !boneRetargeted[track.boneIndex]) {
                finalTracks.push_back(std::move(track));
            }
        }
        for (std::size_t i = 0; i < scene.skeleton.bones.size(); ++i) {
            if (baked[i].active) {
                finalTracks.push_back(std::move(baked[i].tx));
                finalTracks.push_back(std::move(baked[i].ty));
                finalTracks.push_back(std::move(baked[i].tz));
                finalTracks.push_back(std::move(baked[i].rot));
                if(inverseIwCalibration){finalTracks.push_back(std::move(baked[i].sx));finalTracks.push_back(std::move(baked[i].sy));finalTracks.push_back(std::move(baked[i].sz));}
            }
        }
        targetAnim.tracks = std::move(finalTracks);
        targetAnim.durationFrames = srcAnim.durationFrames;
        targetAnim.framerate = srcAnim.framerate;
        targetAnim.looping = srcAnim.looping;
        targetAnim.notifications = srcAnim.notifications;
        targetAnim.viewmodelCameraReference=srcAnim.viewmodelCameraReference;
        if(!targetAnim.viewmodelCameraReference){
            const auto camera=sourceScene.skeleton.boneByCanonicalName.find("tag_camera");
            if(camera!=sourceScene.skeleton.boneByCanonicalName.end())targetAnim.viewmodelCameraReference=sourceScene.skeleton.bones[camera->second].restGlobal;
        }
    }
}

std::optional<float> mechanismReadyFrame(const CastScene& scene,std::size_t animationIndex,std::string_view boneToken) {
    if(animationIndex>=scene.animations.size())return std::nullopt;
    const auto& animation=scene.animations[animationIndex];
    if(animation.durationFrames==0||animation.tracks.empty())return std::nullopt;
    const auto wanted=canonicalName(std::string(boneToken));
    std::vector<std::size_t> mechanismBones;
    for(const auto& track:animation.tracks){
        if(track.boneIndex>=scene.skeleton.bones.size())continue;
        const auto name=canonicalName(scene.skeleton.bones[track.boneIndex].name);
        if(name.find(wanted)==std::string::npos)continue;
        if(std::find(mechanismBones.begin(),mechanismBones.end(),track.boneIndex)==mechanismBones.end())mechanismBones.push_back(track.boneIndex);
    }
    if(mechanismBones.empty())return std::nullopt;
    const auto finalPose=scene.sampleLocalPose(animationIndex,static_cast<float>(animation.durationFrames));
    if(finalPose.size()!=scene.skeleton.bones.size())return std::nullopt;
    std::size_t lastDifferent{};
    bool moved{};
    constexpr float kPositionTolerance=1.0e-4f;
    constexpr float kRotationToleranceRadians=0.05f*(kPi/180.0f);
    for(std::size_t frame=0;frame<=animation.durationFrames;++frame){
        const auto pose=scene.sampleLocalPose(animationIndex,static_cast<float>(frame));
        if(pose.size()!=finalPose.size())return std::nullopt;
        bool differs{};
        for(const auto bone:mechanismBones){
            const auto& p=pose[bone].position;const auto& finalP=finalPose[bone].position;
            const auto& q=pose[bone].rotation;const auto& finalQ=finalPose[bone].rotation;
            const float cosine=std::clamp(std::abs(q.x*finalQ.x+q.y*finalQ.y+q.z*finalQ.z+q.w*finalQ.w),0.0f,1.0f);
            const float angle=2.0f*std::acos(cosine);
            if(length(p-finalP)>kPositionTolerance||angle>kRotationToleranceRadians){differs=true;break;}
        }
        if(differs){moved=true;lastDifferent=frame;}
    }
    if(!moved)return std::nullopt;
    return static_cast<float>(std::min<std::size_t>(animation.durationFrames,lastDifferent+1));
}

std::optional<Vec3> resolveMuzzlePosition(const CastScene& value,const std::vector<Mat4>& pose) {
    if(value.pointBlankNativeCentimetres)if(auto it=value.skeleton.boneByName.find("pb2cast_weapon__FXDummy");it!=value.skeleton.boneByName.end()&&it->second<pose.size()){
        auto matrix=pose[it->second];
        // A shared animation can contain the unsuppressed FXDummy offset even
        // on a suppressed model. Keep the model's barrel attachment in its
        // animated parent's space, just like the barrel geometry itself.
        if(it->second<value.skeleton.bones.size()){
            const auto& socket=value.skeleton.bones[it->second];const auto parent=socket.parent;
            if(parent>=0&&static_cast<std::size_t>(parent)<pose.size()&&static_cast<std::size_t>(parent)<value.skeleton.bones.size())
                matrix=pose[parent]*inverseAffine(value.skeleton.bones[parent].restGlobal)*socket.restGlobal;
        }
        const auto p=transformPoint(matrix,{});if(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z))return p;
    }
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    for(const auto& attachment:value.attachments)if(attachment.muzzleLocal&&attachment.boneIndex<pose.size()){
        const auto p=transformPoint(pose[attachment.boneIndex]*attachment.localMatrix(),*attachment.muzzleLocal);
        if(finite(p))return p;
    }
    // Source exports name this bone simply 'muzzle'. Brass and weapon-root bones
    // are deliberately excluded: neither identifies the end of the barrel.
    for(const char* name:{"muzzle","muzzle_flash","attach_muzzle","weapon_muzzle","muzzle_1","tag_flash","tag_flash_1","tag_flash1","tag_flash2","tag_flash_silenced","tag_muzzle","tag_barrel","j_barrel"}){
        const auto found=value.skeleton.boneByCanonicalName.find(name);
        if(found==value.skeleton.boneByCanonicalName.end()||found->second>=pose.size())continue;
        const auto& m=pose[found->second];const Vec3 p{m.v[12],m.v[13],m.v[14]};
        if(finite(p))return p;
    }
    return std::nullopt;
}

std::size_t appendAttachment(const cast::Document& document,CastScene& scene,std::size_t boneIndex,std::string name) {
    if(boneIndex>=scene.skeleton.bones.size()||!document.valid())return 0;
    auto imported=buildScene(document);
    return appendPreparedAttachment(std::move(imported),scene,boneIndex,std::move(name));
}

std::size_t appendPreparedAttachment(CastScene&& imported,CastScene& scene,std::size_t boneIndex,std::string name) {
    if(boneIndex>=scene.skeleton.bones.size())return 0;
    if(imported.meshes.empty())return 0;
    Attachment attachment;attachment.name=std::move(name);attachment.boneIndex=boneIndex;
    std::vector<Mat4> importedPose;for(const auto& bone:imported.skeleton.bones)importedPose.push_back(bone.restGlobal);
    attachment.muzzleLocal=resolveMuzzlePosition(imported,importedPose);
    attachment.firstMesh=scene.meshes.size();attachment.meshCount=imported.meshes.size();
    const auto attachmentIndex=static_cast<std::int32_t>(scene.attachments.size());
    for(auto& mesh:imported.meshes){
        mesh.skinned=false;mesh.attachmentIndex=attachmentIndex;
        for(auto& vertex:mesh.vertices){vertex.bones={};vertex.weights={1,0,0,0};}
        scene.meshes.push_back(std::move(mesh));
    }
    for(auto& warning:imported.warnings)scene.warnings.push_back("Attachment '"+attachment.name+"': "+warning);
    scene.attachments.push_back(std::move(attachment));
    return imported.meshes.size();
}

bool sameRigGeometry(const CastScene& a,const CastScene& b){
    if(a.meshes.empty()||a.meshes.size()!=b.meshes.size()||a.skeleton.bones.size()!=b.skeleton.bones.size())return false;
    for(std::size_t i=0;i<a.skeleton.bones.size();++i){
        const auto& x=a.skeleton.bones[i];const auto& y=b.skeleton.bones[i];
        if(x.name!=y.name||x.parent!=y.parent||x.restGlobal.v!=y.restGlobal.v||x.inverseBind.v!=y.inverseBind.v)return false;
    }
    const auto equal3=[](Vec3 x,Vec3 y){return x.x==y.x&&x.y==y.y&&x.z==y.z;};
    for(std::size_t i=0;i<a.meshes.size();++i){
        const auto& x=a.meshes[i];const auto& y=b.meshes[i];
        if(x.skinned!=y.skinned||x.modelTransform.v!=y.modelTransform.v||x.indices!=y.indices||x.vertices.size()!=y.vertices.size())return false;
        for(std::size_t j=0;j<x.vertices.size();++j){
            const auto& p=x.vertices[j];const auto& q=y.vertices[j];
            if(!equal3(p.position,q.position)||!equal3(p.normal,q.normal)||p.uv.x!=q.uv.x||p.uv.y!=q.uv.y||p.bones!=q.bones||p.weights!=q.weights)return false;
        }
    }
    return true;
}

std::size_t appendRigModel(const cast::Document& document,CastScene& scene,std::string name,std::optional<Mat4> sourceMount,float uniformScale){
    (void)sourceMount;
    if(!document.valid())return 0;auto imported=buildScene(document);if(imported.meshes.empty())return 0;
    if(std::abs(uniformScale-1.0f)>1e-4f){
        for(auto& mesh:imported.meshes)for(auto& vertex:mesh.vertices)vertex.position=vertex.position*uniformScale;
        for(auto& bone:imported.skeleton.bones){
            bone.restLocal.position=bone.restLocal.position*uniformScale;
            // Scale within the imported model's own space, before socket parenting.
            // Native (unit-scale) rigs never enter this branch.
            for(const auto component:{12,13,14}){
                bone.restGlobal.v[component]*=uniformScale;
                bone.inverseBind.v[component]*=uniformScale;
            }
        }
    }
    const std::string weaponProfile=scene.rigParts.empty()?std::string{}:canonicalName(scene.rigParts.front().name);
    const std::string partProfile=canonicalName(name);const bool preT6WeaponPart=partProfile.starts_with("t5_")||partProfile.find("_iw5")!=std::string::npos;
    // The IW revolver view export omits the usual _vm marker. It still owns
    // its j_gun root: merging it with the native hands' wrist helper makes
    // cross-game retargeting copy that helper's pose into the actual weapon.
    const bool iwRevolverPart=partProfile=="weapon_revolver_camo"||partProfile=="weapon_revolver_camo_lod0";
    const bool iw7WeaponPart=(partProfile.starts_with("weapon_")&&partProfile.find("_vm")!=std::string::npos)||iwRevolverPart;const bool awWeaponPart=partProfile.starts_with("vm_")||iw7WeaponPart;
    const bool iw5AttachmentPart=!scene.rigParts.empty()&&partProfile.starts_with("viewmodel_")&&containsAny(partProfile,{"scope","acog","eotech","reflex","reticle","magnifier","silencer","foregrip","thermal"});
    const bool coldWarPart=partProfile.starts_with("wpn_t9_")&&partProfile.find("_view")!=std::string::npos;
    const bool meleeKnifePart=canonicalName(name).find("wpn_knife_base_view")!=std::string::npos;
    const bool compositeHeadPart=partProfile.starts_with("head_")&&!containsAny(partProfile,{"juggernaut","jugger"});
    const bool awCharacterPart=partProfile.starts_with("mp_")&&!partProfile.starts_with("mp_view_")&&
        (partProfile.starts_with("mp_top_")||partProfile.starts_with("mp_pants_")||partProfile.starts_with("mp_head")||partProfile.starts_with("mp_glove")||partProfile.starts_with("mp_boot_")||partProfile.starts_with("mp_kneepad_")||partProfile.starts_with("mp_shinguard_")||partProfile.starts_with("mp_loadouts_")||partProfile.starts_with("mp_exo_")||partProfile.starts_with("mp_eyewear_"));
    if(coldWarPart&&std::any_of(imported.skeleton.bones.begin(),imported.skeleton.bones.end(),[](const auto& b){return b.parent<0&&canonicalName(b.name)=="tag_weapon";})){
        // Legacy hands already have a tag_weapon socket with a character-space
        // inverse bind. The T9 receiver owns a zero-space tag_weapon bone;
        // sharing the socket's inverse bind tears the receiver off its parts.
        if(const auto slot=scene.skeleton.boneByCanonicalName.find("tag_weapon");slot!=scene.skeleton.boneByCanonicalName.end()&&scene.rigParts.empty()){
            const auto index=slot->second;auto& b=scene.skeleton.bones[index];scene.skeleton.boneByName.erase(b.name);scene.skeleton.boneByCanonicalName.erase(slot);
            b.name="t9_hands|tag_weapon_mount";scene.skeleton.boneByName[b.name]=index;scene.skeleton.boneByCanonicalName["tag_weapon_mount"]=index;
        }
    }
    std::vector<float> rootMinimumZ(imported.skeleton.bones.size(),std::numeric_limits<float>::max());
    std::vector<float> rootMinimumY(imported.skeleton.bones.size(),std::numeric_limits<float>::max()),rootMaximumY(imported.skeleton.bones.size(),std::numeric_limits<float>::lowest());
    for(const auto& mesh:imported.meshes)for(const auto& vertex:mesh.vertices)for(std::size_t influence=0;influence<vertex.bones.size();++influence){const auto root=vertex.bones[influence];if(root>=imported.skeleton.bones.size()||imported.skeleton.bones[root].parent>=0||vertex.weights[influence]<0.5f)continue;
        rootMinimumZ[root]=std::min(rootMinimumZ[root],vertex.position.z);rootMinimumY[root]=std::min(rootMinimumY[root],vertex.position.y);rootMaximumY[root]=std::max(rootMaximumY[root],vertex.position.y);}
    if(iw7WeaponPart||partProfile.starts_with("t6_wpn_"))if(const auto handGun=scene.skeleton.boneByCanonicalName.find("j_gun");handGun!=scene.skeleton.boneByCanonicalName.end()&&scene.skeleton.bones[handGun->second].parent>=0&&canonicalName(scene.skeleton.bones[static_cast<std::size_t>(scene.skeleton.bones[handGun->second].parent)].name)=="j_wrist_ri"){const auto index=handGun->second;const auto oldName=scene.skeleton.bones[index].name;scene.skeleton.boneByName.erase(oldName);scene.skeleton.boneByCanonicalName.erase("j_gun");scene.skeleton.bones[index].name="iw7_hands|j_gun_hand";scene.skeleton.boneByName[scene.skeleton.bones[index].name]=index;scene.skeleton.boneByCanonicalName["j_gun_hand"]=index;}
    std::vector<std::size_t> boneMap(imported.skeleton.bones.size());std::vector<std::size_t> partRoots;
    for(std::size_t i=0;i<imported.skeleton.bones.size();++i){const auto& source=imported.skeleton.bones[i];auto canonical=canonicalName(source.name);if(meleeKnifePart&&source.parent<0&&canonical=="j_gun")canonical="j_gun1";
        const bool xprScopePart=partProfile.find("xpr50_scope_view")!=std::string::npos;const auto existing=scene.skeleton.boneByCanonicalName.find(canonical);const bool independentAttachmentRoot=(iw5AttachmentPart||xprScopePart)&&source.parent<0;
        if(existing!=scene.skeleton.boneByCanonicalName.end()&&!independentAttachmentRoot){boneMap[i]=existing->second;continue;}
        Bone bone=source;if(source.parent<0&&partProfile.find("xpr50_view")!=std::string::npos&&canonical=="j_gun")bone.restLocal.position={};if(xprScopePart&&source.parent<0){bone.restLocal.position={};bone.restLocal.rotation={};bone.restLocal.scale={1,1,1};}if(meleeKnifePart&&source.parent<0&&canonical=="j_gun1")bone.name="j_gun1";else if(independentAttachmentRoot)bone.name=name+"|"+source.name;const auto sourceCanonical=canonical;
        // BO2 pistol clip channels are authored as tiny offsets around the
        // model's non-zero bind socket. Treating them as absolute translations
        // nudges the B23R/FNP45 magazine out of its magwell.
        // T5/IW5 bolt tags export near zero at rest and a local pull distance
        // while their model bind socket is non-zero. Treating that channel as
        // absolute collapses an L96-style bolt down toward the gun origin.
        // T6 uses different authored semantics, so keep this pre-T6-specific.
        bone.translationTracksAreDeltas=!awCharacterPart&&source.parent>=0&&(sourceCanonical.starts_with("j_")||(awWeaponPart&&sourceCanonical.starts_with("jnt_"))||sourceCanonical=="tag_clip"||sourceCanonical=="tag_pump"||(preT6WeaponPart&&sourceCanonical.starts_with("tag_bolt")));
        // T6 embedded magazines share bind-relative translations, including
        // incoming tag_clip_full (Five-Seven/FAL/SIG), tag_clip1 (AN94/HAMR),
        // and their left-hand counterparts. Do not discard the parked bind
        // origin or rebase a reload endpoint: both destroy hand contact.
        if(partProfile.starts_with("t6_wpn_")&&partProfile.find("_view")!=std::string::npos&&sourceCanonical.starts_with("tag_clip")&&source.parent>=0)
            bone.translationTracksAreDeltas=true;
        if(source.parent>=0&&static_cast<std::size_t>(source.parent)<i)bone.parent=static_cast<std::int32_t>(boneMap[static_cast<std::size_t>(source.parent)]);
        else if(source.parent<0)if(const auto socket=[&]() -> std::optional<std::size_t>{
            if(partProfile.starts_with("t6_wpn_")&&partProfile.find("_view_lh")!=std::string::npos&&canonical=="j_gun1"){
                for(const auto name:{"tag_weapon1","tag_weapon_left"})if(const auto it=scene.skeleton.boneByCanonicalName.find(name);it!=scene.skeleton.boneByCanonicalName.end())return it->second;
            }
            if(coldWarPart){const auto target=canonical=="tag_weapon"?(scene.skeleton.boneByCanonicalName.contains("tag_weapon_mount")?"tag_weapon_mount":"tag_weapon_right"):canonical=="tag_clip"?"tag_weapon":canonical=="tag_scope"?"tag_rail":"";
                if(const auto it=scene.skeleton.boneByCanonicalName.find(target);it!=scene.skeleton.boneByCanonicalName.end())return it->second;}
            return xprScopePart&&scene.skeleton.boneByCanonicalName.contains("tag_rail")?std::optional<std::size_t>{scene.skeleton.boneByCanonicalName.at("tag_rail")}:rigSocketForRoot(scene.skeleton,canonical);
        }()){
            bone.parent=static_cast<std::int32_t>(*socket);
            if(canonical=="tag_clip"&&!coldWarPart){
                // Separate BO2 view magazines omit their weapon-specific bind
                // offset. Reconstruct the magwell station from the release when
                // available, otherwise from the receiver bolt plus the common
                // six-unit forward separation observed in these T6 rigs.
                const auto release=scene.skeleton.boneByCanonicalName.find("j_mag_release"),bolt=scene.skeleton.boneByCanonicalName.find("j_bolt");
                // The separate magazine's vertices are authored around their
                // own zero root; tag_clip1 is packed into weapon-model space.
                // They must not share a rest transform even though both curves
                // participate in the same reload.
                const bool t6Magazine=(partProfile.starts_with("t6_attach_mag_")||partProfile.starts_with("t6_attach_fastmag_"))&&partProfile.find("_view")!=std::string::npos;
                // T6's missing attachment station is recovered from native
                // reload contact at assembly time, never from a bolt guess.
                if(t6Magazine){
                    bone.restLocal.position=source.restLocal.position;
                } else if(release!=scene.skeleton.boneByCanonicalName.end()){
                    Vec3 position,scale;Quat rotation;decomposeAffine(inverseAffine(scene.skeleton.bones[*socket].restGlobal)*scene.skeleton.bones[release->second].restGlobal,position,rotation,scale);bone.restLocal.position=position;
                } else if(bolt!=scene.skeleton.boneByCanonicalName.end()){
                    Vec3 position,scale;Quat rotation;decomposeAffine(inverseAffine(scene.skeleton.bones[*socket].restGlobal)*scene.skeleton.bones[bolt->second].restGlobal,position,rotation,scale);bone.restLocal.position=position+Vec3{6.0f,0,0};
                }
                bone.translationTracksAreDeltas=true;
                // Absolute tag_clip curves are authored around the weapon
                // origin even though the separate magazine model mounts at a
                // non-zero magwell socket. Reapply that socket to tag_clip.
                // tag_clip1 belongs to geometry packed into the weapon and its
                // absolute tracks are already authored against its own bind;
                // offsetting clip1 is what pushed the AN94 hand magazine ahead.
                bone.absoluteTranslationOffset=bone.restLocal.position;
                if(const auto swap=scene.skeleton.boneByCanonicalName.find("tag_clip1");swap!=scene.skeleton.boneByCanonicalName.end())scene.skeleton.bones[swap->second].absoluteTranslationOffset={};
            }
            const bool optic=isOpticRoot(canonical);
            if(optic&&weaponProfile.find("an94")!=std::string::npos){
                // Verified T6 AttachmentUnique view offset for the AN-94 rail.
                // Share it across optic types so cross-weapon optics use one station.
                bone.restLocal.position={10.2f,0.0f,-3.0f};
            } else if(optic&&weaponProfile.find("dsr50")!=std::string::npos){
                // Verified receiver station for DSR-50 view optics.
                bone.restLocal.position={-17.019f,0.0f,12.3f};
            } else if(optic&&canonicalName(scene.skeleton.bones[*socket].name)=="tag_scope_rail"){
                // A few exported rail tags retain a lateral model-space offset
                // (notably SVU). AttachmentUnique offsets center the optic.
                bone.restLocal.position.y=-scene.skeleton.bones[*socket].restLocal.position.y;
                bone.restLocal.position.z=10.2f;
            } else if(optic&&canonicalName(scene.skeleton.bones[*socket].name)=="j_gun"){
                const auto bolt=scene.skeleton.boneByCanonicalName.find("j_bolt"),handle=scene.skeleton.boneByCanonicalName.find("j_handle"),sights=scene.skeleton.boneByCanonicalName.find("tag_sights");
                if(bolt!=scene.skeleton.boneByCanonicalName.end()){bone.restLocal.position=scene.skeleton.bones[bolt->second].restLocal.position+Vec3{4.0f,0,0};bone.restLocal.position.y=0;}
                if(handle!=scene.skeleton.boneByCanonicalName.end()&&bolt!=scene.skeleton.boneByCanonicalName.end())bone.restLocal.position.x=(scene.skeleton.bones[handle->second].restLocal.position.x+scene.skeleton.bones[bolt->second].restLocal.position.x)*0.5f;
                if(bolt!=scene.skeleton.boneByCanonicalName.end()&&rootMinimumZ[i]!=std::numeric_limits<float>::max())bone.restLocal.position.z=scene.skeleton.bones[bolt->second].restLocal.position.z+2.0f-rootMinimumZ[i];
                else if(sights!=scene.skeleton.boneByCanonicalName.end())bone.restLocal.position.z=scene.skeleton.bones[sights->second].restLocal.position.z;
                if(rootMinimumY[i]!=std::numeric_limits<float>::max())bone.restLocal.position.y=-(rootMinimumY[i]+rootMaximumY[i])*0.5f;
            }
        }
        if(awCharacterPart&&(canonical=="j_hip_le"||canonical=="j_hip_ri")){
            // Clothing exports flatten the identity pelvis helper. Native AW
            // tops retain it; restore the parent without changing local bind.
            if(const auto pelvis=scene.skeleton.boneByCanonicalName.find("pelvis");pelvis!=scene.skeleton.boneByCanonicalName.end())bone.parent=static_cast<std::int32_t>(pelvis->second);
        }
        if(compositeHeadPart||awCharacterPart){
            // Modular MW3 heads are exported in spine-local space. Shared
            // j_head/j_neck bones already use the body's bind, while newly
            // added jaw/eye bones must follow that same remapped hierarchy.
            // Otherwise weighted head vertices mix body-space corrections
            // with identity corrections from the source-space facial binds.
            const auto local=trs(bone.restLocal.position,bone.restLocal.rotation,bone.restLocal.scale);
            bone.restGlobal=bone.parent>=0&&static_cast<std::size_t>(bone.parent)<scene.skeleton.bones.size()?scene.skeleton.bones[bone.parent].restGlobal*local:local;
            bone.inverseBind=inverseAffine(bone.restGlobal);
        }
        const auto index=scene.skeleton.bones.size();boneMap[i]=index;scene.skeleton.boneByName.try_emplace(bone.name,index);scene.skeleton.boneByCanonicalName.emplace(canonical,index);scene.skeleton.bones.push_back(std::move(bone));if(source.parent<0)partRoots.push_back(index);
    }
    const auto partName=canonicalName(name);if(partName.find("attach_mag_")!=std::string::npos||partName.find("fastmag")!=std::string::npos){
        const auto clip=scene.skeleton.boneByCanonicalName.find("tag_clip");if(clip!=scene.skeleton.boneByCanonicalName.end())scene.skeleton.bones[clip->second].absoluteTranslationOffset=scene.skeleton.bones[clip->second].restLocal.position;
        const auto swap=scene.skeleton.boneByCanonicalName.find("tag_clip1");if(swap!=scene.skeleton.boneByCanonicalName.end())scene.skeleton.bones[swap->second].absoluteTranslationOffset={};
    }
    const bool viewmodelWeaponPart=scene.skeleton.boneByCanonicalName.contains("tag_view");const auto before=scene.meshes.size();for(auto& mesh:imported.meshes){
        if((compositeHeadPart||awCharacterPart)&&mesh.skinned)for(auto& vertex:mesh.vertices){const auto sourcePosition=vertex.position,sourceNormal=vertex.normal;Vec3 correctedPosition{},correctedNormal{};float totalWeight{};for(std::size_t influence=0;influence<vertex.bones.size();++influence){const auto sourceBone=vertex.bones[influence];const float weight=vertex.weights[influence];if(weight<=0||sourceBone>=boneMap.size()||sourceBone>=imported.skeleton.bones.size()||boneMap[sourceBone]>=scene.skeleton.bones.size())continue;const auto correction=scene.skeleton.bones[boneMap[sourceBone]].restGlobal*imported.skeleton.bones[sourceBone].inverseBind;correctedPosition=correctedPosition+transformPoint(correction,sourcePosition)*weight;const Vec3 transformedNormal{correction.v[0]*sourceNormal.x+correction.v[4]*sourceNormal.y+correction.v[8]*sourceNormal.z,correction.v[1]*sourceNormal.x+correction.v[5]*sourceNormal.y+correction.v[9]*sourceNormal.z,correction.v[2]*sourceNormal.x+correction.v[6]*sourceNormal.y+correction.v[10]*sourceNormal.z};correctedNormal=correctedNormal+transformedNormal*weight;totalWeight+=weight;}if(totalWeight>0.0001f){vertex.position=correctedPosition/totalWeight;vertex.normal=normalize(correctedNormal);}}
        if(mesh.skinned)for(auto& vertex:mesh.vertices)for(auto& bone:vertex.bones)if(bone<boneMap.size())bone=static_cast<std::uint32_t>(boneMap[bone]);
        mesh.name=name+" / "+mesh.name;mesh.attachmentIndex=-1;mesh.viewmodelWeapon=viewmodelWeaponPart;scene.meshes.push_back(std::move(mesh));
    }
    scene.rigParts.push_back({name,before,scene.meshes.size()-before,std::move(partRoots)});
    for(auto& warning:imported.warnings)scene.warnings.push_back("Rig model '"+name+"': "+warning);
    return scene.meshes.size()-before;
}

std::optional<std::size_t> findBestAnimation(const CastScene& scene,const AnimationQuery& query) {
    std::optional<std::size_t> best;int bestScore=std::numeric_limits<int>::min();
    for(std::size_t i=0;i<scene.animations.size();++i){const auto& clip=scene.animations[i];
        if(query.requireMappedTracks&&clip.tracks.empty())continue;
        if(clip.contextual&&!query.allowContextual)continue;
        if(clip.action!=query.action)continue;
        if(query.domain!=AnimationDomain::Other&&clip.domain!=query.domain)continue;
        const bool longGun=query.weapon==WeaponClass::Rifle||query.weapon==WeaponClass::Sniper||query.weapon==WeaponClass::Automatic||query.weapon==WeaponClass::Shotgun||
            query.weapon==WeaponClass::M1216||query.weapon==WeaponClass::Judge||query.weapon==WeaponClass::G11||query.weapon==WeaponClass::LMG||
            query.weapon==WeaponClass::Crossbow||query.weapon==WeaponClass::BallisticKnife;
        if(query.action==ActionRole::None){
            if(query.motion!=MotionRole::Unknown&&clip.motion!=query.motion)continue;
            if(query.stance!=Stance::Any&&clip.stance!=Stance::Any&&clip.stance!=query.stance&&(query.motion!=MotionRole::Slide||clip.motion!=MotionRole::Slide))continue;
            const bool compatible=query.weapon==clip.weapon||(query.weapon!=WeaponClass::Any&&clip.weapon==WeaponClass::Any)||(longGun&&clip.weapon==WeaponClass::Rifle);
            if(!compatible)continue;
        }
        if(query.action!=ActionRole::None&&query.stance!=Stance::Any&&clip.stance!=Stance::Any&&clip.stance!=query.stance)continue;
        if(query.action!=ActionRole::None){
            if(query.weapon==WeaponClass::Any&&clip.weapon!=WeaponClass::Any)continue;
            const bool sharedAutomatic=query.action==ActionRole::Fire&&query.weapon==WeaponClass::LMG&&clip.weapon==WeaponClass::Automatic;
            const bool rifleEquipFamily=query.weapon==WeaponClass::Sniper||query.weapon==WeaponClass::Automatic||query.weapon==WeaponClass::Shotgun||
                query.weapon==WeaponClass::M1216||query.weapon==WeaponClass::G11||query.weapon==WeaponClass::LMG||query.weapon==WeaponClass::Crossbow;
            const bool sharedLongGunAction=(query.action==ActionRole::Equip||query.action==ActionRole::Unequip)&&rifleEquipFamily&&clip.weapon==WeaponClass::Rifle;
            const bool strictFamily=query.action==ActionRole::Fire||query.action==ActionRole::Reload||query.action==ActionRole::Equip||query.action==ActionRole::FirstRaise||query.action==ActionRole::Unequip;
            if(query.weapon!=WeaponClass::Any&&clip.weapon!=query.weapon&&!sharedAutomatic&&!sharedLongGunAction&&(strictFamily||clip.weapon!=WeaponClass::Any))continue;
        }
        if(query.action==ActionRole::Fire||query.action==ActionRole::Reload){
            const bool automaticLmgFire=query.action==ActionRole::Fire&&query.weapon==WeaponClass::LMG&&clip.weapon==WeaponClass::Automatic;
            if(query.weapon==WeaponClass::Any||(clip.weapon!=query.weapon&&!automaticLmgFire))continue;
            if(query.action==ActionRole::Fire&&clip.ads!=query.ads)continue;
            if(query.action==ActionRole::Reload&&query.reloadStyle!=ReloadStyle::Any&&clip.reloadStyle!=query.reloadStyle)continue;
            const bool combinedLocomotion=clip.motion==MotionRole::Walk||clip.motion==MotionRole::Run||clip.motion==MotionRole::Sprint||clip.motion==MotionRole::Crawl;
            if(combinedLocomotion&&query.motion!=clip.motion)continue;
        }
        if(query.action==ActionRole::Death&&query.direction!=Direction::Any&&clip.direction!=Direction::Any&&clip.direction!=query.direction)continue;
        int score=100;
        if(query.motion!=MotionRole::Unknown)score+=clip.motion==query.motion?60:-18;
        if(query.stance!=Stance::Any)score+=clip.stance==query.stance?35:(clip.stance==Stance::Any?8:-14);
        if(query.direction!=Direction::Any)score+=clip.direction==query.direction?45:(clip.direction==Direction::Any?10:-16);
        else if(clip.direction==Direction::Any)score+=4;
        const bool sharedLongGunPose=query.action==ActionRole::None&&clip.weapon==WeaponClass::Rifle&&longGun;
        const bool sharedEquipPose=(query.action==ActionRole::Equip||query.action==ActionRole::Unequip)&&clip.weapon==WeaponClass::Rifle&&
            (query.weapon==WeaponClass::Sniper||query.weapon==WeaponClass::Automatic||query.weapon==WeaponClass::Shotgun||query.weapon==WeaponClass::M1216||
             query.weapon==WeaponClass::G11||query.weapon==WeaponClass::LMG||query.weapon==WeaponClass::Crossbow);
        if(query.weapon!=WeaponClass::Any)score+=clip.weapon==query.weapon?50:((sharedLongGunPose||sharedEquipPose)?30:(clip.weapon==WeaponClass::Any?10:-18));
        else if(clip.weapon==WeaponClass::Any)score+=5;
        if(query.action==ActionRole::None)score+=clip.ads==query.ads?8:-8;
        if(query.action==ActionRole::None&&clip.looping)score+=6;
        score+=static_cast<int>(std::min<std::size_t>(clip.tracks.size(),20));
        score-=static_cast<int>(std::min<std::size_t>(clip.unmappedCurveCount,20));
        if(score>bestScore){best=i;bestScore=score;}
    }
    return best;
}

std::vector<Transform> CastScene::sampleLocalPose(std::size_t animationIndex,float frame) const {
    std::vector<Transform> local;sampleLocalPoseInto(animationIndex,frame,local);return local;
}
void CastScene::sampleLocalPoseInto(std::size_t animationIndex,float frame,std::vector<Transform>& local) const {
    if(animationIndex<animations.size()&&animations[animationIndex].coldWarWorldPose)
        {local=animations[animationIndex].coldWarWorldPose->sample(skeleton,frame);return;}
    local.clear();local.reserve(skeleton.bones.size());
    for(const auto& bone:skeleton.bones)local.push_back(bone.restLocal);
    if(animationIndex<animations.size()) {
        const auto& animation=animations[animationIndex];
        if(animation.looping&&animation.durationFrames>0)frame=std::fmod(std::max(0.0f,frame),static_cast<float>(animation.durationFrames));
        else frame=std::clamp(frame,0.0f,static_cast<float>(animation.durationFrames));
        for(const auto& track:animation.tracks) {
            if(track.boneIndex>=local.size())continue;
            auto& value=local[track.boneIndex]; const auto& rest=skeleton.bones[track.boneIndex].restLocal;
            if(track.property==TrackProperty::Rotation) {
                const auto sampled=rotationAt(track,frame);
                if(track.mode==TrackMode::Absolute){
                    value.rotation=sampled;
                    if(const auto mount=rigMountReferences.find(track.boneIndex);mount!=rigMountReferences.end()){
                        const auto r=mount->second.rotation;
                        value.rotation=normalize(multiply(multiply(rest.rotation,Quat{-r.x,-r.y,-r.z,r.w}),sampled));
                    }
                }
                else if(track.mode==TrackMode::Relative)value.rotation=normalize(multiply(rest.rotation,sampled));
                else value.rotation=normalize(multiply(value.rotation,slerp(Quat{},sampled,track.additiveWeight)));
            } else {
                float *target{},restValue{};
                switch(track.property){
                case TrackProperty::TranslationX:target=&value.position.x;restValue=rest.position.x;break;
                case TrackProperty::TranslationY:target=&value.position.y;restValue=rest.position.y;break;
                case TrackProperty::TranslationZ:target=&value.position.z;restValue=rest.position.z;break;
                case TrackProperty::ScaleX:target=&value.scale.x;restValue=rest.scale.x;break;
                case TrackProperty::ScaleY:target=&value.scale.y;restValue=rest.scale.y;break;
                case TrackProperty::ScaleZ:target=&value.scale.z;restValue=rest.scale.z;break;
                default:break;
                }
                if(target){
                    applyScalar(*target,restValue,scalarAt(track,frame),track.mode,track.additiveWeight);
                    if(track.mode==TrackMode::Absolute)if(const auto mount=rigMountReferences.find(track.boneIndex);mount!=rigMountReferences.end()){
                        if(track.property==TrackProperty::TranslationX)*target+=rest.position.x-mount->second.position.x;
                        if(track.property==TrackProperty::TranslationY)*target+=rest.position.y-mount->second.position.y;
                        if(track.property==TrackProperty::TranslationZ)*target+=rest.position.z-mount->second.position.z;
                    }
                }
            }
        }
    }
}

std::vector<Mat4> CastScene::globalPose(const std::vector<Transform>& localPose) const {
    std::vector<Mat4> globals(localPose.size());
    for(std::size_t i=0;i<localPose.size();++i){
        const auto matrix=trs(localPose[i].position,localPose[i].rotation,localPose[i].scale);
        const auto parent=skeleton.bones[i].parent;
        globals[i]=parent>=0?globals[parent]*matrix:matrix;
        if(codmNativeCentimetres)for(const auto& [target,source]:nativePoseFollowers)if(target==i&&source<i){globals[i]=globals[source];break;}
        if(codmRigAdapter&&i>=codmRigAdapter->firstBone&&i-codmRigAdapter->firstBone<codmRigAdapter->bindings.size()){
            const auto& b=codmRigAdapter->bindings[i-codmRigAdapter->firstBone];if(b.source<i){globals[i]=globals[b.source]*b.offset;if(b.rollSource<i&&b.rollWeight>0){Vec3 p0,p1,s0,s1;Quat q0,q1;decomposeAffine(globals[i],p0,q0,s0);decomposeAffine(globals[b.rollSource]*b.rollOffset,p1,q1,s1);globals[i]=trs(lerp(p0,p1,b.rollWeight),slerp(q0,q1,b.rollWeight),lerp(s0,s1,b.rollWeight));}}
        }
    }
    return globals;
}

std::vector<Mat4> CastScene::samplePose(std::size_t animationIndex,float frame) const {
    return globalPose(sampleLocalPose(animationIndex,frame));
}

void restoreT6MagazineAfterReload(const CastScene& scene,std::vector<Mat4>& pose,std::size_t targetAnimation,float targetFrame) {
    if(pose.size()!=scene.skeleton.bones.size()||targetAnimation>=scene.animations.size())return;
    if(std::none_of(scene.rigParts.begin(),scene.rigParts.end(),[](const auto& part){return part.name.starts_with("t6_wpn_")&&part.name.find("_view")!=std::string::npos;}))return;
    std::vector<std::size_t> installed,dummies;
    const auto addPair=[&](const char* clipName,const char* dummyName){
        const auto clip=scene.skeleton.boneByCanonicalName.find(clipName),dummy=scene.skeleton.boneByCanonicalName.find(dummyName);
        if(clip==scene.skeleton.boneByCanonicalName.end()||dummy==scene.skeleton.boneByCanonicalName.end())return;
        const auto& c=scene.skeleton.bones[clip->second];const auto& d=scene.skeleton.bones[dummy->second];
        // Dual-wield tag_clip1 is a real installed magazine on another gun,
        // not an incoming dummy. Only pair parked siblings on the same gun.
        if(c.parent!=d.parent||length(c.restLocal.position-d.restLocal.position)<20.0f)return;
        installed.push_back(clip->second);dummies.push_back(dummy->second);
    };
    addPair("tag_clip","tag_clip_full");addPair("tag_clip1","tag_clip_full1");
    addPair("tag_clip","tag_clip1");
    if(dummies.empty())return;
    const auto local=scene.sampleLocalPose(targetAnimation,targetFrame);
    const auto descends=[&](std::size_t bone,std::size_t root){for(std::size_t count=0;count<pose.size();++count){if(bone==root)return true;const auto parent=scene.skeleton.bones[bone].parent;if(parent<0)return false;bone=static_cast<std::size_t>(parent);}return false;};
    for(std::size_t i=0;i<pose.size();++i){
        if(std::any_of(dummies.begin(),dummies.end(),[&](auto root){return descends(i,root);}))pose[i]=trs({},Quat{},Vec3{0,0,0});
        else if(std::any_of(installed.begin(),installed.end(),[&](auto root){return descends(i,root);})){
            const auto parent=scene.skeleton.bones[i].parent;
            pose[i]=(parent>=0?pose[parent]:Mat4::identity())*trs(local[i].position,local[i].rotation,local[i].scale);
        }
    }
}

std::vector<Mat4> CastScene::sampleBlendedPose(std::size_t fromAnimation,float fromFrame,
                                               std::size_t toAnimation,float toFrame,float alpha) const {
    return globalPose(sampleBlendedLocalPose(fromAnimation,fromFrame,toAnimation,toFrame,alpha));
}
std::vector<Transform> CastScene::sampleBlendedLocalPose(std::size_t fromAnimation,float fromFrame,
                                               std::size_t toAnimation,float toFrame,float alpha) const {
    auto from=sampleLocalPose(fromAnimation,fromFrame),to=sampleLocalPose(toAnimation,toFrame);
    alpha=std::clamp(alpha,0.0f,1.0f);alpha=alpha*alpha*(3.0f-2.0f*alpha);
    const auto count=std::min(from.size(),to.size());
    for(std::size_t i=0;i<count;++i){
        from[i].position=lerp(from[i].position,to[i].position,alpha);
        from[i].rotation=slerp(from[i].rotation,to[i].rotation,alpha);
        from[i].scale=lerp(from[i].scale,to[i].scale,alpha);
    }
    return from;
}

std::vector<Mat4> CastScene::sampleLayeredPose(std::size_t baseAnimation,float baseFrame,
                                               std::size_t layerAnimation,float layerFrame,
                                               float weight,LayerMode mode,bool suppressRootMotion,const std::vector<Transform>* baseOverride) const {
    auto base=baseOverride?*baseOverride:sampleLocalPose(baseAnimation,baseFrame);
    if(layerAnimation>=animations.size()||base.size()!=skeleton.bones.size())return globalPose(base);
    pose_detail::Lease lease;auto& buffer=lease.workspace; if(buffer.samples.empty())buffer.samples.resize(1);
    auto& sample=buffer.samples[0];sampleLocalPoseInto(layerAnimation,layerFrame,sample.pose);
    if(mode==LayerMode::Additive)sampleLocalPoseInto(layerAnimation,0.0f,sample.reference);
    const auto& layer=sample.pose;const auto& reference=sample.reference;weight=std::clamp(weight,0.0f,1.0f);
    auto& animated=sample.channels;pose_detail::markChannels(animations[layerAnimation],skeleton,animated);
    for(std::size_t i=0;i<base.size()&&i<layer.size();++i){
        if(suppressRootMotion&&skeleton.bones[i].parent<0)continue;
        const auto& channels=animated[i];
        if(mode==LayerMode::Override){
            if(channels.tx)base[i].position.x=base[i].position.x+(layer[i].position.x-base[i].position.x)*weight;
            if(channels.ty)base[i].position.y=base[i].position.y+(layer[i].position.y-base[i].position.y)*weight;
            if(channels.tz)base[i].position.z=base[i].position.z+(layer[i].position.z-base[i].position.z)*weight;
            if(channels.rotation)base[i].rotation=slerp(base[i].rotation,layer[i].rotation,weight);
            if(channels.sx)base[i].scale.x=base[i].scale.x+(layer[i].scale.x-base[i].scale.x)*weight;
            if(channels.sy)base[i].scale.y=base[i].scale.y+(layer[i].scale.y-base[i].scale.y)*weight;
            if(channels.sz)base[i].scale.z=base[i].scale.z+(layer[i].scale.z-base[i].scale.z)*weight;
            continue;
        }
        const auto& origin=reference[i];
        if(channels.tx)base[i].position.x+=(layer[i].position.x-origin.position.x)*weight;
        if(channels.ty)base[i].position.y+=(layer[i].position.y-origin.position.y)*weight;
        if(channels.tz)base[i].position.z+=(layer[i].position.z-origin.position.z)*weight;
        if(channels.rotation){
            const Quat inverseOrigin{-origin.rotation.x,-origin.rotation.y,-origin.rotation.z,origin.rotation.w};
            const auto rotationDelta=normalize(multiply(inverseOrigin,layer[i].rotation));
            base[i].rotation=normalize(multiply(base[i].rotation,slerp(Quat{},rotationDelta,weight)));
        }
        if(channels.sx){const float ratio=layer[i].scale.x/std::max(std::abs(origin.scale.x),1e-8f);base[i].scale.x*=1.0f+(ratio-1.0f)*weight;}
        if(channels.sy){const float ratio=layer[i].scale.y/std::max(std::abs(origin.scale.y),1e-8f);base[i].scale.y*=1.0f+(ratio-1.0f)*weight;}
        if(channels.sz){const float ratio=layer[i].scale.z/std::max(std::abs(origin.scale.z),1e-8f);base[i].scale.z*=1.0f+(ratio-1.0f)*weight;}
    }
    return globalPose(base);
}

std::vector<Mat4> CastScene::sampleLayerStack(std::size_t baseAnimation,float baseFrame,const std::vector<PoseLayer>& layers) const {
    auto base=sampleLocalPose(baseAnimation,baseFrame);pose_detail::Lease lease;auto& buffer=lease.workspace;
    if(buffer.samples.empty())buffer.samples.resize(1);auto& sample=buffer.samples[0];
    for(const auto& entry:layers){if(entry.animation>=animations.size()||entry.weight<=0)continue;
        sampleLocalPoseInto(entry.animation,entry.frame,sample.pose);sampleLocalPoseInto(entry.animation,0.0f,sample.reference);
        const auto& layer=sample.pose;const auto& reference=sample.reference;const float weight=std::clamp(entry.weight,0.0f,1.0f);
        auto& animated=sample.channels;pose_detail::markChannels(animations[entry.animation],skeleton,animated);
        for(std::size_t i=0;i<base.size()&&i<layer.size();++i){if(entry.suppressRootMotion&&skeleton.bones[i].parent<0)continue;const auto& channels=animated[i];const auto& origin=reference[i];if(entry.mode==LayerMode::Override){if(channels.tx)base[i].position.x+=(layer[i].position.x-base[i].position.x)*weight;if(channels.ty)base[i].position.y+=(layer[i].position.y-base[i].position.y)*weight;if(channels.tz)base[i].position.z+=(layer[i].position.z-base[i].position.z)*weight;if(channels.rotation)base[i].rotation=slerp(base[i].rotation,layer[i].rotation,weight);if(channels.sx)base[i].scale.x+=(layer[i].scale.x-base[i].scale.x)*weight;if(channels.sy)base[i].scale.y+=(layer[i].scale.y-base[i].scale.y)*weight;if(channels.sz)base[i].scale.z+=(layer[i].scale.z-base[i].scale.z)*weight;continue;}
            if(channels.tx)base[i].position.x+=(layer[i].position.x-origin.position.x)*weight;if(channels.ty)base[i].position.y+=(layer[i].position.y-origin.position.y)*weight;if(channels.tz)base[i].position.z+=(layer[i].position.z-origin.position.z)*weight;if(channels.rotation){const Quat inverseOrigin{-origin.rotation.x,-origin.rotation.y,-origin.rotation.z,origin.rotation.w};const auto delta=normalize(multiply(inverseOrigin,layer[i].rotation));base[i].rotation=normalize(multiply(base[i].rotation,slerp(Quat{},delta,weight)));}if(channels.sx)base[i].scale.x*=1.0f+(layer[i].scale.x/std::max(std::abs(origin.scale.x),1e-8f)-1.0f)*weight;if(channels.sy)base[i].scale.y*=1.0f+(layer[i].scale.y/std::max(std::abs(origin.scale.y),1e-8f)-1.0f)*weight;if(channels.sz)base[i].scale.z*=1.0f+(layer[i].scale.z/std::max(std::abs(origin.scale.z),1e-8f)-1.0f)*weight;}}
    return globalPose(base);
}

std::vector<Transform> CastScene::sampleLocalPoseSlots(std::size_t baseAnimation,float baseFrame,const std::vector<PoseSlot>& slots) const {
    auto base=sampleLocalPose(baseAnimation,baseFrame);
    using pose_detail::Channels;pose_detail::Lease lease;auto& buffer=lease.workspace;
    for(const auto& slot:slots){
        if(buffer.samples.size()<slot.nodes.size())buffer.samples.resize(slot.nodes.size());
        std::size_t count=0;
        for(const auto& node:slot.nodes){
            if(node.animation>=animations.size()||node.weight<=0)continue;
            auto& sample=buffer.samples[count++];sample.node=&node;
            sampleLocalPoseInto(node.animation,node.frame,sample.pose);
            if(node.mode==LayerMode::Additive)sampleLocalPoseInto(node.referenceAnimation<animations.size()?node.referenceAnimation:node.animation,node.referenceAnimation<animations.size()?node.referenceFrame:0,sample.reference);
            sample.weight=std::max(0.0f,node.weight);
            pose_detail::markChannels(animations[node.animation],skeleton,sample.channels,node.preserveWeaponMechanisms,node.preserveViewmodelAimRoot);
        }
        const std::span<pose_detail::Sample> samples(buffer.samples.data(),count);
        if(samples.empty())continue;
        for(std::size_t bone=0;bone<base.size();++bone){
            const auto channelWeight=[&](auto member){float total{};for(const auto& sample:samples)if(sample.channels[bone].*member&&!(sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)&&sample.node->mode==LayerMode::Override)total+=sample.weight;return total;};
            const auto blendScalar=[&](float& target,auto member,auto value){const float total=channelWeight(member);if(total<=0)return;const float norm=std::max(1.0f,total);float result=target*(1.0f-std::min(1.0f,total));for(const auto& sample:samples)if(sample.channels[bone].*member&&!(sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)&&sample.node->mode==LayerMode::Override)result+=value(sample.pose[bone])*(sample.weight/norm);target=result;};
            blendScalar(base[bone].position.x,&Channels::tx,[](const Transform& value){return value.position.x;});
            blendScalar(base[bone].position.y,&Channels::ty,[](const Transform& value){return value.position.y;});
            blendScalar(base[bone].position.z,&Channels::tz,[](const Transform& value){return value.position.z;});
            blendScalar(base[bone].scale.x,&Channels::sx,[](const Transform& value){return value.scale.x;});
            blendScalar(base[bone].scale.y,&Channels::sy,[](const Transform& value){return value.scale.y;});
            blendScalar(base[bone].scale.z,&Channels::sz,[](const Transform& value){return value.scale.z;});
            const float rotationWeight=channelWeight(&Channels::rotation);if(rotationWeight>0){const float norm=std::max(1.0f,rotationWeight);Quat result=base[bone].rotation;float accumulated=std::max(0.0f,1.0f-std::min(1.0f,rotationWeight));for(const auto& sample:samples)if(sample.channels[bone].rotation&&!(sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)&&sample.node->mode==LayerMode::Override){const float contribution=sample.weight/norm;result=slerp(result,sample.pose[bone].rotation,contribution/std::max(1e-8f,accumulated+contribution));accumulated+=contribution;}base[bone].rotation=result;}
            for(const auto& sample:samples){if(sample.node->mode!=LayerMode::Additive||sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)continue;const auto& owned=sample.channels[bone];const auto& value=sample.pose[bone];const auto& origin=sample.reference[bone];const float weight=std::clamp(sample.weight,0.0f,1.0f);
                if(owned.tx)base[bone].position.x+=(value.position.x-origin.position.x)*weight;if(owned.ty)base[bone].position.y+=(value.position.y-origin.position.y)*weight;if(owned.tz)base[bone].position.z+=(value.position.z-origin.position.z)*weight;
                if(owned.rotation){const Quat inverseOrigin{-origin.rotation.x,-origin.rotation.y,-origin.rotation.z,origin.rotation.w};const auto delta=normalize(multiply(inverseOrigin,value.rotation));base[bone].rotation=normalize(multiply(base[bone].rotation,slerp(Quat{},delta,weight)));}
                if(owned.sx)base[bone].scale.x*=1.0f+(value.scale.x/std::max(std::abs(origin.scale.x),1e-8f)-1.0f)*weight;if(owned.sy)base[bone].scale.y*=1.0f+(value.scale.y/std::max(std::abs(origin.scale.y),1e-8f)-1.0f)*weight;if(owned.sz)base[bone].scale.z*=1.0f+(value.scale.z/std::max(std::abs(origin.scale.z),1e-8f)-1.0f)*weight;
            }
        }
    }
    return base;
}

std::vector<Mat4> CastScene::samplePoseSlots(std::size_t baseAnimation,float baseFrame,const std::vector<PoseSlot>& slots) const {
    return globalPose(sampleLocalPoseSlots(baseAnimation,baseFrame,slots));
}

const char* notetrackKindName(NotetrackKind kind) {
    switch (kind) {
        case NotetrackKind::Fire: return "Fire";
        case NotetrackKind::MuzzleFlash: return "Muzzle Flash";
        case NotetrackKind::EjectBrass: return "Eject Brass";
        case NotetrackKind::DropClip: return "Drop Clip";
        case NotetrackKind::ClipIn: return "Clip In";
        case NotetrackKind::Rechamber: return "Rechamber";
        case NotetrackKind::Pullout: return "Pullout";
        case NotetrackKind::Putaway: return "Putaway";
        case NotetrackKind::Melee: return "Melee";
        case NotetrackKind::Footstep: return "Footstep";
        case NotetrackKind::Sound: return "Sound";
        case NotetrackKind::Custom: return "Custom";
        default: return "Unknown";
    }
}

NotetrackKind classifyNotetrackName(std::string_view name) {
    std::string lower{name};
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("fire") != std::string::npos) return NotetrackKind::Fire;
    if (lower.find("flash") != std::string::npos) return NotetrackKind::MuzzleFlash;
    if (lower.find("brass") != std::string::npos || lower.find("eject") != std::string::npos) return NotetrackKind::EjectBrass;
    if (lower.find("clipout") != std::string::npos || lower.find("clip_out") != std::string::npos || lower.find("dropclip") != std::string::npos) return NotetrackKind::DropClip;
    if (lower.find("clipin") != std::string::npos || lower.find("clip_in") != std::string::npos) return NotetrackKind::ClipIn;
    if (lower.find("chamber") != std::string::npos || lower.find("bolt") != std::string::npos) return NotetrackKind::Rechamber;
    if (lower.find("pullout") != std::string::npos || lower.find("raise") != std::string::npos) return NotetrackKind::Pullout;
    if (lower.find("putaway") != std::string::npos || lower.find("drop") != std::string::npos) return NotetrackKind::Putaway;
    if (lower.find("melee") != std::string::npos || lower.find("swipe") != std::string::npos) return NotetrackKind::Melee;
    if (lower.find("step") != std::string::npos || lower.find("foot") != std::string::npos) return NotetrackKind::Footstep;
    if (lower.find("sound") != std::string::npos || lower.find("snd") != std::string::npos) return NotetrackKind::Sound;
    return NotetrackKind::Unknown;
}

std::vector<Mat4> CastScene::sampleRootRelativePose(std::size_t animationIndex,float frame,
                                                    const std::vector<Transform>& rootReference) const {
    auto pose=sampleLocalPose(animationIndex,frame),origin=sampleLocalPose(animationIndex,0.0f);
    for(std::size_t i=0;i<pose.size()&&i<rootReference.size()&&i<origin.size();++i){
        if(skeleton.bones[i].parent>=0)continue;
        pose[i].position=rootReference[i].position+(pose[i].position-origin[i].position);
        const Quat inverseOrigin{-origin[i].rotation.x,-origin[i].rotation.y,-origin[i].rotation.z,origin[i].rotation.w};
        pose[i].rotation=normalize(multiply(rootReference[i].rotation,multiply(inverseOrigin,pose[i].rotation)));
    }
    return globalPose(pose);
}

const char* animationRoleName(AnimationRole role){
    switch(role){case AnimationRole::Idle:return "Idle";case AnimationRole::Walk:return "Walk";case AnimationRole::Run:return "Run";case AnimationRole::Sprint:return "Sprint";case AnimationRole::Jump:return "Jump";case AnimationRole::Fire:return "Fire";case AnimationRole::Reload:return "Reload";case AnimationRole::Melee:return "Melee";case AnimationRole::Death:return "Death";default:return "Unknown";}
}
const char* animationDomainName(AnimationDomain value){switch(value){case AnimationDomain::PlayerBody:return "Player body";case AnimationDomain::PlayerTorso:return "Player torso";case AnimationDomain::ViewModel:return "Viewmodel";default:return "Other";}}
const char* motionRoleName(MotionRole value){switch(value){case MotionRole::Idle:return "Idle";case MotionRole::Walk:return "Walk";case MotionRole::Run:return "Run";case MotionRole::Sprint:return "Sprint";case MotionRole::Crawl:return "Crawl";case MotionRole::Jump:return "Jump";case MotionRole::Land:return "Land";case MotionRole::Turn:return "Turn";case MotionRole::Transition:return "Transition";case MotionRole::Stumble:return "Stumble";case MotionRole::Climb:return "Climb";case MotionRole::Dive:return "Dive";case MotionRole::Slide:return "Slide";case MotionRole::Ladder:return "Ladder";default:return "Unknown";}}
const char* actionRoleName(ActionRole value){switch(value){case ActionRole::Aim:return "Aim";case ActionRole::Fire:return "Fire";case ActionRole::Reload:return "Reload";case ActionRole::Melee:return "Melee";case ActionRole::Flinch:return "Flinch";case ActionRole::Death:return "Death";case ActionRole::Shellshock:return "Shellshock";case ActionRole::Deploy:return "Deploy";case ActionRole::Plant:return "Plant";case ActionRole::Equip:return "Equip";case ActionRole::FirstRaise:return "First raise";case ActionRole::Unequip:return "Unequip";case ActionRole::Alert:return "Alert";case ActionRole::Gesture:return "Gesture";case ActionRole::GrenadePrep:return "Grenade prep";case ActionRole::Throw:return "Throw";case ActionRole::SwitchWeapon:return "Weapon switch";default:return "None";}}
const char* weaponClassName(WeaponClass value){switch(value){case WeaponClass::Rifle:return "Rifle";case WeaponClass::Sniper:return "Sniper";case WeaponClass::Automatic:return "Automatic / SMG";case WeaponClass::Pistol:return "Pistol";case WeaponClass::DualWield:return "Dual wield";case WeaponClass::Shotgun:return "Shotgun";case WeaponClass::M1216:return "M1216";case WeaponClass::Judge:return "Judge";case WeaponClass::G11:return "G11";case WeaponClass::LMG:return "LMG";case WeaponClass::Crossbow:return "Crossbow";case WeaponClass::BallisticKnife:return "Ballistic knife";case WeaponClass::Knife:return "Knife";case WeaponClass::Grenade:return "Grenade";case WeaponClass::Launcher:return "Launcher";case WeaponClass::RiotShield:return "Riot shield";case WeaponClass::Heavy:return "Heavy gunner";case WeaponClass::Minigun:return "Minigun";case WeaponClass::Equipment:return "Equipment / hold";case WeaponClass::Tablet:return "Tablet";case WeaponClass::Radio:return "Radio";case WeaponClass::Briefcase:return "Briefcase";case WeaponClass::RC:return "RC";default:return "Any";}}
const char* reloadStyleName(ReloadStyle value){switch(value){case ReloadStyle::Standard:return "Standard";case ReloadStyle::GL:return "Grenade launcher";case ReloadStyle::HandleClip:return "Handle clip";case ReloadStyle::RearClip:return "Rear clip";case ReloadStyle::MP40:return "MP40";default:return "Any";}}
const char* stanceName(Stance stance){switch(stance){case Stance::Stand:return "Stand";case Stance::Crouch:return "Crouch";case Stance::Prone:return "Prone";default:return "Any";}}
const char* directionName(Direction direction){switch(direction){case Direction::Forward:return "Forward";case Direction::Backward:return "Backward";case Direction::Left:return "Left";case Direction::Right:return "Right";default:return "Any";}}

} // namespace scene
