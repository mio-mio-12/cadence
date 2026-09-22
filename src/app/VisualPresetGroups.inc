// Numeric groups shared by save, staged load, and copy. Runtime counters and GPU handles are deliberately excluded.
template<class App> auto hitmarker_shapeValues(App& app){return std::tie(app.hitmarkerEnabled,app.hitmarkerProcedural,app.hitmarkerDot,app.hitmarkerArms,app.hitmarkerGap,app.hitmarkerLength,app.hitmarkerThickness,app.hitmarkerRotation,app.hitmarkerOutline,app.hitmarkerDotSize,app.hitmarkerOpacity,app.hitmarkerFadePower,app.hitmarkerPulse,app.hitmarkerDurationScale,app.hitmarkerKillScale,app.hitmarkerSpriteSize);}
template<class App> auto hitmarker_colorsValues(App& app){return std::tie(app.hitmarkerColor.x,app.hitmarkerColor.y,app.hitmarkerColor.z,app.hitmarkerColor.w,app.hitmarkerHeadColor.x,app.hitmarkerHeadColor.y,app.hitmarkerHeadColor.z,app.hitmarkerHeadColor.w,app.hitmarkerKillColor.x,app.hitmarkerKillColor.y,app.hitmarkerKillColor.z,app.hitmarkerKillColor.w,app.hitmarkerOutlineColor.x,app.hitmarkerOutlineColor.y,app.hitmarkerOutlineColor.z,app.hitmarkerOutlineColor.w);}
template<class App> auto camera_visualValues(App& app){return std::tie(app.actorThirdPerson,app.viewmodelCamera,app.fovSystem,app.viewmodelFov,app.viewmodelFovScale,app.cameraNearClipCm,app.cameraFarClipMeters,app.scopeOverlayScale,app.experimentalAuthoredViewmodelMantle,app.experimentalExoViewmodelJump);}
template<class App> auto camo_visualValues(App& app){return std::tie(app.camoStrength,app.camoScale,app.camoOffsetX,app.camoOffsetY,app.camoRotation,app.camoAlphaLow,app.camoAlphaHigh,app.camoInvert,app.camoLumaMask,app.camoLumaNativeT6,app.camoLumaLow,app.camoLumaHigh,app.camoLumaGamma,app.camoLumaContrast,app.ignoreViewmodelTextureAlpha,app.alphaOverlay,app.alphaOverlayIntensity);}
template<class App> auto material_visualValues(App& app){return std::tie(app.normalMapIntensity,app.mapDecalDepthBias,app.alternateSpecularEnabled,app.alternateSpecularIntensity,app.alternateSpecularIntensity2,app.alternateSpecularSharpness,app.alternateSpecularSharpness2,app.viewmodelSpecularProfile,app.playermodelSpecularProfile,app.worldSpecularProfile,app.specularImperfectionsHigh,app.specularImperfectionsLow,app.specularImperfectionsScale,app.specularImperfectionsStrength,app.surfaceNormalReflectionInfluence,app.cubemapKawaseSamples,app.antialiasing);}
template<class App> auto lens_visualValues(App& app){return std::tie(app.lensTintIntensity,app.worldLensAlpha,app.worldLensCubemapIntensity,app.worldLensSpecularIntensity,app.worldLensTint.x,app.worldLensTint.y,app.worldLensTint.z,app.worldLensTintIntensity);}
template<class App> auto lighting_visualValues(App& app){return std::tie(app.ambientBlackbody,app.ambientKelvin,app.sunBlackbody,app.sunKelvin,app.fogHeightEnabled,app.fogHeightFalloffMeters,app.fogHeightMeters,app.shadowBias,app.shadowContrast,app.shadowDistanceMeters,app.shadowFadeStartMeters,app.shadowIntensity,app.shadowNormalBias,app.shadowSoftness,app.shadowResolution,app.viewmodelSelfShadows,app.viewmodelShadowResolution,app.farShadowEnabled,app.farShadowResolution,app.farShadowBlendMeters,app.farShadowDistanceMeters,app.farShadowStartMeters,app.shadowTint.x,app.shadowTint.y,app.shadowTint.z);}
template<class App> auto campath_visualValues(App& app){return std::tie(app.campathAutoThemeSync,app.campathDashLength,app.campathDashedLine,app.campathSplineThickness,app.campathFrustumColor.x,app.campathFrustumColor.y,app.campathFrustumColor.z,app.campathNodeColor.x,app.campathNodeColor.y,app.campathNodeColor.z,app.campathSplineColor.x,app.campathSplineColor.y,app.campathSplineColor.z);}
template<class App> auto water_placementValues(App& app){return std::tie(app.water.enabled,app.water.animate,app.water.height,app.water.radius,app.water.timeOffset,app.water.quality);}
template<class App> auto map_alphaValues(App& app){return std::tie(app.ignoreMapTextureAlpha);}
template<class App> auto sky_calibrationValues(App& app){return std::tie(app.skyFaceSources[0],app.skyFaceQuarterTurns[0],app.skyFaceSources[1],app.skyFaceQuarterTurns[1],app.skyFaceSources[2],app.skyFaceQuarterTurns[2],app.skyFaceSources[3],app.skyFaceQuarterTurns[3],app.skyFaceSources[4],app.skyFaceQuarterTurns[4],app.skyFaceSources[5],app.skyFaceQuarterTurns[5]);}
template<class App,class F> void forEachExtendedVisualGroup(App& app,F&& visit){
    visit("map_alpha",map_alphaValues(app));
    visit("hitmarker_shape",hitmarker_shapeValues(app));
    visit("hitmarker_colors",hitmarker_colorsValues(app));
    visit("camera_visual",camera_visualValues(app));
    visit("camo_visual",camo_visualValues(app));
    visit("material_visual",material_visualValues(app));
    visit("lens_visual",lens_visualValues(app));
    visit("lighting_visual",lighting_visualValues(app));
    visit("campath_visual",campath_visualValues(app));
    visit("water_placement",water_placementValues(app));
    visit("sky_calibration",sky_calibrationValues(app));
}
