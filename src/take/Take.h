#pragma once

#include "gameplay/ResponseCurve.h"
#include "scene/Math3D.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace scene {struct Skeleton;}
namespace take {

struct InterpolationRigs {
    const scene::Skeleton* view{};
    const scene::Skeleton* world{};
    const scene::Skeleton* bots{};
};
// Per-sample geometry visibility, not a saved lighting/post-processing preset.
enum Visibility : std::uint8_t { VisibilityKnown=1, HideWeapon=2, HideViewmodel=4, ScopeOverlay=8 };
inline bool validVisibility(std::uint8_t value){
    return (value&~15u)==0&&(value==0||(value&VisibilityKnown))&&(!(value&ScopeOverlay)||(value&HideViewmodel));
}

struct CameraState {
    scene::Vec3 target{};
    float yaw{};
    float pitch{};
    float distance{5.0f};
    float fov{0.0f};
    float adsBlend{0.0f};
};

struct RecordedActorState {
    std::uint32_t id{};
    std::int32_t modelVariant{};
    float health{100.0f};
    bool alive{true};
    std::uint8_t visibility{};
    std::vector<scene::Mat4> pose;
    std::string primaryClip;
    std::string torsoClip;
    float primaryFrame{};
    float torsoFrame{};
};

struct DollyCameraKeyframe {
    std::uint32_t tick{};
    scene::Vec3 position{};
    scene::Vec3 rotationDegrees{};
    float fov{90.0f};
};

struct DollyCameraPath {
    std::string name;
    std::vector<DollyCameraKeyframe> keyframes;
};

struct Sample {
    float time{};
    CameraState camera;
    std::vector<scene::Mat4> pose;
    std::string primaryClip;
    std::string torsoClip;
    float primaryFrame{};
    float torsoFrame{};
    RecordedActorState worldActor;
    std::vector<RecordedActorState> bots;
    std::vector<std::uint32_t> hiddenBones;
    std::uint8_t weaponSlot{};
    std::uint8_t visibility{};
};

struct ShotEvent {
    float time{};
    scene::Vec3 origin{};
    scene::Vec3 direction{};
    scene::Vec3 muzzlePos{};
    scene::Vec3 hitPos{};
    bool hitFound{false};
    bool sniperWeapon{false};

    // Muzzle flash FX
    float muzzleFlashDuration{0.06f};
    float muzzleFlashSize{1.0f};
    float muzzleFlashRotation{0.0f};
    scene::Vec4 muzzleFlashColor{1.0f, 0.92f, 0.65f, 1.0f};
    float muzzleFlashCurvePower{1.6f};

    // Bullet trail FX
    bool trailEmissive{true};
    gameplay::view::ResponseCurve trailTaperCurve{};
    float trailStartTaper{1.0f};
    float trailEndTaper{1.0f};
    bool trailSprite{false};
    float trailLifetime{0.65f};
    float trailWidth{1.0f};
    scene::Vec4 trailColor{1.0f, 1.0f, 1.0f, 1.0f};
    float trailEmissiveIntensity{2.5f};
    float trailFeathering{0.2f};
    float trailSpeed{14000.0f};
    float trailLength{120.0f};

    // Muzzle smoke FX
    bool smokeEnabled{true};
    float smokeEmissionDuration{0.35f};
    float smokeLifetime{1.1f};
    float smokeStartWidth{3.41f};
    float smokeEndWidth{20.0f};
    float smokeTaperingExp{1.44f};
    float smokeBlastSpeed{30.7f};
    float smokeRiseSpeed{50.0f};
    float smokeDispersion{2.6f};
    scene::Vec4 smokeColor{1.0f, 1.0f, 1.0f, 6.0f / 255.0f};
};

struct AttachedModel {
    std::string path;
    std::string bone;
    scene::Vec3 position{};
    scene::Vec3 rotationDegrees{};
    scene::Vec3 scale{1.0f,1.0f,1.0f};
    std::int32_t modelVariant{-1};
};

struct ActorManifest {
    std::string baseModel;
    std::vector<std::string> rigModels;
    std::vector<std::int32_t> rigModelVariants;
    bool replaceBaseMeshes{};
    std::vector<AttachedModel> attachedModels;
    std::vector<std::string> hiddenBones;
    bool viewmodelCamera{};
    float viewmodelFov{65.0f};
    float viewmodelFovScale{1.0f};

    [[nodiscard]] bool empty() const{return baseModel.empty();}
};

struct Take {
    float sampleRate{30.0f};
    std::size_t boneCount{};
    ActorManifest actor;
    ActorManifest worldActor;
    std::array<ActorManifest,3> actorSlots;
    std::array<ActorManifest,3> worldActorSlots;
    std::array<std::size_t,3> actorSlotBoneCounts{};
    std::array<std::size_t,3> worldActorSlotBoneCounts{};
    std::size_t worldBoneCount{};
    ActorManifest botActor;
    std::size_t botBoneCount{};
    std::size_t botCount{};
    std::vector<DollyCameraKeyframe> dollyCamera;
    std::vector<DollyCameraPath> cameraPaths;
    std::vector<Sample> samples;
    std::vector<ShotEvent> shots;

    [[nodiscard]] float duration() const;
    [[nodiscard]] bool compatible(std::size_t bones) const;
    [[nodiscard]] std::size_t bonesForSlot(std::uint8_t slot) const;
    [[nodiscard]] std::size_t bonesForWorldSlot(std::uint8_t slot) const;
    [[nodiscard]] bool compatibleSlot(std::size_t bones,std::uint8_t slot) const;
    [[nodiscard]] bool botsCompatible(std::size_t bones) const;
    [[nodiscard]] std::size_t sampleIndex(float time) const;
    [[nodiscard]] const Sample* sampleAt(float time) const;
    [[nodiscard]] Sample interpolatedSample(float time,InterpolationRigs rigs={}) const;
    bool trim(float start,float end);
    void clear();
};

[[nodiscard]] bool save(const Take& take,const std::filesystem::path& path,std::string& error);
[[nodiscard]] bool load(const std::filesystem::path& path,Take& take,std::string& error);
[[nodiscard]] std::uint64_t outputFrameCount(float start,float end,std::uint32_t fps,float timescale);
[[nodiscard]] float outputTakeTime(float start,std::uint64_t frame,std::uint32_t fps,float timescale);

} // namespace take
