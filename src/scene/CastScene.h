#pragma once

#include "cast/CastDocument.h"
#include "scene/Math3D.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scene {

[[nodiscard]] bool isEmissiveMaterialIdentity(std::string_view value);

struct Vertex {
    Vec3 position{};
    Vec3 normal{0, 0, 1};
    Vec2 uv{};
    std::array<std::uint32_t, 4> bones{};
    std::array<float, 4> weights{1, 0, 0, 0};
    Vec4 color{1, 1, 1, 1};
};

struct Bounds {
    Vec3 minimum{};
    Vec3 maximum{};
    bool valid{};
    [[nodiscard]] Vec3 center() const { return (minimum + maximum) * 0.5f; }
    [[nodiscard]] float radius() const { return valid ? length(maximum - minimum) * 0.5f : 1.0f; }
};

struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    Mat4 modelTransform = Mat4::identity();
    Vec4 color{0.52f, 0.62f, 0.72f, 1.0f};
    std::filesystem::path albedoPath;
    std::filesystem::path normalPath;
    std::filesystem::path specularPath;
    std::filesystem::path metalnessPath;
    std::filesystem::path roughnessPath;
    std::string materialName;
    bool skinned{};
    bool camoBlend{};
    bool camoUseAlpha{true};
    bool hideWhenCamo{};
    bool lens{};
    bool eyeOverlay{}; // Clear corneal shell, distinct from the opaque iris.
    bool emissive{};
    bool forceAlpha{};
    bool decal{};
    bool decalMultiply{};
    bool decalAdditive{};
    bool alphaTest{};
    bool ignoreAlbedoAlpha{}; // Alpha stores auxiliary material data, not opacity.
    bool viewmodelWeapon{};
    bool gltfPbr{};
    // Explicit imported material policy; legacy CoD heuristics remain unchanged.
    bool materialPolicyExplicit{};
    bool doubleSided{};
    bool unlit{};
    bool useVertexColor{};
    bool vertexBlendBaked{};
    float alphaCutoff{0.35f};
    float materialDepthBias{}; // Polygon offset only, never a geometry displacement.
    int renderQueue{-1};
    int sourceBlend{-1}, destinationBlend{-1}; // Preserved Unity blend factors.
    std::filesystem::path emissivePath;
    std::shared_ptr<const std::string> sourceMaterialMetadata;
    bool specularGlossiness{}; // Authored linear RGB F0 / alpha gloss, not metallic-roughness.
    float metallicFactor{};
    float roughnessFactor{1.0f};
    float transmissionFactor{};
    float indexOfRefraction{1.5f};
    Vec3 emissiveFactor{};
    std::uint8_t normalProfile{}; // 0=T6/default, 1=T5 AG, 2=IW5 AG, 3=glTF RGB, 4=AW RG, 5=IW3/COD4 AG
    std::int32_t attachmentIndex{-1};
    std::int32_t actorVariant{-1};
    Bounds bounds{};
};

struct Transform {
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1, 1, 1};
};

struct Bone {
    std::string name;
    std::int32_t parent{-1};
    Transform restLocal;
    Mat4 restGlobal = Mat4::identity();
    Mat4 inverseBind = Mat4::identity();
    bool translationTracksAreDeltas{};
    Vec3 absoluteTranslationOffset{};
    bool isMechanism{};
    bool isAimRoot{};
};

struct Skeleton {
    std::vector<Bone> bones;
    std::unordered_map<std::string, std::size_t> boneByName;
    std::unordered_map<std::string, std::size_t> boneByCanonicalName;
};

enum class TrackProperty { TranslationX, TranslationY, TranslationZ, Rotation, ScaleX, ScaleY, ScaleZ };
enum class TrackMode { Absolute, Relative, Additive };
enum class AnimationRole { Unknown, Idle, Walk, Run, Sprint, Jump, Fire, Reload, Melee, Death };
enum class AnimationDomain { Other, PlayerBody, PlayerTorso, ViewModel };
enum class MotionRole { Unknown, Idle, Walk, Run, Sprint, Crawl, Jump, Land, Turn, Transition, Stumble, Climb, Dive, Slide, Ladder };
enum class ActionRole { None, Aim, Fire, Reload, Melee, Flinch, Death, Shellshock, Deploy, Plant, Equip, FirstRaise, Unequip, Alert, Gesture, GrenadePrep, Throw, SwitchWeapon, Inspect };
enum class WeaponClass { Any, Rifle, Sniper, Automatic, Pistol, DualWield, Shotgun, M1216, Judge, G11, LMG, Crossbow, BallisticKnife, Knife, Grenade, Launcher, RiotShield, Heavy, Minigun, Equipment, Tablet, Radio, Briefcase, RC };
enum class LayerMode { Additive, Override };
struct PoseLayer { std::size_t animation{};float frame{},weight{1.0f};LayerMode mode{LayerMode::Additive};bool suppressRootMotion{true};bool preserveWeaponMechanisms{};bool preserveViewmodelAimRoot{};std::size_t referenceAnimation{static_cast<std::size_t>(-1)};float referenceFrame{}; };
struct PoseSlot { std::vector<PoseLayer> nodes; };
enum class ReloadStyle { Any, Standard, GL, HandleClip, RearClip, MP40 };
enum class Stance { Any, Stand, Crouch, Prone };
enum class Direction { Any, Forward, Backward, Left, Right, ForwardLeft, ForwardRight, BackwardLeft, BackwardRight };

enum class NotetrackKind {
    Unknown,
    Fire,
    MuzzleFlash,
    EjectBrass,
    DropClip,
    ClipIn,
    Rechamber,
    Pullout,
    Putaway,
    Melee,
    Footstep,
    Sound,
    Custom
};

struct Track {
    // Dense retarget bind support is needed for standalone poses, but must not
    // acquire ownership of unauthored bones when the clip is used as a layer.
    std::size_t boneIndex{};
    TrackProperty property{};
    TrackMode mode{TrackMode::Absolute};
    float additiveWeight{1.0f};
    std::vector<std::uint32_t> frames;
    std::vector<float> scalarValues;
    std::vector<Quat> rotationValues;
    bool ownsLayer{true};
};

struct ColdWarWorldPose;
struct Animation {
    std::string name;
    std::string sourceName;
    std::string sourceGame;
    float framerate{30.0f};
    bool looping{};
    std::uint32_t durationFrames{};
    std::size_t sourceCurveCount{};
    std::size_t unmappedCurveCount{};
    AnimationRole role{AnimationRole::Unknown};
    AnimationDomain domain{AnimationDomain::Other};
    MotionRole motion{MotionRole::Unknown};
    ActionRole action{ActionRole::None};
    WeaponClass weapon{WeaponClass::Any};
    Stance stance{Stance::Any};
    Direction direction{Direction::Any};
    int aimYawDegrees{};
    int aimPitchDegrees{};
    bool hasAimYaw{};
    bool hasAimPitch{};
    bool ads{};
    ReloadStyle reloadStyle{ReloadStyle::Standard};
    bool contextual{};
    std::vector<Track> tracks;
    struct Notification { std::string name; std::vector<std::uint32_t> frames; NotetrackKind kind{NotetrackKind::Unknown}; };
    std::vector<Notification> notifications;
    std::optional<Mat4> viewmodelCameraReference;
    std::shared_ptr<const ColdWarWorldPose> coldWarWorldPose;
};

struct AnimationQuery {
    AnimationDomain domain{AnimationDomain::PlayerBody};
    MotionRole motion{MotionRole::Unknown};
    ActionRole action{ActionRole::None};
    WeaponClass weapon{WeaponClass::Any};
    Stance stance{Stance::Stand};
    Direction direction{Direction::Any};
    bool ads{};
    ReloadStyle reloadStyle{ReloadStyle::Standard};
    bool allowContextual{};
    bool requireMappedTracks{true};
    std::string preferredGame{};
    bool forceT6Locomotion{false};
};

struct RuntimePoseSource {
    Skeleton skeleton;
    Animation animation;
};

struct Attachment {
    std::optional<Vec3> muzzleLocal; // Imported model-space muzzle, retained when its skeleton is flattened.
    std::string name;
    std::size_t boneIndex{};
    std::size_t firstMesh{};
    std::size_t meshCount{};
    Vec3 position{};
    Vec3 rotationDegrees{};
    Vec3 scale{1,1,1};
    bool customCamoLuma{};
    bool useBaseColorLumaMask{};
    bool invertCamoMask{true};
    float camoLumaLow{};
    float camoLumaHigh{1.0f};
    float camoLumaGamma{1.0f};
    float camoLumaContrast{1.0f};

    [[nodiscard]] Mat4 localMatrix() const {
        const Vec3 radians=rotationDegrees*(kPi/180.0f);
        return trs(position,fromEulerRadians(radians),scale);
    }
};

struct RigPart {
    std::string name;
    std::size_t firstMesh{};
    std::size_t meshCount{};
    std::vector<std::size_t> rootBones;
};

struct CodmRigBinding { std::size_t source{}; Mat4 offset=Mat4::identity(); std::size_t rollSource{static_cast<std::size_t>(-1)}; Mat4 rollOffset=Mat4::identity(); float rollWeight{}; };
struct CodmRigAdapter {
    std::string identity;
    std::size_t firstBone{};
    std::vector<CodmRigBinding> bindings;
    std::vector<std::string> helperRules;
};
struct CastScene {
    bool dualWield{};
    std::array<std::size_t,2> dualFireClips{static_cast<std::size_t>(-1),static_cast<std::size_t>(-1)};
    // Original local bind at the moment a user starts editing a rig mount.
    std::unordered_map<std::size_t,Transform> rigMountReferences;
    struct RuntimePoseAdapter {
        std::shared_ptr<const RuntimePoseSource> source;
        std::vector<std::optional<std::size_t>> sourceBoneForTarget;
        std::vector<Quat> alignedRestGlobalRotations;
        std::vector<bool> adaptedBones;
        float translationScale{1.0f};
    };

    std::vector<Mesh> meshes;
    Skeleton skeleton;
    std::vector<Animation> animations;
    std::vector<Attachment> attachments;
    std::vector<RigPart> rigParts;
    std::unordered_set<std::size_t> hiddenBones;
    Bounds bounds;
    std::vector<std::string> warnings;
    std::unordered_map<std::size_t,RuntimePoseAdapter> runtimePoseAdapters;
    bool codmNativeCentimetres{};
    bool pointBlankNativeCentimetres{};
    std::string pointBlankWeaponStem;
    float codmTranslationFactor{100.f};
    std::string codmNativeWeaponStem;
    bool codmNativeCameraCalibrated{};
    std::vector<std::pair<std::size_t,std::size_t>> nativePoseFollowers;
    std::shared_ptr<const CodmRigAdapter> codmRigAdapter;

    [[nodiscard]] std::vector<Transform> sampleLocalPose(std::size_t animationIndex, float frame) const;
    void sampleLocalPoseInto(std::size_t animationIndex,float frame,std::vector<Transform>& output) const;
    [[nodiscard]] std::vector<Mat4> globalPose(const std::vector<Transform>& localPose) const;
    [[nodiscard]] std::vector<Mat4> samplePose(std::size_t animationIndex, float frame) const;
    [[nodiscard]] std::vector<Transform> sampleBlendedLocalPose(std::size_t fromAnimation,float fromFrame,std::size_t toAnimation,float toFrame,float alpha) const;
    [[nodiscard]] std::vector<Mat4> sampleBlendedPose(std::size_t fromAnimation, float fromFrame,
                                                     std::size_t toAnimation, float toFrame,
                                                     float alpha) const;
    [[nodiscard]] std::vector<Mat4> sampleLayeredPose(std::size_t baseAnimation,float baseFrame,
                                                     std::size_t layerAnimation,float layerFrame,
                                                     float weight,LayerMode mode=LayerMode::Additive,
                                                     bool suppressRootMotion=true,const std::vector<Transform>* baseOverride=nullptr) const;
    [[nodiscard]] std::vector<Mat4> sampleLayerStack(std::size_t baseAnimation,float baseFrame,const std::vector<PoseLayer>& layers) const;
    [[nodiscard]] std::vector<Transform> sampleLocalPoseSlots(std::size_t baseAnimation,float baseFrame,const std::vector<PoseSlot>& slots) const;
    [[nodiscard]] std::vector<Mat4> samplePoseSlots(std::size_t baseAnimation,float baseFrame,const std::vector<PoseSlot>& slots) const;
    [[nodiscard]] std::vector<Mat4> sampleRootRelativePose(std::size_t animationIndex,float frame,
                                                          const std::vector<Transform>& rootReference) const;
};

struct ClassificationOverride {
    std::optional<AnimationDomain> domain;
    std::optional<MotionRole> motion;
    std::optional<ActionRole> action;
    std::optional<WeaponClass> weapon;
    std::optional<Stance> stance;
    std::optional<Direction> direction;
    std::optional<bool> ads;
    std::optional<bool> looping;
    std::optional<bool> contextual;
    std::optional<ReloadStyle> reloadStyle;
};
void applyClassificationOverride(Animation& animation, const ClassificationOverride& override);

[[nodiscard]] CastScene buildScene(const cast::Document& document,bool prepareViewmodel=true);
[[nodiscard]] std::optional<Vec3> resolveMuzzlePosition(const CastScene& scene,const std::vector<Mat4>& globalPose);
std::size_t appendAnimations(const cast::Document& document, CastScene& scene);
bool composeIwSprintOffset(CastScene& scene,std::size_t animation,std::size_t idle);
// T6 swaps authored incoming dummy magazines for installed magazines on reload exit.
void restoreT6MagazineAfterReload(const CastScene& scene,std::vector<Mat4>& pose,std::size_t targetAnimation,float targetFrame);
void retargetAnimationRange(CastScene& scene,std::size_t firstAnimation,const Skeleton& sourceSkeleton);
void retargetAnimationRange(CastScene& scene,std::size_t firstAnimation,const CastScene& sourceScene,std::size_t firstSourceAnimation,bool inverseIwCalibration=false);
bool installRuntimePoseAdapter(CastScene& target,std::size_t targetAnimation,const CastScene& source,std::size_t sourceAnimation);
bool retargetSource2AnimationRange(CastScene& target,std::size_t firstAnimation,const CastScene& source,std::size_t firstSourceAnimation);
float source2RetargetScale(const Skeleton& target,const Skeleton& source);
[[nodiscard]] std::optional<float> mechanismReadyFrame(const CastScene& scene,std::size_t animationIndex,std::string_view boneToken);
std::size_t appendAttachment(const cast::Document& document, CastScene& scene,
                             std::size_t boneIndex, std::string name);
// Consume already prepared mesh data; retain the source skeleton for socket
// queries. Identical attachment path without decoding twice.
std::size_t appendPreparedAttachment(CastScene&& imported,CastScene& scene,std::size_t boneIndex,std::string name);
// Exact geometry/bind identity, ignoring materials; useful for rejecting auto-mounted skin variants.
bool sameRigGeometry(const CastScene& a,const CastScene& b);
std::size_t appendRigModel(const cast::Document& document,CastScene& scene,std::string name,std::optional<Mat4> sourceMount=std::nullopt,float uniformScale=1.0f);
void classifyAnimationName(std::string_view name, Animation& animation);
[[nodiscard]] std::optional<std::size_t> findBestAnimation(const CastScene& scene,const AnimationQuery& query);
[[nodiscard]] const char* animationRoleName(AnimationRole role);
[[nodiscard]] const char* animationDomainName(AnimationDomain domain);
[[nodiscard]] const char* motionRoleName(MotionRole motion);
[[nodiscard]] const char* actionRoleName(ActionRole action);
[[nodiscard]] const char* weaponClassName(WeaponClass weapon);
[[nodiscard]] const char* reloadStyleName(ReloadStyle style);
[[nodiscard]] const char* stanceName(Stance stance);
[[nodiscard]] const char* directionName(Direction direction);
[[nodiscard]] NotetrackKind classifyNotetrackName(std::string_view name);
[[nodiscard]] const char* notetrackKindName(NotetrackKind kind);
[[nodiscard]] std::string canonicalName(std::string value);
void installCrossGenerationViewmodelAliases(Skeleton& skeleton);
[[nodiscard]] std::optional<std::size_t> rigSocketForRoot(const Skeleton& skeleton,const std::string& rootName);
[[nodiscard]] bool isViewmodelSkeleton(const Skeleton& skeleton);

} // namespace scene
