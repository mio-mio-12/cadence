// Resource uploads are deferred until the entire settings stream is valid.
struct VisualPresetResources {bool sky{},lut{},impactSurface{},impactBot{},weaponFx{};};
void copyVisualPresetSettings(AppState& to,const AppState& from){
    to.actorOverlays=from.actorOverlays;
    to.ambientBlackbody=from.ambientBlackbody;
    to.ambientColor=from.ambientColor;
    to.autoBlackPoint=from.autoBlackPoint;
    to.autoBlackPointIntensity=from.autoBlackPointIntensity;
    to.autoWhitePoint=from.autoWhitePoint;
    to.autoWhitePointIntensity=from.autoWhitePointIntensity;
    to.awClearcoat=from.awClearcoat;
    to.awClearcoatRoughness=from.awClearcoatRoughness;
    to.awDiffuseWrap=from.awDiffuseWrap;
    to.awEnvironmentIntensity=from.awEnvironmentIntensity;
    to.awMetalness=from.awMetalness;
    to.awRoughnessBias=from.awRoughnessBias;
    to.awRoughnessScale=from.awRoughnessScale;
    to.awSpecularLevel=from.awSpecularLevel;
    to.bloomAffectsSky=from.bloomAffectsSky;
    to.bloomAspectRatio=from.bloomAspectRatio;
    to.bloomBlendMode=from.bloomBlendMode;
    to.bloomEnabled=from.bloomEnabled;
    to.bloomIntensity=from.bloomIntensity;
    to.bloomKawaseSamples=from.bloomKawaseSamples;
    to.bloomRadius=from.bloomRadius;
    to.bloomRotation=from.bloomRotation;
    to.bloomSaturationBias=from.bloomSaturationBias;
    to.bloomSoftThreshold=from.bloomSoftThreshold;
    to.bloomThreshold=from.bloomThreshold;
    to.boostCamera=from.boostCamera;
    to.boostCameraMovesViewmodel=from.boostCameraMovesViewmodel;
    to.boostFallback=from.boostFallback;
    to.bulletTrailTexturePath=from.bulletTrailTexturePath;
    to.cameraControls=from.cameraControls;
    to.csgoCrosshairHorizontalSpacing=from.csgoCrosshairHorizontalSpacing;
    to.csgoCrosshairMarkLength=from.csgoCrosshairMarkLength;
    to.csgoCrosshairMarkOffset=from.csgoCrosshairMarkOffset;
    to.csgoCrosshairSideMarks=from.csgoCrosshairSideMarks;
    to.csgoCrosshairVerticalSpacing=from.csgoCrosshairVerticalSpacing;
    to.cubemapSpecular=from.cubemapSpecular;
    to.cubemapSpecularBlur=from.cubemapSpecularBlur;
    to.cubemapSpecularIntensity=from.cubemapSpecularIntensity;
    to.directionalTilt=from.directionalTilt;
    to.dof=from.dof;
    to.eeveeClearcoat=from.eeveeClearcoat;
    to.eeveeClearcoatRoughness=from.eeveeClearcoatRoughness;
    to.eeveeIor=from.eeveeIor;
    to.eeveeMetallic=from.eeveeMetallic;
    to.eeveeRoughness=from.eeveeRoughness;
    to.eeveeSpecular=from.eeveeSpecular;
    to.eeveeSpecularTint=from.eeveeSpecularTint;
    to.environmentContrast=from.environmentContrast;
    to.environmentExposure=from.environmentExposure;
    to.environmentHighlightBoost=from.environmentHighlightBoost;
    to.environmentHighlightThreshold=from.environmentHighlightThreshold;
    to.environmentIntensity=from.environmentIntensity;
    to.environmentPath=from.environmentPath;
    to.environmentRotation=from.environmentRotation;
    to.environmentToneMap=from.environmentToneMap;
    to.experimentalUniversalFirstPersonMantle=from.experimentalUniversalFirstPersonMantle;
    to.filmBrightness=from.filmBrightness;
    to.filmContrast=from.filmContrast;
    to.filmDarkTint=from.filmDarkTint;
    to.filmDesaturation=from.filmDesaturation;
    to.filmInvert=from.filmInvert;
    to.filmLightTint=from.filmLightTint;
    to.filmMidTint=from.filmMidTint;
    to.filmMidTintEnabled=from.filmMidTintEnabled;
    to.filmTweaks=from.filmTweaks;
    to.fogColor=from.fogColor;
    to.fogEnabled=from.fogEnabled;
    to.fogHalfDistanceMeters=from.fogHalfDistanceMeters;
    to.fogOpacity=from.fogOpacity;
    to.fogSkyAmount=from.fogSkyAmount;
    to.fogStartMeters=from.fogStartMeters;
    to.hbao=from.hbao;
    to.impactBotBlastSpeed=from.impactBotBlastSpeed;
    to.impactBotColor=from.impactBotColor;
    to.impactBotDispersion=from.impactBotDispersion;
    to.impactBotEnabled=from.impactBotEnabled;
    to.impactBotEndWidth=from.impactBotEndWidth;
    to.impactBotFeathering=from.impactBotFeathering;
    to.impactBotLifetime=from.impactBotLifetime;
    to.impactBotOriginY=from.impactBotOriginY;
    to.impactBotPath=from.impactBotPath;
    to.impactBotPlanesCount=from.impactBotPlanesCount;
    to.impactBotRiseSpeed=from.impactBotRiseSpeed;
    to.impactBotStartWidth=from.impactBotStartWidth;
    to.impactBotTaperingExp=from.impactBotTaperingExp;
    to.impactBotTexOffsetX=from.impactBotTexOffsetX;
    to.impactBotTexOffsetY=from.impactBotTexOffsetY;
    to.impactBotTexRotation=from.impactBotTexRotation;
    to.impactBotTexScale=from.impactBotTexScale;
    to.impactSurfaceBlastSpeed=from.impactSurfaceBlastSpeed;
    to.impactSurfaceColor=from.impactSurfaceColor;
    to.impactSurfaceDispersion=from.impactSurfaceDispersion;
    to.impactSurfaceEnabled=from.impactSurfaceEnabled;
    to.impactSurfaceEndWidth=from.impactSurfaceEndWidth;
    to.impactSurfaceFeathering=from.impactSurfaceFeathering;
    to.impactSurfaceLifetime=from.impactSurfaceLifetime;
    to.impactSurfaceOriginY=from.impactSurfaceOriginY;
    to.impactSurfacePath=from.impactSurfacePath;
    to.impactSurfacePlanesCount=from.impactSurfacePlanesCount;
    to.impactSurfaceRiseSpeed=from.impactSurfaceRiseSpeed;
    to.impactSurfaceStartWidth=from.impactSurfaceStartWidth;
    to.impactSurfaceTaperingExp=from.impactSurfaceTaperingExp;
    to.impactSurfaceTexOffsetX=from.impactSurfaceTexOffsetX;
    to.impactSurfaceTexOffsetY=from.impactSurfaceTexOffsetY;
    to.impactSurfaceTexRotation=from.impactSurfaceTexRotation;
    to.impactSurfaceTexScale=from.impactSurfaceTexScale;
    to.iw3DualLobe=from.iw3DualLobe;
    to.lensAlpha=from.lensAlpha;
    to.lensCubemapIntensity=from.lensCubemapIntensity;
    to.lensDistortion=from.lensDistortion;
    to.lensSpecularIntensity=from.lensSpecularIntensity;
    to.lensTint=from.lensTint;
    to.lutIntensity=from.lutIntensity;
    to.lutPath=from.lutPath;
    to.mantleFallbackCurve=from.mantleFallbackCurve;
    to.mantleFallbackPositionIw=from.mantleFallbackPositionIw;
    to.mantleFallbackRecovery=from.mantleFallbackRecovery;
    to.mantleFallbackRotationDegrees=from.mantleFallbackRotationDegrees;
    to.muzzleFlashPath=from.muzzleFlashPath;
    to.postPassOrder=from.postPassOrder;
    to.projectileTrailTaper=from.projectileTrailTaper;
    to.shadingModel=from.shadingModel;
    to.shadingSubModel=from.shadingSubModel;
    to.shadowSpecularMultiplier=from.shadowSpecularMultiplier;
    to.skyVerticalFlip=from.skyVerticalFlip;
    to.slideFallback=from.slideFallback;
    to.smokeTrailTexturePath=from.smokeTrailTexturePath;
    to.sniperTrailTaper=from.sniperTrailTaper;
    to.sniperZoomIn=from.sniperZoomIn;
    to.sniperZoomOut=from.sniperZoomOut;
    to.specularIntensity=from.specularIntensity;
    to.specularIntensity2=from.specularIntensity2;
    to.specularSharpness=from.specularSharpness;
    to.specularSharpness2=from.specularSharpness2;
    to.sunAmbient=from.sunAmbient;
    to.sunAzimuth=from.sunAzimuth;
    to.sunBlackbody=from.sunBlackbody;
    to.sunColor=from.sunColor;
    to.sunElevation=from.sunElevation;
    to.sunIntensity=from.sunIntensity;
    to.sunLighting=from.sunLighting;
    to.sunShadows=from.sunShadows;
    to.tonemapRec709Match=from.tonemapRec709Match;
    to.tonemapRec709Strength=from.tonemapRec709Strength;
    to.tonemappingMode=from.tonemappingMode;
    to.tritiumTint=from.tritiumTint;
    to.universalFirstPersonMantleScale=from.universalFirstPersonMantleScale;
    to.viewmodelCubemapMultiplier=from.viewmodelCubemapMultiplier;
    to.vignetteEnabled=from.vignetteEnabled;
    to.vignetteIntensity=from.vignetteIntensity;
    to.vignetteRadius=from.vignetteRadius;
    to.vignetteSoftness=from.vignetteSoftness;
    to.volumetric=from.volumetric;
    static_cast<render::water::Appearance&>(to.water)=from.water;
    to.nightSky=from.nightSky;
    to.dayNight=from.dayNight;to.dayNightSeconds=0;
    to.weaponCubemapBlur=from.weaponCubemapBlur;
    to.worldCubemapMultiplier=from.worldCubemapMultiplier;
    crosshairValues(to)=crosshairValues(from);
    actorOverlayValues(to)=actorOverlayValues(from);
    weapon_flashValues(to)=weapon_flashValues(from);
    weapon_smokeValues(to)=weapon_smokeValues(from);
    weapon_trailsValues(to)=weapon_trailsValues(from);
}
bool parseVisualPreset(AppState& app,std::istream& in,VisualPresetResources& resources){std::string signature,key;int version{};if(!(in>>signature>>version)||signature!="CASTVISUAL"||version<1||version>5)return false;app.actorOverlays={};app.cameraControls={};app.volumetric={};app.dayNight={};app.nightSky={};app.hbao={};app.dof={};app.viewmodelCubemapMultiplier=app.worldCubemapMultiplier=1.f;while(true){if(in.fail())return false;if(in.eof())break;in>>std::ws;if(in.eof())break;if(!(in>>key))return false;if(key=="crosshair"){if(!readVisualValues(in,crosshairValues(app)))return false;continue;}
if(key=="night_galaxy_noise"){if(!(in>>app.nightSky.galaxyNoiseAlgorithm>>app.nightSky.galaxyNoiseScale>>app.nightSky.galaxyNoiseDetail>>app.nightSky.galaxyNoiseContrast))return false;app.nightSky.sanitize();continue;}
if(key=="night_sky"){if(!(in>>app.nightSky))return false;continue;}
if(key=="water_appearance"){if(!(in>>static_cast<render::water::Appearance&>(app.water)))return false;continue;}
if(key=="water_optics"){if(!(in>>app.water.optics))return false;continue;}
if(key=="water_spectrum"){if(!(in>>app.water.crossSwell>>app.water.swellAngle>>app.water.swellLength>>app.water.seed))return false;app.water.sanitize();continue;}
if(key=="water_surface"){if(!(in>>app.water.surface))return false;continue;}
if(key=="star_rotation"){if(!(in>>app.dayNight.starAutoRotate>>app.dayNight.starRotationSpeed))return false;continue;}
if(key=="day_night"){if(!(in>>app.dayNight))return false;continue;}
if(key=="actor_overlays"){if(!readVisualValues(in,actorOverlayValues(app)))return false;app.actorOverlays.sanitize();continue;}
if(key=="weapon_flash"){if(!readVisualValues(in,weapon_flashValues(app)))return false;continue;}
if(key=="weapon_smoke"){if(!readVisualValues(in,weapon_smokeValues(app)))return false;continue;}
if(key=="weapon_trails"){if(!readVisualValues(in,weapon_trailsValues(app)))return false;continue;}if(key=="weapon_fx_images"){
 std::string flash,smoke,trail,error;if(!(in>>std::quoted(flash)>>std::quoted(smoke)>>std::quoted(trail)))return false;
 app.muzzleFlashPath=flash;app.smokeTrailTexturePath=smoke;app.bulletTrailTexturePath=trail;
 resources.weaponFx=true;
 continue;
}
if(key=="camera_controls"){if(!(in>>app.cameraControls))return false;continue;}if(key=="view_motion"){if(!(in>>app.experimentalUniversalFirstPersonMantle>>app.universalFirstPersonMantleScale>>app.mantleFallbackPositionIw.x>>app.mantleFallbackPositionIw.y>>app.mantleFallbackPositionIw.z>>app.mantleFallbackRotationDegrees.x>>app.mantleFallbackRotationDegrees.y>>app.mantleFallbackRotationDegrees.z>>app.mantleFallbackRecovery>>app.mantleFallbackCurve>>app.boostFallback>>app.slideFallback>>app.boostCamera>>app.boostCameraMovesViewmodel>>app.directionalTilt>>app.sniperZoomIn>>app.sniperZoomOut))return false;continue;}if(key=="reticle_spacing"){if(!(in>>app.csgoCrosshairHorizontalSpacing>>app.csgoCrosshairVerticalSpacing>>app.csgoCrosshairSideMarks>>app.csgoCrosshairMarkLength>>app.csgoCrosshairMarkOffset))return false;continue;}if(key=="sniper_taper_curve"){if(!(in>>app.sniperTrailTaper))return false;continue;}if(key=="bullet_taper_curve"){if(!(in>>app.projectileTrailTaper))return false;continue;}if(key=="volumetric"){if(!(in>>app.volumetric))return false;app.volumetric.sanitize();continue;}if(key=="hbao_before_fog"){in>>app.hbao.beforeFog;continue;}if(key=="cubemap_surface_multipliers"){in>>app.viewmodelCubemapMultiplier>>app.worldCubemapMultiplier;continue;}if(key=="dof"){in>>app.dof;app.dof.sanitize();}else if(key=="ao_transparency"){in>>app.hbao.separateTransparent>>app.hbao.transparentGap;}else if(key=="hbao"){in>>app.hbao;app.hbao.sanitize();}else if(key=="shading")in>>app.shadingModel>>app.iw3DualLobe;else if(key=="brdf_model")in>>app.shadingSubModel;else if(key=="aw")in>>app.awRoughnessScale>>app.awRoughnessBias>>app.awMetalness>>app.awSpecularLevel>>app.awDiffuseWrap>>app.awClearcoat>>app.awClearcoatRoughness>>app.awEnvironmentIntensity;else if(key=="eevee")in>>app.eeveeMetallic>>app.eeveeRoughness>>app.eeveeIor>>app.eeveeSpecular>>app.eeveeSpecularTint>>app.eeveeClearcoat>>app.eeveeClearcoatRoughness;else if(key=="tonemapping")in>>app.tonemappingMode;else if(key=="tonemap_rec709")in>>app.tonemapRec709Match>>app.tonemapRec709Strength;else if(key=="lens")in>>app.lensAlpha>>app.lensTint.x>>app.lensTint.y>>app.lensTint.z>>app.lensSpecularIntensity>>app.lensCubemapIntensity;else if(key=="emissive")in>>app.tritiumTint.x>>app.tritiumTint.y>>app.tritiumTint.z;else if(key=="spec")in>>app.specularIntensity>>app.specularSharpness>>app.specularIntensity2>>app.specularSharpness2;else if(key=="cube"){in>>app.cubemapSpecular>>app.cubemapSpecularIntensity>>app.cubemapSpecularBlur;std::string restLine;std::getline(in,restLine);std::istringstream iss(restLine);float wb=0.08f;if(iss>>wb)app.weaponCubemapBlur=wb;}else if(key=="shadow_specular")in>>app.shadowSpecularMultiplier;else if(key=="film")in>>app.filmTweaks>>app.filmBrightness>>app.filmContrast>>app.filmDesaturation>>app.filmInvert>>app.filmDarkTint.x>>app.filmDarkTint.y>>app.filmDarkTint.z>>app.filmLightTint.x>>app.filmLightTint.y>>app.filmLightTint.z;else if(key=="film_mid")in>>app.filmMidTintEnabled>>app.filmMidTint.x>>app.filmMidTint.y>>app.filmMidTint.z;else if(key=="fog")in>>app.fogEnabled>>app.fogColor.x>>app.fogColor.y>>app.fogColor.z>>app.fogStartMeters>>app.fogHalfDistanceMeters>>app.fogOpacity>>app.fogSkyAmount;else if(key=="sun"){in>>app.sunLighting>>app.sunShadows>>app.sunAzimuth>>app.sunElevation>>app.sunIntensity>>app.sunAmbient>>app.sunColor.x>>app.sunColor.y>>app.sunColor.z>>app.ambientColor.x>>app.ambientColor.y>>app.ambientColor.z;app.sunBlackbody=app.ambientBlackbody=false;}else if(key=="environment")in>>app.environmentIntensity>>app.environmentExposure>>app.environmentRotation>>app.skyVerticalFlip;else if(key=="env_hdr")in>>app.environmentContrast>>app.environmentHighlightThreshold>>app.environmentHighlightBoost>>app.environmentToneMap;else if(key=="post_bloom")in>>app.bloomEnabled>>app.bloomThreshold>>app.bloomSoftThreshold>>app.bloomSaturationBias>>app.bloomIntensity>>app.bloomRadius>>app.bloomKawaseSamples>>app.bloomAffectsSky>>app.bloomAspectRatio>>app.bloomRotation;else if(key=="bloom_blend")in>>app.bloomBlendMode;else if(key=="post_vignette")in>>app.vignetteEnabled>>app.vignetteIntensity>>app.vignetteRadius>>app.vignetteSoftness;else if(key=="post_distortion")in>>app.lensDistortion;else if(key=="post_levels")in>>app.autoBlackPoint>>app.autoBlackPointIntensity>>app.autoWhitePoint>>app.autoWhitePointIntensity;else if(key=="post_lut"){std::string lutFile;in>>app.lutIntensity>>std::quoted(lutFile);app.lutPath=lutFile;resources.lut=true;}else if(key=="post_order"){for(int& idx:app.postPassOrder)in>>idx;}else if(key=="impact_surf"){std::string pathStr;in>>app.impactSurfaceEnabled>>app.impactSurfacePlanesCount>>app.impactSurfaceStartWidth>>app.impactSurfaceEndWidth>>app.impactSurfaceLifetime>>app.impactSurfaceBlastSpeed>>app.impactSurfaceRiseSpeed>>app.impactSurfaceDispersion>>app.impactSurfaceTaperingExp>>app.impactSurfaceFeathering>>app.impactSurfaceColor.x>>app.impactSurfaceColor.y>>app.impactSurfaceColor.z>>app.impactSurfaceColor.w>>app.impactSurfaceTexScale>>app.impactSurfaceTexOffsetX>>app.impactSurfaceTexOffsetY>>app.impactSurfaceTexRotation>>app.impactSurfaceOriginY>>std::quoted(pathStr);app.impactSurfacePath=pathStr;resources.impactSurface=true;}else if(key=="impact_bot"){std::string pathStr;in>>app.impactBotEnabled>>app.impactBotPlanesCount>>app.impactBotStartWidth>>app.impactBotEndWidth>>app.impactBotLifetime>>app.impactBotBlastSpeed>>app.impactBotRiseSpeed>>app.impactBotDispersion>>app.impactBotTaperingExp>>app.impactBotFeathering>>app.impactBotColor.x>>app.impactBotColor.y>>app.impactBotColor.z>>app.impactBotColor.w>>app.impactBotTexScale>>app.impactBotTexOffsetX>>app.impactBotTexOffsetY>>app.impactBotTexRotation>>app.impactBotOriginY>>std::quoted(pathStr);app.impactBotPath=pathStr;resources.impactBot=true;}else if(key=="sky"){std::string value;in>>std::quoted(value);app.environmentPath=value;resources.sky=true;}else {std::string rest;std::getline(in,rest);}}std::array<bool,7> seen{};for(int value:app.postPassOrder){if(value<0||value>=7||seen[value])return false;seen[value]=true;}return !in.bad();}

bool loadVisualPreset(AppState& app,const std::filesystem::path& path){
    std::ifstream in(path);if(!in)return false;
    auto staged=std::make_unique<AppState>();copyVisualPresetSettings(*staged,app);
    VisualPresetResources resources;
    if(!parseVisualPreset(*staged,in,resources))return false;
    // Environment is the only resource whose failure rejects the old loader.
    // Its renderer setters preserve the existing texture on failure. Load it
    // first, before publishing settings or changing any other textures.
    if(resources.sky){
        const auto& sky=staged->environmentPath;
        if(sky.empty())app.renderer.clearEnvironment();
        else {std::string error;const bool ok=lowerText(sky.extension().string())==".iwi"?
            app.renderer.setT6SkyboxIwi(sky,error):app.renderer.setEnvironmentPanorama(sky,error);
            if(!ok)return false;}
    }
    copyVisualPresetSettings(app,*staged);
    std::string error;
    if(resources.lut){if(app.lutPath.empty())app.renderer.clearLutTexture();else if(!app.renderer.setLutTexture(app.lutPath,error))app.status=error;}
    if(resources.impactSurface){if(app.impactSurfacePath.empty())app.renderer.clearImpactSurfaceTexture();else if(!app.renderer.setImpactSurfaceTexture(app.impactSurfacePath,error))app.status=error;}
    if(resources.impactBot){if(app.impactBotPath.empty())app.renderer.clearImpactBotTexture();else if(!app.renderer.setImpactBotTexture(app.impactBotPath,error))app.status=error;}
    if(resources.weaponFx){
        if(app.muzzleFlashPath.empty())app.renderer.clearMuzzleFlashTexture();else if(!app.renderer.setMuzzleFlashTexture(app.muzzleFlashPath,error))app.status=error;
        if(app.smokeTrailTexturePath.empty())app.renderer.clearSmokeTexture();else if(!app.renderer.setSmokeTexture(app.smokeTrailTexturePath,error))app.status=error;
        if(app.bulletTrailTexturePath.empty())app.renderer.clearBulletTrailTexture();else if(!app.renderer.setBulletTrailTexture(app.bulletTrailTexturePath,error))app.status=error;
    }
    app.renderer.setEnvironmentParameters(app.environmentIntensity,app.environmentExposure,app.environmentRotation,app.environmentContrast,app.environmentHighlightBoost,app.environmentHighlightThreshold,app.environmentToneMap);app.renderer.setSkyVerticalFlip(app.skyVerticalFlip);app.renderer.setFilmTweaks(app.filmTweaks,app.filmBrightness,app.filmContrast,app.filmDesaturation,app.filmDarkTint,app.filmMidTint,app.filmLightTint,app.filmMidTintEnabled,app.filmInvert);app.renderer.setLensTweaks(app.bloomEnabled,app.bloomThreshold,app.bloomSoftThreshold,app.bloomSaturationBias,app.bloomAspectRatio,app.bloomRotation,app.bloomIntensity,app.vignetteEnabled,app.vignetteIntensity,app.vignetteRadius,app.vignetteSoftness);app.renderer.setPostEffects(app.bloomRadius,app.lensDistortion,app.lutIntensity,app.autoBlackPoint,app.autoBlackPointIntensity,app.autoWhitePoint,app.autoWhitePointIntensity);app.renderer.setEeveeParameters(app.eeveeMetallic,app.eeveeRoughness,app.eeveeIor,app.eeveeSpecular,app.eeveeSpecularTint,app.eeveeClearcoat,app.eeveeClearcoatRoughness);app.renderer.setTonemapping(app.tonemappingMode,app.tonemapRec709Match,app.tonemapRec709Strength);app.renderer.setBrdfModel(app.shadingSubModel);app.renderer.setBloomBlendMode(app.bloomBlendMode);app.renderer.setPostPassOrder(app.postPassOrder);app.renderer.setShadowSpecularMultiplier(app.shadowSpecularMultiplier);
    return true;
}
