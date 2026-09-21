#pragma once
#include "render/ActorBounds.h"
#include "render/ActorOverlays.h"
#include "render/FramePreparation.h"

#include "gameplay/ResponseCurve.h"
#include "scene/CastScene.h"
#include "render/Hbao.h"
#include "render/VolumetricLighting.h"
#include "render/Water.h"
#include "render/DayNight.h"
#include "render/NightSky.h"
#include "render/DepthOfField.h"
#include "render/ReplaySmoke.h"
#include "render/GpuTimer.h"
#include "scene/GlbMap.h"
#include "gameplay/BotActor.h"

#include <cstdint>
#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace take { struct DollyCameraKeyframe; }
struct GLFWwindow;

namespace render {
namespace texture { struct Prepared; }

using ProgressCallback = std::function<void(std::string_view msg, float progress)>;

class StageRenderer {
public:
    void setNightSky(NightSkySettings settings){settings.sanitize();nightSky_=settings;}
    bool setNightSkyPanorama(const std::filesystem::path& path);
    void setDayNight(daynight::Settings s,double time){s.sanitize();dayNight_=s;dayNightTime_=std::isfinite(time)?time:0;}
    void setVolumetricLighting(VolumetricLightingSettings settings){settings.sanitize();volumetric_=settings;}
    void setWater(water::Settings settings,double time){settings.sanitize();water_=settings;waterTime_=std::isfinite(time)?time:0;}
    const std::string& waterError()const{return waterError_;}
    const std::string& volumetricLightingError()const{return volumetricError_;}
    double gpuFrameMilliseconds() const { return gpuTimer_.milliseconds(); }
    void setCubemapSurfaceMultipliers(float viewmodel,float world){viewmodelCubemapMultiplier_=std::isfinite(viewmodel)?std::clamp(viewmodel,0.f,4.f):1.f;worldCubemapMultiplier_=std::isfinite(world)?std::clamp(world,0.f,4.f):1.f;}
    void setActorOverlays(ActorOverlaySettings settings,double time,std::vector<ActorOverlayInput> actors={},ActorOverlayInput player={},bool replay=false){if(replay!=overlayReplay_||time<overlayTime_||time-overlayTime_>.5){overlayHistory_.actors.clear();overlayImpacts_.clear();}overlayReplay_=replay;settings.sanitize();actorOverlays_=settings;overlayTime_=time;overlayInputs_=std::move(actors);overlayPlayer_=player;}
    void clearImpactCubes(){overlayImpacts_.clear();}
    void addImpactCube(scene::Vec3 position,double time=-1){if(actorOverlays_.impactCubes){if(overlayImpacts_.size()>=256)overlayImpacts_.pop_front();overlayImpacts_.push_back({position,time<0?overlayTime_:time});}}
    StageRenderer() = default;
    ~StageRenderer();
    StageRenderer(const StageRenderer&) = delete;
    StageRenderer& operator=(const StageRenderer&) = delete;

    [[nodiscard]] bool initialize(std::string& error);
    void shutdown();
    [[nodiscard]] bool loadScene(const scene::CastScene& scene, std::string& error);
    [[nodiscard]] bool loadPreviewScene(const scene::CastScene& scene, std::string& error);
    [[nodiscard]] bool replaceMainScene(const scene::CastScene& scene, const scene::CastScene* worldActorScene, const scene::CastScene* botActorScene, std::string& error);
    [[nodiscard]] bool hasMapMeshes() const noexcept { return mapMeshCount_ > 0; }
    [[nodiscard]] bool appendMainSceneMeshes(const scene::CastScene& scene,std::size_t firstSourceMesh,std::string& error);
    [[nodiscard]] bool loadClassScenes(const scene::CastScene& primary,const scene::CastScene& secondary,const scene::CastScene* primaryWorld,const scene::CastScene* secondaryWorld,const scene::CastScene* mapScene,const scene::CastScene* botActorScene,std::string& error,const scene::CastScene* third=nullptr,const scene::CastScene* thirdWorld=nullptr);
    [[nodiscard]] bool replaceClassScenes(const scene::CastScene& primary,const scene::CastScene& secondary,const scene::CastScene* primaryWorld,const scene::CastScene* secondaryWorld,const scene::CastScene* botActorScene,std::string& error,const scene::CastScene* third=nullptr,const scene::CastScene* thirdWorld=nullptr);
    void setActiveClassSlot(int slot) noexcept { activeClassSlot_=slot>=0&&slot<3?slot:0; }
    [[nodiscard]] bool loadActorScene(const scene::CastScene& scene,std::string& error);
    [[nodiscard]] bool replaceBotScene(const scene::CastScene* scene,std::string& error);
    [[nodiscard]] bool loadAuxiliaryScenes(const scene::CastScene* mapScene,const scene::CastScene* worldActorScene,const scene::CastScene* botActorScene,std::string& error,ProgressCallback progressCallback=nullptr);
    void clearScene();
    void render(const scene::CastScene& scene, const std::vector<scene::Mat4>& globalPose,
                const scene::Mat4& viewProjection, int width, int height,
                bool showGrid, bool showSkeleton, bool wireframe,
                const scene::CastScene* actorScene=nullptr,const std::vector<std::vector<scene::Mat4>>* actorPoses=nullptr,const std::vector<int>* actorVariants=nullptr,
                const scene::CastScene* worldActorScene=nullptr,const std::vector<scene::Mat4>* worldActorPose=nullptr,bool showMainScene=true,
                const scene::CastScene* mapScene=nullptr,const gameplay::bot::NavigationGraph* navigation=nullptr,
                const std::vector<scene::Vec3>* botSpawns=nullptr,int selectedNavigationNode=-1,int selectedNavigationBlock=-1,bool navigationOnTop=false,
                const std::vector<take::DollyCameraKeyframe>* campathOverlay=nullptr,int selectedCampathNode=-1,
                const take::DollyCameraKeyframe* activeCameraSample=nullptr);
    [[nodiscard]] bool saveColorPng(const std::filesystem::path& path,std::string& error);
    [[nodiscard]] bool savePixelsPng(const std::filesystem::path& path,int width,int height,const std::vector<std::uint8_t>& rgbaPixels,std::string& error);
    [[nodiscard]] bool saveDepthImage(const std::filesystem::path& path,int formatBitDepth,float projectionNear,float projectionFar,float rangeNear,float rangeFar,bool invert,std::string& error);
    [[nodiscard]] bool readColorRgba(std::vector<std::uint8_t>& pixels,std::string& error);
    [[nodiscard]] bool readDepthRgba(std::vector<std::uint8_t>& pixels,float projectionNear,float projectionFar,float rangeNear,float rangeFar,bool invert,std::string& error);
    [[nodiscard]] bool readDepthRawFloat(std::vector<float>& depths,float projectionNear,float projectionFar,float rangeNear,float rangeFar,bool invert,std::string& error);
    void setViewmodelCapture(bool enabled,scene::Vec4 background) noexcept { viewmodelCapture_=enabled;captureBackground_=background; }
    [[nodiscard]] bool setCamoTexture(const std::filesystem::path& path,std::string& error);
    void clearCamoTexture();
    [[nodiscard]] bool setStickerTexture(const std::filesystem::path& path,std::string& error);
    void clearStickerTexture();
    [[nodiscard]] bool setScopeOverlayTexture(const std::filesystem::path& path,std::string& error);
    void clearScopeOverlayTexture();
    [[nodiscard]] std::uintptr_t scopeOverlayTexture() const noexcept { return scopeOverlayTexture_; }
    [[nodiscard]] float scopeOverlayAspect() const noexcept { return scopeOverlayAspect_; }
    [[nodiscard]] scene::Vec3 scopeOverlayEdgeColor() const noexcept { return scopeOverlayEdgeColor_; }
    [[nodiscard]] bool setMuzzleFlashTexture(const std::filesystem::path& path,std::string& error);
    void clearMuzzleFlashTexture();
    [[nodiscard]] unsigned muzzleFlashTexture() const noexcept { return muzzleFlashTexture_; }
    [[nodiscard]] bool setSmokeTexture(const std::filesystem::path& path,std::string& error);
    void clearSmokeTexture();
    [[nodiscard]] unsigned smokeTexture() const noexcept { return smokeTexture_; }
    void setSmokeFeathering(float f) noexcept { smokeFeathering_ = std::clamp(f, 0.0f, 1.0f); }
    [[nodiscard]] float smokeFeathering() const noexcept { return smokeFeathering_; }
    [[nodiscard]] bool setSpecularImperfectionsTexture(const std::filesystem::path& path,std::string& error);
    void clearSpecularImperfectionsTexture();
    void setSpecularImperfectionParameters(float strength,float scale,float low,float high) noexcept { imperfectionStrength_=strength;imperfectionScale_=scale;imperfectionLow_=low;imperfectionHigh_=high; }
    void setCamoParameters(float strength,float scale,float alphaLow,float alphaHigh,bool invert,float offsetX=0,float offsetY=0,float rotation=0,bool universal=false,bool lumaMask=false,bool lumaNativeT6=false,float lumaLow=0,float lumaHigh=1,float lumaGamma=1,float lumaContrast=1) noexcept { camoStrength_=strength;camoScale_=scale;camoAlphaLow_=alphaLow;camoAlphaHigh_=alphaHigh;camoInvert_=invert;camoOffset_={offsetX,offsetY,0};camoRotation_=rotation;universalCamo_=universal;camoLumaMask_=lumaMask;camoLumaNativeT6_=lumaNativeT6;camoLumaLow_=lumaLow;camoLumaHigh_=lumaHigh;camoLumaGamma_=lumaGamma;camoLumaContrast_=lumaContrast; }
    void setWeaponMaterialTuning(float specularMultiplier,scene::Vec3 specularLow,scene::Vec3 specularHigh,float cubemapMultiplier=1.0f,float weaponCubemapBlur=0.08f,float metalnessOverride=-1.f) noexcept { weaponSpecularMultiplier_=specularMultiplier;weaponSpecularLow_=specularLow;weaponSpecularHigh_=specularHigh;weaponCubemapMultiplier_=cubemapMultiplier;weaponCubemapBlur_=weaponCubemapBlur;weaponMetalnessOverride_=std::isfinite(metalnessOverride)?std::clamp(metalnessOverride,-1.f,1.f):-1.f; }
    void setSurfaceNormalReflectionInfluence(float influence) noexcept { surfaceNormalReflectionInfluence_=influence; }
    void setWeaponMetalnessFromDiffuse(bool enabled,float black=0,float white=1,float gamma=1) noexcept {
        weaponMetalnessFromDiffuse_=enabled;
        weaponMetalnessBlack_=std::isfinite(black)?std::clamp(black,0.f,1.f):0.f;
        weaponMetalnessWhite_=std::isfinite(white)?std::clamp(white,0.f,1.f):1.f;
        weaponMetalnessGamma_=std::isfinite(gamma)?std::clamp(gamma,.05f,5.f):1.f;
    }
    [[nodiscard]] float surfaceNormalReflectionInfluence() const noexcept { return surfaceNormalReflectionInfluence_; }
    void setCameraPosition(scene::Vec3 pos) noexcept { cameraPosition_=pos; }
    void setFirstPersonProjection(bool enabled) noexcept { firstPersonProjection_=enabled; }
    [[nodiscard]] scene::Vec3 cameraPosition() const noexcept { return cameraPosition_; }
    void setCampathAppearance(float thickness,scene::Vec3 splineColor,scene::Vec3 nodeColor,scene::Vec3 frustumColor,bool dashed=false,float dashLength=12.0f) noexcept { campathThickness_=thickness;campathSplineColor_=splineColor;campathNodeColor_=nodeColor;campathFrustumColor_=frustumColor;campathDashed_=dashed;campathDashLength_=dashLength; }
    void setMaterialParameters(float lensAlpha,scene::Vec3 lensTint,float lensCubemapIntensity,float specularIntensity,float specularSharpness,float specularIntensity2,float specularSharpness2,float lensSpecularIntensity,bool cubemapSpecular,float cubemapSpecularIntensity,float cubemapBlur,int shadingModel,bool iw3DualLobe,float awRoughnessScale,float awRoughnessBias,float awMetalness,float awSpecularLevel,float awDiffuseWrap,float awClearcoat,float awClearcoatRoughness,float awEnvironmentIntensity) noexcept { lensAlpha_=lensAlpha;lensTint_=lensTint;lensCubemapIntensity_=lensCubemapIntensity;specularIntensity_=specularIntensity;specularSharpness_=specularSharpness;specularIntensity2_=specularIntensity2;specularSharpness2_=specularSharpness2;lensSpecularIntensity_=lensSpecularIntensity;cubemapSpecular_=cubemapSpecular;cubemapSpecularIntensity_=cubemapSpecularIntensity;cubemapBlur_=cubemapBlur;shadingModel_=std::clamp(shadingModel,0,3);iw3DualLobe_=iw3DualLobe;awRoughnessScale_=awRoughnessScale;awRoughnessBias_=awRoughnessBias;awMetalness_=awMetalness;awSpecularLevel_=awSpecularLevel;awDiffuseWrap_=awDiffuseWrap;awClearcoat_=awClearcoat;awClearcoatRoughness_=awClearcoatRoughness;awEnvironmentIntensity_=awEnvironmentIntensity; }
    void setMaterialParameters(float lensAlpha,scene::Vec3 lensTint,float lensCubemapIntensity,float specularIntensity,float specularSharpness,float specularIntensity2,float specularSharpness2,float lensSpecularIntensity,bool cubemapSpecular,float cubemapSpecularIntensity,float cubemapBlur,int shadingModel,bool iw3DualLobe) noexcept { setMaterialParameters(lensAlpha,lensTint,lensCubemapIntensity,specularIntensity,specularSharpness,specularIntensity2,specularSharpness2,lensSpecularIntensity,cubemapSpecular,cubemapSpecularIntensity,cubemapBlur,shadingModel,iw3DualLobe,awRoughnessScale_,awRoughnessBias_,awMetalness_,awSpecularLevel_,awDiffuseWrap_,awClearcoat_,awClearcoatRoughness_,awEnvironmentIntensity_); }
    void setEeveeParameters(float metallic,float roughness,float ior,float specular,float specularTint,float clearcoat,float clearcoatRoughness) noexcept { eeveeMetallic_=metallic;eeveeRoughness_=roughness;eeveeIor_=ior;eeveeSpecular_=specular;eeveeSpecularTint_=specularTint;eeveeClearcoat_=clearcoat;eeveeClearcoatRoughness_=clearcoatRoughness; }
    void setBrdfModel(int model) noexcept { brdfModel_=std::clamp(model,0,10); }
    [[nodiscard]] int brdfModel() const noexcept { return brdfModel_; }
    void setTonemapping(int mode,bool rec709Match=false,float rec709Strength=1.0f) noexcept { tonemappingMode_=std::clamp(mode,0,4);tonemapRec709Match_=rec709Match;tonemapRec709Strength_=rec709Strength; }
    void setAlternateSpecular(bool enabled,float intensity1,float exponent1,float intensity2,float exponent2,int viewmodelProfile,int playerProfile,int worldProfile) noexcept { alternateSpecularEnabled_=enabled;alternateSpecularIntensity_=intensity1;alternateSpecularSharpness_=exponent1;alternateSpecularIntensity2_=intensity2;alternateSpecularSharpness2_=exponent2;viewmodelSpecularProfile_=viewmodelProfile;playerSpecularProfile_=playerProfile;worldSpecularProfile_=worldProfile; }
    void setEmissiveTint(scene::Vec3 tint) noexcept { emissiveTint_=tint; }
    void setLensTintIntensity(float intensity) noexcept { lensTintIntensity_=intensity; }
    void setWorldLensParameters(float alpha,scene::Vec3 tint,float tintIntensity,float directSpecular,float cubemapIntensity) noexcept { worldLensAlpha_=alpha;worldLensTint_=tint;worldLensTintIntensity_=tintIntensity;worldLensSpecularIntensity_=directSpecular;worldLensCubemapIntensity_=cubemapIntensity; }
    void setKawaseSamples(int bloomSamples,int cubemapSamples) noexcept { bloomKawaseSamples_=std::clamp(bloomSamples,2,16);cubemapKawaseSamples_=std::clamp(cubemapSamples,2,16); }
    void setFilmTweaks(bool enabled,float brightness,float contrast,float desaturation,scene::Vec3 darkTint,scene::Vec3 midTint,scene::Vec3 lightTint,bool midTintEnabled,bool invert) noexcept { filmEnabled_=enabled;filmBrightness_=brightness;filmContrast_=contrast;filmDesaturation_=desaturation;filmDarkTint_=darkTint;filmMidTint_=midTint;filmLightTint_=lightTint;filmMidTintEnabled_=midTintEnabled;filmInvert_=invert; }
    void setBloomBlendMode(int mode) noexcept { bloomBlendMode_=std::clamp(mode,0,4); }
    [[nodiscard]] int bloomBlendMode() const noexcept { return bloomBlendMode_; }
    void setHbao(HbaoSettings settings) noexcept { settings.sanitize();hbao_=settings; }
    void setDepthOfField(DepthOfFieldSettings settings) noexcept { settings.sanitize();dof_=settings; }
    void setPostPassOrder(const std::array<int,7>& order) noexcept { postPassOrder_=order; }
    [[nodiscard]] const std::array<int,7>& postPassOrder() const noexcept { return postPassOrder_; }
    void setLensTweaks(bool bloomEnabled,float bloomThreshold,float bloomSoftThreshold,float bloomSaturationBias,float bloomAspect,float bloomRotation,float bloomIntensity,bool vignetteEnabled,float vignetteIntensity,float vignetteRadius,float vignetteSoftness) noexcept { bloomEnabled_=bloomEnabled;bloomThreshold_=bloomThreshold;bloomSoftThreshold_=bloomSoftThreshold;bloomSaturationBias_=bloomSaturationBias;bloomAspect_=bloomAspect;bloomRotation_=bloomRotation;bloomIntensity_=bloomIntensity;vignetteEnabled_=vignetteEnabled;vignetteIntensity_=vignetteIntensity;vignetteRadius_=vignetteRadius;vignetteSoftness_=vignetteSoftness; }
    void setPostEffects(float bloomRadius,float lensDistortion,float lutIntensity,bool autoBlack,float autoBlackIntensity,bool autoWhite,float autoWhiteIntensity) noexcept { bloomRadius_=bloomRadius;lensDistortion_=lensDistortion;lutIntensity_=lutIntensity;autoBlackPoint_=autoBlack;autoBlackPointIntensity_=autoBlackIntensity;autoWhitePoint_=autoWhite;autoWhitePointIntensity_=autoWhiteIntensity; }
    void setPostEffects(float bloomRadius,float lensDistortion) noexcept { bloomRadius_=bloomRadius;lensDistortion_=lensDistortion; }
    [[nodiscard]] bool setLutTexture(const std::filesystem::path& path,std::string& error);
    void clearLutTexture();
    void setFog(bool enabled,scene::Vec3 color,float start,float halfDistance,float opacity,float skyAmount,bool heightEnabled=false,float height=0,float heightFalloff=1) noexcept { fogEnabled_=enabled;fogColor_=color;fogStart_=start;fogHalfDistance_=halfDistance;fogOpacity_=opacity;fogSkyAmount_=skyAmount;fogHeightEnabled_=heightEnabled;fogHeight_=height;fogHeightFalloff_=std::max(1.0f,heightFalloff); }
    void setCameraDepthRange(float nearPlane,float farPlane) noexcept { cameraNear_=std::max(.0001f,nearPlane);cameraFar_=std::max(cameraNear_+.001f,farPlane); }
    void setDebugDepthRange(float nearDistance,float farDistance,bool invert) noexcept { debugDepthNear_=std::max(0.0f,nearDistance);debugDepthFar_=std::max(debugDepthNear_+.001f,farDistance);debugDepthInvert_=invert; }
    void setShadowStyle(float intensity,float bias,float normalBias,float softness,float contrast,scene::Vec3 tint) noexcept { shadowIntensity_=std::clamp(intensity,0.0f,1.0f);shadowBias_=std::max(.00001f,bias);shadowNormalBias_=std::max(0.0f,normalBias);shadowSoftness_=std::max(.1f,softness);shadowContrast_=std::max(.01f,contrast);shadowTint_=tint; }
    void setShadowSpecularMultiplier(float val) noexcept { shadowSpecularMultiplier_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float shadowSpecularMultiplier() const noexcept { return shadowSpecularMultiplier_; }
    void setNavigationAppearance(scene::Vec3 link,scene::Vec3 node,scene::Vec3 priority,float thickness,float interpolation,float handleHeight,int circleSides,float circleScale,float crossLength) noexcept { navigationLinkColor_=link;navigationNodeColor_=node;navigationPriorityColor_=priority;navigationLineThickness_=std::clamp(thickness,.5f,12.0f);navigationInterpolation_=std::clamp(interpolation,0.0f,1.0f);navigationHandleHeight_=handleHeight;navigationCircleSides_=std::clamp(circleSides,3,64);navigationCircleScale_=std::max(.05f,circleScale);navigationCrossLength_=std::max(0.0f,crossLength); }
    void setActorDebugOverlays(bool wallhack,scene::Vec3 visible,scene::Vec3 occluded,float wallThickness,bool hitboxes,scene::Vec3 hitbox,float hitboxThickness) noexcept { wallhack_=wallhack;wallhackVisibleColor_=visible;wallhackOccludedColor_=occluded;wallhackThickness_=wallThickness;hitboxes_=hitboxes;hitboxColor_=hitbox;hitboxThickness_=hitboxThickness; }
    void setAntialiasing(bool enabled) noexcept { msaaEnabled_=enabled; }
    void setAlphaOverlay(bool enabled,float intensity) noexcept { alphaOverlay_=enabled;alphaOverlayIntensity_=intensity; }
    void setDebugView(int mode) noexcept { collisionOverlayEnabled_=mode==10;debugView_=mode==10?1:mode; }
    void setEquipmentVisibility(const std::vector<std::uint8_t>& bots,bool player) { equipmentHiddenActors_=bots;equipmentHiddenPlayer_=player; }
    void setCollisionOverlay(const scene::glb::Map* map,float rangeMeters=30.f,bool throughWalls=false) noexcept { collisionOverlayMap_=map;collisionOverlayRange_=std::clamp(rangeMeters,1.f,200.f)*100;collisionOverlayThroughWalls_=throughWalls; }
    void setDecalDepthBias(float value) noexcept { decalDepthBias_=value; }
    void setNormalMapIntensity(float intensity) noexcept { normalMapIntensity_=std::max(0.0f,intensity); }
    void setIgnoreTextureAlpha(bool viewmodels,bool maps) noexcept { ignoreViewmodelTextureAlpha_=viewmodels;ignoreMapTextureAlpha_=maps; }
    [[nodiscard]] bool setEnvironmentPanorama(const std::filesystem::path& path,std::string& error);
    [[nodiscard]] bool setT6SkyboxIwi(const std::filesystem::path& path,std::string& error);
    void clearEnvironment();
    void setEnvironmentParameters(float intensity,float exposure,float rotationDegrees) noexcept { environmentIntensity_=intensity;environmentExposure_=exposure;environmentRotation_=rotationDegrees; }
    void setEnvironmentParameters(float intensity,float exposure,float rotationDegrees,float contrast,float highlightBoost,float highlightThreshold,float toneMap) noexcept { environmentIntensity_=intensity;environmentExposure_=exposure;environmentRotation_=rotationDegrees;environmentContrast_=contrast;environmentHighlightBoost_=highlightBoost;environmentHighlightThreshold_=highlightThreshold;environmentToneMap_=toneMap; }
    void setSkyFaceCalibration(const std::array<int,6>& sources,const std::array<int,6>& quarterTurns) noexcept { skyFaceSources_=sources;skyFaceQuarterTurns_=quarterTurns; }
    void setSkyVerticalFlip(bool enabled) noexcept { skyVerticalFlip_=enabled; }
    void setEnvironmentCamera(scene::Vec3 forward,scene::Vec3 up,float fovDegrees,float aspect) noexcept { environmentForward_=forward;environmentUp_=up;environmentFov_=fovDegrees;environmentAspect_=aspect; }
    void setSun(bool enabled,bool shadows,scene::Vec3 direction,float intensity,float ambient,scene::Vec3 sunColor,scene::Vec3 ambientColor,int shadowResolution,float shadowDistance,float shadowFadeStart,scene::Vec3 shadowFocus,bool farEnabled,int farResolution,float farStart,float farDistance,float farBlend) noexcept { sunEnabled_=enabled;sunShadows_=shadows;sunDirection_=direction;sunIntensity_=intensity;sunAmbient_=ambient;sunColor_=sunColor;ambientColor_=ambientColor;requestedShadowResolution_=shadowResolution;shadowDistance_=shadowDistance;shadowFadeStart_=shadowFadeStart;shadowFocus_=shadowFocus;farShadowEnabled_=farEnabled;requestedFarShadowResolution_=farResolution;farShadowStart_=farStart;farShadowDistance_=farDistance;farShadowBlend_=farBlend; }
    void setViewmodelShadows(bool selfShadows,int resolution) noexcept { viewmodelSelfShadows_=selfShadows;requestedViewmodelShadowResolution_=resolution; }
    struct AttachmentCamoLuma {
        bool enabled{};
        bool useBaseColorLumaMask{};
        bool invertCamoMask{true};
        float camoLumaLow{};
        float camoLumaHigh{1.0f};
        float camoLumaGamma{1.0f};
        float camoLumaContrast{1.0f};
    };
    void setAttachmentCamoLuma(int attachmentIndex, bool useMask, bool invert, float low, float high, float gamma, float contrast) {
        attachmentCamoLumas_[attachmentIndex] = {true, useMask, invert, low, high, gamma, contrast};
    }
    void clearAttachmentCamoLumas() { attachmentCamoLumas_.clear(); }

    [[nodiscard]] std::uintptr_t colorTexture() const noexcept { return colorTexture_; }

    void renderMuzzleFlash3D(scene::Vec3 position, float size, float rotation, scene::Vec4 color, const scene::Mat4& viewProjection, scene::Vec3 cameraPos, bool firstPerson = true);
    void renderDebugLine3D(scene::Vec3 start, scene::Vec3 end, scene::Vec4 color, const scene::Mat4& viewProjection);
    void updateAndRenderSmokeCurve(float deltaSeconds, scene::Vec3 muzzlePos, scene::Vec3 muzzleForward, bool isEmitting, const scene::Mat4& viewProjection, scene::Vec3 cameraPos, float lifetime, float startWidth, float endWidth, float taperingExp, float blastSpeed, float riseSpeed, float dispersion, scene::Vec4 color, scene::Vec3 wind = {}, int curveResolution = 3,int emitter=0);
    void clearSmokeCurve();
    void renderReplaySmoke(scene::Vec3 origin,scene::Vec3 forward,float age,std::uint32_t seed,const ReplaySmokeSettings& settings,scene::Vec4 color,const scene::Mat4& viewProjection,scene::Vec3 cameraPos,int curveResolution);
    [[nodiscard]] bool setImpactSurfaceTexture(const std::filesystem::path& path, std::string& error);
    void clearImpactSurfaceTexture();
    [[nodiscard]] unsigned impactSurfaceTexture() const noexcept { return impactSurfaceTexture_; }
    [[nodiscard]] bool setImpactBotTexture(const std::filesystem::path& path, std::string& error);
    void clearImpactBotTexture();
    [[nodiscard]] unsigned impactBotTexture() const noexcept { return impactBotTexture_; }
    void spawnImpact(scene::Vec3 position, scene::Vec3 normal, bool isBot, float startWidth, float endWidth, float lifetime, float blastSpeed, float riseSpeed, float dispersion, float taperingExp, float feathering, scene::Vec4 color, int planeCount = 3, float texScale = 1.0f, float texOffsetX = 0.0f, float texOffsetY = 0.0f, float texRotation = 0.0f, float originY = 0.0f);
    void updateAndRenderImpacts(float deltaSeconds, const scene::Mat4& viewProjection, scene::Vec3 cameraPos);
    void clearImpacts();

    struct BulletTrail {
        enum class Type { Sniper, Projectile };
        Type type{Type::Projectile};
        scene::Vec3 start{};
        scene::Vec3 end{};
        scene::Vec3 dir{};
        float distance{};
        float currentDist{};
        float speed{14000.0f};
        float length{65.0f};
        float width{1.5f};
        float lifetime{0.8f};
        float age{0.0f};
        scene::Vec3 color{1.0f, 0.85f, 0.35f};
        float emissiveIntensity{4.0f};
        float feathering{0.25f};
        bool emissiveEnabled{true};
        gameplay::view::ResponseCurve taperCurve{};
        bool curveEnabled{false};
        float startTaper{1.0f};
        float endTaper{1.0f};
        bool useSprite{false};
    };
    void addBulletTrail(const BulletTrail& trail);
    void clearBulletTrails();
    void updateAndRenderBulletTrails(float deltaSeconds, const scene::Mat4& viewProjection, const scene::Vec3& cameraPos);
    [[nodiscard]] bool setBulletTrailTexture(const std::filesystem::path& path, std::string& error);
    void clearBulletTrailTexture();
    void setBulletTrailEdgeGradient(float darkening, float power, scene::Vec3 tint) noexcept { trailEdgeDarkening_ = std::clamp(darkening, 0.0f, 1.0f); trailEdgePower_ = std::max(0.01f, power); trailEdgeTint_ = tint; }
    [[nodiscard]] float bulletTrailEdgeDarkening() const noexcept { return trailEdgeDarkening_; }
    [[nodiscard]] float bulletTrailEdgePower() const noexcept { return trailEdgePower_; }
    [[nodiscard]] scene::Vec3 bulletTrailEdgeTint() const noexcept { return trailEdgeTint_; }

    struct RenderStats {
        int mapMeshesTotal{};
        int mapOpaqueMeshes{};
        int mapDecalMeshes{};
        int mapTransparentMeshes{};
        int mapAdditiveMeshes{};
        int mapShadowCasters{};
        int lastVisibleMapMeshes{};
        int lastShadowCasterDraws{};
        int lastFarShadowCasterDraws{};
        int lastTotalDrawCalls{};
        int materialRequests{},materialUploads{},emissionTextureBinds{};
        int poseRequests{},poseUploads{};
        int loadedTextureCount{};
        std::size_t estimatedVramBytes{};
        int vramTotalMb{};
        int vramFreeMb{};
        int mapTextureResolution{1024};
        bool hardwareCompressionEnabled{true};
    };
    [[nodiscard]] RenderStats renderStats(bool queryDriver=true) const noexcept;
    void setActorCullingEnabled(bool enabled) noexcept {actorCullingEnabled_=enabled;}
    // Diagnostic A/B switch; does not change draw order, quality, or presets.
    void setMeshStateCacheEnabled(bool enabled) noexcept {meshStateCacheEnabled_=enabled;}
    void setPoseUploadCacheEnabled(bool enabled) noexcept {poseUploadCacheEnabled_=enabled;}
    void setSortPreparationEnabled(bool enabled) noexcept {sortPreparationEnabled_=enabled;}

    void setMapTextureResolution(int maxDim) noexcept { mapTextureResolution_ = std::clamp(maxDim, 256, 4096); }
    [[nodiscard]] int mapTextureResolution() const noexcept { return mapTextureResolution_; }
    void setHardwareTextureCompression(bool enabled) noexcept { hardwareTextureCompression_ = enabled; }
    [[nodiscard]] bool hardwareTextureCompression() const noexcept { return hardwareTextureCompression_; }

    bool captureReShadeSwapchain(struct GLFWwindow* window, std::vector<std::uint8_t>& outPixels, std::string& error);
    unsigned loadTexture(const std::filesystem::path& path,bool sceneTexture=true,bool forceOpaque=false,int maxDimension=1024,bool compress=false,bool srgb=false);

private:
    void renderWater(const scene::Mat4&,const scene::Mat4&,const scene::Mat4&);
    water::Settings water_{};
    double waterTime_{};
    unsigned waterProgram_{},waterVao_{},waterVertices_{},waterIndices_{};
    unsigned waterSceneFramebuffer_{},waterSceneDepth_{},waterSceneColor_{};
    unsigned waterCrestTexture_{},waterFoamTexture_{};
    float waterCrestTightness_{-1.f};
    int waterSceneWidth_{},waterSceneHeight_{};
    bool waterSceneStencil_{};
    int waterResolution_{},waterIndexCount_{};
    std::string waterError_;
    bool renderVolumetricLighting(const scene::Mat4&,const scene::Mat4&,const scene::Mat4&,bool);
    VolumetricLightingSettings volumetric_{};
    std::string volumetricError_;
    unsigned volumetricProgram_{},volumetricFramebuffer_{},volumetricTexture_{};
    int volumetricWidth_{},volumetricHeight_{};
    GpuTimer gpuTimer_;
    float viewmodelCubemapMultiplier_{1.f},worldCubemapMultiplier_{1.f};
    unsigned uploadPreparedTexture(const texture::Prepared& prepared,double* transferMs=nullptr,double* mipmapMs=nullptr);
    int uniformLocation(unsigned program,std::string_view name);
    std::unordered_map<unsigned,std::map<std::string,int,std::less<>>> uniformLocations_;
    // Scratch storage retains capacity only; each producer clears before use.
    std::array<std::vector<float>,8> overlayScratch_;
    std::array<std::vector<float>,4> trailScratch_;
    std::vector<float> smokeVertexScratch_,impactVertexScratch_;
    struct RibbonSample { scene::Vec3 pos,normal;float width,alpha,u; };
    std::vector<RibbonSample> smokeSampleScratch_;
    std::vector<BulletTrail> bulletTrails_;
    unsigned bulletTrailTexture_{};
    struct ImpactPlaneDef {
        scene::Vec3 axisX{};
        scene::Vec3 axisY{};
    };
    struct ImpactBurst {
        scene::Vec3 position{};
        scene::Vec3 normal{};
        scene::Vec3 velocity{};
        scene::Vec4 color{};
        float age{0.0f};
        float lifetime{0.4f};
        float startWidth{3.0f};
        float endWidth{14.0f};
        float taperingExp{1.2f};
        float feathering{0.8f};
        float dispersion{12.0f};
        bool isBot{false};
        float texScale{1.0f};
        float texOffsetX{0.0f};
        float texOffsetY{0.0f};
        float texRotation{0.0f};
        float originY{0.0f};
        std::vector<ImpactPlaneDef> planes;
    };
    std::vector<ImpactBurst> impactBursts_;
    unsigned impactSurfaceTexture_{};
    unsigned impactBotTexture_{};

    using SmokePoint=SmokeCurvePoint;
    std::vector<SmokePoint> replaySmokeScratch_;
    void renderSmokePoints(const std::vector<SmokePoint>& points,const scene::Mat4& viewProjection,scene::Vec3 cameraPos,scene::Vec4 color,int curveResolution);
    struct SmokeEmitter {std::vector<SmokePoint> points;float taper{};scene::Vec3 lastPosition{};bool hasLastPosition{};float timer{};};
    std::array<SmokeEmitter,2> smokeEmitters_;

    struct GpuMesh { unsigned vao{},vertexBuffer{},indexBuffer{},wireIndexBuffer{},texture{},normalTexture{},specularTexture{},metalnessTexture{},roughnessTexture{},emissiveTexture{}; int indexCount{},wireIndexCount{}; scene::Mat4 model; scene::Vec4 color; bool specularGlossiness{};bool skinned{},camoBlend{},camoUseAlpha{true},camoMaskUseful{},hideWhenCamo{},lens{},eyeOverlay{},emissive{},forceAlpha{},decal{},decalMultiply{},decalAdditive{},alphaTest{},ignoreAlbedoAlpha{},source2Material{},viewmodelWeapon{},gltfPbr{},materialPolicyExplicit{},doubleSided{},unlit{},useVertexColor{}; float alphaCutoff{.35f},materialDepthBias{};int renderQueue{-1},sourceBlend{-1},destinationBlend{-1}; float metallicFactor{},roughnessFactor{1.0f},transmissionFactor{},indexOfRefraction{1.5f};scene::Vec3 emissiveFactor{};std::int32_t attachmentIndex{-1},actorVariant{-1}; SurfaceSortKey sortKey; visibility::MeshBounds cullBounds; scene::Vec3 aabbMin{}, aabbMax{}; bool hasBounds{false}; };
    void setShadowMaterial(const GpuMesh& mesh);
    struct MeshUniformLocations {
        int explicitMaterial{-1},unlit{-1},vertexColor{-1},alphaCutoff{-1},hasEmissionMap{-1};
        int model{-1},color{-1},ignoreAlpha{-1},viewmodelSurface{-1},specularGlossiness{-1},gltfPbr{-1},gltfMetallic{-1},gltfRoughness{-1},gltfTransmission{-1},gltfIor{-1},gltfEmissive{-1},skinned{-1},hasAlbedo{-1},camoLumaMask{-1},camoLumaLow{-1},camoLumaHigh{-1},camoLumaGamma{-1},camoLumaContrast{-1},camoInvert{-1},hasNormalMap{-1},normalIntensity{-1},hasSpecularMap{-1},hasMetalnessMap{-1},source2Material{-1},hasRoughnessMap{-1},lens{-1},eyeOverlay{-1},emissive{-1},forceAlpha{-1},decal{-1},decalMultiply{-1},decalAdditive{-1},alphaTest{-1},hasSpecularImperfections{-1},camoBlend{-1},camoUseAlpha{-1},camoMaskUseful{-1},viewmodelLightViewProjection{-1},weaponSurface{-1},weaponSpecularMultiplier{-1},weaponSpecularLow{-1},weaponSpecularHigh{-1},weaponCubemapMultiplier{-1},specularIntensity{-1},specularSharpness{-1},specularIntensity2{-1},specularSharpness2{-1},lensAlpha{-1},lensTint{-1},lensTintIntensity{-1},lensSpecularIntensity{-1},lensCubemapIntensity{-1},eeveeMetallic{-1},eeveeRoughness{-1},eeveeIor{-1},eeveeSpecular{-1},eeveeSpecularTint{-1},eeveeClearcoat{-1},eeveeClearcoatRoughness{-1},brdfModel{-1},shadowSpecularMultiplier{-1};
    } meshUniforms_{};
    std::unordered_map<int, AttachmentCamoLuma> attachmentCamoLumas_;
    struct ShadowUniformLocations {
        int lightViewProjection{-1},bones{-1},boneVisibility{-1},model{-1},skinned{-1};
    } shadowUniforms_{};
    bool resizeTarget(int width,int height);
    bool resizeMsaaTarget(int width,int height);
    void releaseMsaaTarget();
    bool resizeShadowTarget(int resolution);
    bool resizeViewmodelShadowTarget(int resolution);
    bool resizeFarShadowTarget(int resolution);
    void releaseTarget();
    bool renderHbao(const scene::Mat4& viewProjection,bool foreground);
    void configureAoFog(unsigned program,const scene::Mat4& viewProjection,bool foreground);
    bool renderDepthOfField(bool foreground,bool ao,bool isolation);
    unsigned compileProgram(const char* vertex,const char* fragment,std::string& error);
    void drawLines(const std::vector<float>& vertices,const scene::Mat4& viewProjection);
    void drawTriangles(const std::vector<float>& vertices,const scene::Mat4& viewProjection);
    bool appendSceneMeshes(const scene::CastScene& scene,std::string& error,std::size_t firstSourceMesh=0,bool generateWireframe=true,bool isMapScene=false,ProgressCallback progressCallback=nullptr);

    bool initialized_{};
    int width_{},height_{};
    unsigned framebuffer_{},colorTexture_{},depthBuffer_{};
    unsigned msaaFramebuffer_{},msaaColorBuffer_{},msaaDepthBuffer_{};
    int msaaWidth_{},msaaHeight_{};bool msaaEnabled_{true};
    unsigned meshProgram_{},lineProgram_{},shadowProgram_{},environmentProgram_{},postProgram_{},kawaseProgram_{},billboardProgram_{};
    HbaoSettings hbao_{};
    DepthOfFieldSettings dof_{};
    unsigned dofProgram_{},dofFramebuffer_{},aoIsolationFramebuffer_{},aoIsolationTexture_{};
    std::array<unsigned,2> dofTextures_{};int dofWidth_{},dofHeight_{};
    unsigned hbaoProgram_{},hbaoFramebuffer_{};std::array<unsigned,2> hbaoTextures_{};
    unsigned postFramebuffer_{},postTexture_{},bloomFramebuffer_{},lutTexture_{};std::array<unsigned,6> bloomTextures_{};std::array<int,6> bloomWidths_{},bloomHeights_{};int lutSize_{};
    unsigned lineVao_{},lineBuffer_{},billboardVao_{},billboardBuffer_{};
    unsigned boneTexture_{},boneBuffer_{},visibilityTexture_{},visibilityBuffer_{};
    unsigned camoTexture_{};
    std::unordered_map<unsigned,bool> textureHasUsefulAlpha_;
    unsigned scopeOverlayTexture_{};
    float scopeOverlayAspect_{1.0f};
    scene::Vec3 scopeOverlayEdgeColor_{};
    unsigned muzzleFlashTexture_{};
    unsigned specularImperfectionsTexture_{};
    float imperfectionStrength_{},imperfectionScale_{1.0f},imperfectionLow_{},imperfectionHigh_{1.0f};
    unsigned environmentTexture_{};
    std::array<unsigned,6> environmentFaces_{};
    int environmentMode_{};
    daynight::Settings dayNight_{};
    NightSkySettings nightSky_{};
    unsigned nightPanorama_{};
    std::filesystem::path nightPanoramaPath_;
    double dayNightTime_{};
    float environmentIntensity_{1.0f},environmentExposure_{},environmentRotation_{},environmentContrast_{1.0f},environmentHighlightBoost_{1.0f},environmentHighlightThreshold_{.75f},environmentToneMap_{1.0f};
    scene::Vec3 environmentForward_{1,0,0},environmentUp_{0,0,1};
    float environmentFov_{80.0f},environmentAspect_{16.0f/9.0f};
    unsigned shadowFramebuffer_{},shadowTexture_{};
    int shadowResolution_{},requestedShadowResolution_{2048};
    unsigned viewmodelShadowFramebuffer_{},viewmodelShadowTexture_{};int viewmodelShadowResolution_{},requestedViewmodelShadowResolution_{1024};bool viewmodelSelfShadows_{true};
    unsigned farShadowFramebuffer_{},farShadowTexture_{};
    int farShadowResolution_{},requestedFarShadowResolution_{1024};
    float camoStrength_{0.77f},camoScale_{1.0f},camoAlphaLow_{0.031f},camoAlphaHigh_{1.0f},camoRotation_{},camoLumaLow_{},camoLumaHigh_{1},camoLumaGamma_{1},camoLumaContrast_{1};scene::Vec3 camoOffset_{};
    bool weaponMetalnessFromDiffuse_{};float weaponMetalnessBlack_{},weaponMetalnessWhite_{1.f},weaponMetalnessGamma_{1.f};
    float weaponSpecularMultiplier_{1.0f},weaponCubemapMultiplier_{1.0f},weaponCubemapBlur_{0.08f},weaponMetalnessOverride_{-1.f};scene::Vec3 weaponSpecularLow_{},weaponSpecularHigh_{1,1,1};
    bool camoInvert_{true},universalCamo_{},camoLumaMask_{},camoLumaNativeT6_{};
    float lensAlpha_{0.60f},lensCubemapIntensity_{1.0f},specularIntensity_{1.0f},specularSharpness_{256.0f},specularIntensity2_{0.18f},specularSharpness2_{32.0f},lensSpecularIntensity_{4.0f},cubemapSpecularIntensity_{0.35f},cubemapBlur_{0.08f},lensTintIntensity_{1.0f};scene::Vec3 lensTint_{1,1,1},emissiveTint_{1,1,1};float worldLensAlpha_{.6f},worldLensTintIntensity_{1},worldLensSpecularIntensity_{4},worldLensCubemapIntensity_{1};scene::Vec3 worldLensTint_{1,1,1};
    bool alternateSpecularEnabled_{};float alternateSpecularIntensity_{1.0f},alternateSpecularSharpness_{256.0f},alternateSpecularIntensity2_{0.18f},alternateSpecularSharpness2_{32.0f};int viewmodelSpecularProfile_{},playerSpecularProfile_{},worldSpecularProfile_{};
    float awRoughnessScale_{1.0f},awRoughnessBias_{},awMetalness_{},awSpecularLevel_{1.0f},awDiffuseWrap_{},awClearcoat_{0.20f},awClearcoatRoughness_{0.18f},awEnvironmentIntensity_{1.0f};
    float eeveeMetallic_{0.0f},eeveeRoughness_{0.5f},eeveeIor_{1.45f},eeveeSpecular_{0.5f},eeveeSpecularTint_{0.0f},eeveeClearcoat_{0.0f},eeveeClearcoatRoughness_{0.03f};
    int tonemappingMode_{0};
    bool tonemapRec709Match_{false};
    float tonemapRec709Strength_{1.0f};
    int bloomBlendMode_{0};
    std::array<int,7> postPassOrder_{0,1,2,3,4,5,6};
    bool cubemapSpecular_{},iw3DualLobe_{true},alphaOverlay_{};
    float alphaOverlayIntensity_{1.0f};
    int debugView_{};
    std::vector<std::uint8_t> equipmentHiddenActors_;
    bool equipmentHiddenPlayer_{};
    const scene::glb::Map* collisionOverlayMap_{};
    bool collisionOverlayEnabled_{},collisionOverlayThroughWalls_{};
    float collisionOverlayRange_{3000};
    int shadingModel_{1};
    int brdfModel_{0};
    bool filmEnabled_{};float filmBrightness_{},filmContrast_{1.0f},filmDesaturation_{};scene::Vec3 filmDarkTint_{1,1,1},filmMidTint_{1,1,1},filmLightTint_{1,1,1};bool filmMidTintEnabled_{},filmInvert_{};
    int bloomKawaseSamples_{8},cubemapKawaseSamples_{8};bool bloomEnabled_{},vignetteEnabled_{},autoBlackPoint_{true},autoWhitePoint_{true};float bloomThreshold_{1.0f},bloomSoftThreshold_{.1f},bloomSaturationBias_{},bloomAspect_{1.0f},bloomRotation_{},bloomIntensity_{},bloomRadius_{2.0f},vignetteIntensity_{},vignetteRadius_{0.75f},vignetteSoftness_{0.25f},lensDistortion_{},lutIntensity_{1.0f},autoBlackPointIntensity_{},autoWhitePointIntensity_{1.0f};
    unsigned smokeTexture_{};
    float smokeFeathering_{0.35f};
    bool fogEnabled_{},fogHeightEnabled_{};scene::Vec3 fogColor_{0.5f,0.55f,0.62f};float fogStart_{1000.0f},fogHalfDistance_{5000.0f},fogOpacity_{1.0f},fogSkyAmount_{},fogHeight_{},fogHeightFalloff_{1.0f},cameraNear_{.1f},cameraFar_{100000},debugDepthNear_{10.0f},debugDepthFar_{10000.0f};bool debugDepthInvert_{};
    float decalDepthBias_{6.0f};
    std::array<int,6> skyFaceSources_{0,1,3,2,4,5};
    std::array<int,6> skyFaceQuarterTurns_{};
    bool skyVerticalFlip_{};
    bool ignoreViewmodelTextureAlpha_{true},ignoreMapTextureAlpha_{};
    bool firstPersonProjection_{},foregroundDrawn_{};
    bool viewmodelCapture_{};scene::Vec4 captureBackground_{0,1,0,1};
    float normalMapIntensity_{1.0f};
    float surfaceNormalReflectionInfluence_{1.0f};
    scene::Vec3 cameraPosition_{};
    bool sunEnabled_{},sunShadows_{true};
    scene::Vec3 sunDirection_{-0.45f,-0.35f,-0.82f};
    scene::Vec3 sunColor_{1,1,1},ambientColor_{1,1,1};
    float sunIntensity_{1.0f},sunAmbient_{0.34f},shadowIntensity_{1.0f},shadowBias_{.0015f},shadowNormalBias_{.02f},shadowSoftness_{1.0f},shadowContrast_{1.0f};scene::Vec3 shadowTint_{};
    float shadowSpecularMultiplier_{0.25f};
    float shadowDistance_{5000.0f},shadowFadeStart_{3500.0f};
    bool farShadowEnabled_{};float farShadowStart_{4000.0f},farShadowDistance_{12000.0f},farShadowBlend_{800.0f};
    scene::Vec3 shadowFocus_{};
    bool overlayTargetStencil_{},overlayMsaaStencil_{};bool overlayReplay_{};ActorOverlaySettings actorOverlays_{};
    ActorOverlayHistory overlayHistory_;
    double overlayTime_{};
    std::vector<ActorOverlayInput> overlayInputs_;
    ActorOverlayInput overlayPlayer_{};
    struct OverlayImpact{scene::Vec3 position;double time;};
    std::deque<OverlayImpact> overlayImpacts_;
    scene::Vec3 navigationLinkColor_{.15f,.72f,1},navigationNodeColor_{.1f,.9f,1},navigationPriorityColor_{1,.82f,.12f},wallhackVisibleColor_{.15f,1,.25f},wallhackOccludedColor_{1,.1f,.15f},hitboxColor_{1,.75f,.1f};float navigationLineThickness_{2},navigationInterpolation_{},navigationHandleHeight_{20},navigationCircleScale_{1},navigationCrossLength_{8},wallhackThickness_{2},hitboxThickness_{1.5f};int navigationCircleSides_{20};bool wallhack_{},hitboxes_{};
    scene::Vec3 campathSplineColor_{0.18f,0.85f,1.0f},campathNodeColor_{1.0f,0.78f,0.15f},campathFrustumColor_{0.25f,0.95f,0.65f};
    float campathThickness_{2.5f},campathDashLength_{12.0f};
    bool campathDashed_{false};
    void* imageFactory_{};
    bool ownsCom_{};
    std::vector<unsigned> textures_;
    std::unordered_map<std::wstring,unsigned> sceneTextureCache_;
    std::vector<GpuMesh> meshes_;
    std::vector<std::size_t> mapOpaqueMeshes_,mapDecalMeshes_,mapTransparentMeshes_,mapAdditiveMeshes_,mapShadowCasterMeshes_;
    std::size_t mainMeshCount_{},mapMeshFirst_{},mapMeshCount_{},worldActorMeshFirst_{},worldActorMeshCount_{},actorMeshFirst_{},actorMeshCount_{};
    std::array<std::size_t,3> classMainFirst_{},classMainCount_{},classWorldFirst_{},classWorldCount_{};
    int activeClassSlot_{};
    bool classScenesResident_{};
    int lastVisibleMapMeshes_{},lastShadowCasterDraws_{},lastFarShadowCasterDraws_{},lastTotalDrawCalls_{};
    bool actorCullingEnabled_{true};
    struct ActorBoundsEntry {
        const scene::CastScene* scene{};const std::vector<scene::Mat4>* pose{};
        std::size_t first{},count{};int variant{};scene::Bounds bounds;
    };
    std::vector<ActorBoundsEntry> actorBoundsScratch_;
    bool meshStateCacheEnabled_{true};
    // Scratch GPU copies only. Entries are keyed anew on EVERY render call:
    // recorded poses, simulation state and replay data are never modified.
    struct PoseUpload {
        const scene::CastScene* scene{};
        const std::vector<scene::Mat4>* pose{};
        unsigned bones{},boneTexture{},visibility{},visibilityTexture{};
        std::vector<scene::Mat4> skin;
        std::vector<float> shown;
        VisibilityPreparation visibilityState;
    };
    std::vector<PoseUpload> poseUploads_;
    std::vector<float> sortDistances_;
    std::vector<std::size_t> fadedWorldOrder_;
    bool sortPreparationEnabled_{true};
    mutable DriverPollInterval driverPoll_;
    mutable int driverTotalMb_{},driverFreeMb_{};
    bool poseUploadCacheEnabled_{true};
    int lastPoseRequests_{},lastPoseUploads_{};
    int lastMaterialRequests_{},lastMaterialUploads_{},lastEmissionTextureBinds_{};
    std::size_t estimatedVramBytes_{};
    int mapTextureResolution_{1024};
    bool hardwareTextureCompression_{true};
    float trailEdgeDarkening_{0.70f};
    float trailEdgePower_{2.5f};
    scene::Vec3 trailEdgeTint_{0.80f, 0.35f, 0.08f};
};

} // namespace render
