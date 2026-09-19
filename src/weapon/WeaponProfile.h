#pragma once

#include "scene/Math3D.h"
#include "gameplay/ViewRecoil.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace weapon {

enum class Archetype { Rifle, Smg, Lmg, Shotgun, BoltSniper, BoltIndividualSniper, SemiSniper, Pistol, DualWield, Launcher, Equipment, Unknown };

[[nodiscard]] constexpr bool isSniper(Archetype value) noexcept {
    return value==Archetype::BoltSniper||value==Archetype::BoltIndividualSniper||value==Archetype::SemiSniper;
}

struct Stats {
    float fireTime{0.1f},rechamberTime{},adsIn{0.25f},adsOut{0.25f},dropTime{0.4f},raiseTime{0.6f},yyReturnScale{1.5f};
    float reloadTime{},reloadEmptyTime{},firstRaiseTime{},meleeTime{0.7f};
    float sprintInTime{0.2f},sprintLoopTime{0.8f},sprintOutTime{0.25f};
    float quickDropTime{0.25f},quickRaiseTime{0.4f};
    float rechamberStartDelay{}; // Signed offset from the fire clip end for new profiles.
    bool rechamberDelayFromFireEnd{true}; // False preserves legacy shot-relative / -1 automatic timing.
    float sprintPlaybackScale{1.f};
    gameplay::view::RecoilSettings recoil{};
    float adsFov{50.0f},moveSpeedScale{1.0f};
    float adsKickPitchMin{0.3f},adsKickPitchMax{0.45f},adsKickYawMin{-0.5f},adsKickYawMax{0.5f},adsKickCenterSpeed{15.0f};
    float hipKickPitchMin{0.3f},hipKickPitchMax{0.45f},hipKickYawMin{-0.5f},hipKickYawMax{0.5f},hipKickCenterSpeed{15.0f};
    int burstCount{1};
    float burstDelay{};
    bool fullAuto{},boltAction{},individualReload{};
    // Preserve prior cadence-ready interruption unless explicitly disabled.
    bool canFireWhileRechambering{true};
    float rechamberFireUnlock{0.f}; // Seconds after rechamber begins; fire cadence still applies.
    // Defaults to the old sniper visibility policy; can be overridden per weapon.
    bool hideWeaponOnAds{};
    float damage{35.0f};
};

[[nodiscard]] inline bool canInterruptRechamber(const Stats& stats,float elapsed){
    const float delay=std::isfinite(stats.rechamberFireUnlock)?std::max(0.f,stats.rechamberFireUnlock):0.f;
    return stats.canFireWhileRechambering&&std::isfinite(elapsed)&&elapsed>=delay;
}
[[nodiscard]] inline float rechamberStartSeconds(const Stats& stats,float fireDuration){
    if(!stats.rechamberDelayFromFireEnd)return gameplay::view::rechamberDelay(stats.rechamberStartDelay,fireDuration);
    const float duration=std::isfinite(fireDuration)?std::max(.03f,fireDuration):.03f;
    const float offset=std::isfinite(stats.rechamberStartDelay)?std::clamp(stats.rechamberStartDelay,-5.f,5.f):0.f;
    return std::max(0.f,duration+offset);
}

struct AnimationOffset {
    std::string animation;
    scene::Vec3 position{};
    scene::Vec3 rotationDegrees{};
    float scale{1.0f};
};

struct MaterialTuning {
    bool useBaseColorLumaMask{};
    bool invertCamoMask{true};
    float camoAlphaLow{0.031f};
    float camoAlphaHigh{1.0f};
    float camoLumaLow{};
    float camoLumaHigh{1.0f};
    float camoLumaGamma{1.0f};
    float camoLumaContrast{1.0f};
    float specularMultiplier{1.0f};
    bool overrideMetalness{};
    float metalness{0.0f};
    bool metalnessFromDiffuse{};
    float metalnessBlack{},metalnessWhite{1.f},metalnessGamma{1.f};
    // Per-channel input levels. Crossing either endpoint intentionally
    // inverts that channel instead of forcing a sorted range.
    scene::Vec3 specularColorLow{};
    scene::Vec3 specularColorHigh{1.0f,1.0f,1.0f};
    float cubemapSpecularIntensity{1.0f};
};

struct RigMount {
    bool attachedModel{};
    scene::Vec3 attachmentRotationDegrees{};
    scene::Vec3 attachmentScale{1,1,1};
    std::string model;
    std::string rootBone;
    std::string parentTag;
    scene::Vec3 position{};
    scene::Quat rotation{};
    bool hasCustomCamoLuma{};
    MaterialTuning materials{};
};

struct Profile {
    int version{1};
    std::string name{"Generated weapon"};
    std::string internalName;
    std::string source{"generated"};
    std::string animationPrefix;
    std::string viewModelName;
    std::string worldModelName;
    std::string handModelName;
    std::string baseModel;
    std::string scopeOverlayImage; // Empty selects the game's automatic overlay.
    std::vector<std::string> rigModels;
    Archetype archetype{Archetype::Unknown};
    Stats stats{};
    MaterialTuning materials{};
    // Camera-local offset for the complete first-person rig. The gameplay
    // camera is deliberately not moved with it.
    scene::Vec3 gunPosition{};
    // Standard keys are idle, fire, ads_fire, reload, reload_empty,
    // ads_up/down, sprint_*, melee, rechamber, pullout, putaway and mantle.
    // Optional pairing keys append .from_primary/.from_sidearm or
    // .to_primary/.to_sidearm.
    std::map<std::string,std::string> animations;
    // Exact source dependencies for assigned clips, including other games.
    std::map<std::string,std::string> animationFiles;
    // Ordered authored variants for actions such as inspect and knife attack.
    // The legacy animations map remains the canonical single-clip fallback and
    // is kept for old weaponfiles and UI code.
    std::map<std::string,std::vector<std::string>> animationVariants;
    std::vector<AnimationOffset> animationOffsets;
    std::vector<RigMount> rigMounts;
    std::vector<std::string> hiddenTags;
    bool lockInspectRig{true};
    bool lockInspectGrip{false};
    // Capability flag; unlike the gameplay weapon class this identifies a
    // melee/knife profile for effect and animation policy decisions.
    bool meleeWeapon{false};
    float knifeRadiusCm{150.0f};
    float knifeConeDegrees{40.0f}; // Full angle, centered on aim.
};

[[nodiscard]] inline bool isAdsDownClip(std::string_view name) { return name.find("_ads_down")!=std::string_view::npos||name.find("_ads_base_down")!=std::string_view::npos; }
[[nodiscard]] inline bool isAdsUpClip(std::string_view name) { return name.find("_ads_up")!=std::string_view::npos||name.find("_ads_base_up")!=std::string_view::npos; }

[[nodiscard]] const char* archetypeName(Archetype value);
[[nodiscard]] Archetype archetypeFromName(std::string value);
[[nodiscard]] Archetype inferArchetype(std::string value);
[[nodiscard]] Stats defaultsFor(Archetype value);
[[nodiscard]] Profile makeGenerated(std::string name,Archetype archetype,std::string animationPrefix={});
// Share behavior between cosmetic variants without replacing their models or mounts.
inline void applyFamilyBehavior(Profile& target,const Profile& family){
    target.stats=family.stats;target.archetype=family.archetype;
    target.animationPrefix=family.animationPrefix;target.animations=family.animations;target.animationFiles=family.animationFiles;
    target.animationVariants=family.animationVariants;
}
[[nodiscard]] std::optional<std::string> animationFor(const Profile& profile,const std::string& action,const std::string& pairing={});
[[nodiscard]] std::vector<std::string> animationVariantsFor(const Profile& profile,const std::string& action,const std::string& pairing={});
[[nodiscard]] const AnimationOffset* offsetFor(const Profile& profile,const std::string& animation);
bool save(const Profile& profile,const std::filesystem::path& path,std::string& error);
bool load(const std::filesystem::path& path,Profile& profile,std::string& error);
bool importLegacyWeaponFile(Profile& profile,const std::filesystem::path& path,std::string& error);

} // namespace weapon
