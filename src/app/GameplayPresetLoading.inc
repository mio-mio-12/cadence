// Existing versioned format, staged before publishing any gameplay settings.
void copyGameplayPresetSettings(AppState& to,const AppState& from){
    applyMotionDefaults(to,captureMotionDefaults(from));
    to.actionBlendTime=from.actionBlendTime;
    to.actorJumpHeightIw=from.actorJumpHeightIw;
    to.actorMantleFacingConeDegrees=from.actorMantleFacingConeDegrees;
    to.actorMantleFalloff=from.actorMantleFalloff;
    to.actorMantleInputBuffer=from.actorMantleInputBuffer;
    to.actorMantleReachIw=from.actorMantleReachIw;
    to.actorMantleSpeedMultiplier=from.actorMantleSpeedMultiplier;
    to.actorMaxMantleHeightIw=from.actorMaxMantleHeightIw;
    to.actorMinMantleHeightIw=from.actorMinMantleHeightIw;
    to.autoStrafeKey=from.autoStrafeKey;
    to.bhopGroundClipThresholdIw=from.bhopGroundClipThresholdIw;
    to.boostCamera=from.boostCamera;
    to.boostCameraMovesViewmodel=from.boostCameraMovesViewmodel;
    to.boostFallback=from.boostFallback;
    to.cameraControls=from.cameraControls;
    to.directionalTilt=from.directionalTilt;
    to.experimentalExoViewmodelJump=from.experimentalExoViewmodelJump;
    to.experimentalUniversalFirstPersonMantle=from.experimentalUniversalFirstPersonMantle;
    to.mantleFallbackCurve=from.mantleFallbackCurve;
    to.mantleFallbackPositionIw=from.mantleFallbackPositionIw;
    to.mantleFallbackRecovery=from.mantleFallbackRecovery;
    to.mantleFallbackRotationDegrees=from.mantleFallbackRotationDegrees;
    to.movementAlgorithm=from.movementAlgorithm;
    to.slideFallback=from.slideFallback;
    to.sniperZoomIn=from.sniperZoomIn;
    to.sniperZoomOut=from.sniperZoomOut;
    to.sourceAirAccelerate=from.sourceAirAccelerate;
    to.sourceAutoJump=from.sourceAutoJump;
    to.sourceAutoStrafe=from.sourceAutoStrafe;
    to.sourceUnlimitedBunnyhop=from.sourceUnlimitedBunnyhop;
    to.transitionDuration=from.transitionDuration;
    to.universalFirstPersonMantleScale=from.universalFirstPersonMantleScale;
    to.viewBobScale=from.viewBobScale;
    to.weaponSwitchAlgorithm=from.weaponSwitchAlgorithm;
}
bool parseGameplayPreset(AppState& app,std::istream& in){std::string signature;int version{};if(!(in>>signature>>version)||signature!="CADENCEGAME"||version<1||version>9)return false;if(!(in>>app.transitionDuration>>app.actionBlendTime>>app.weaponSwitchAlgorithm>>app.movementAlgorithm>>app.actorJumpHeightIw>>app.viewBobScale>>app.bhopGroundClipThresholdIw))return false;app.sourceAutoStrafe=false;applyMotionDefaults(app,gameplay::view::MotionDefaults{});app.boostFallback=gameplay::view::boostDefaults();app.slideFallback=gameplay::view::slideDefaults();app.boostCamera=gameplay::view::boostCameraDefaults();app.boostCameraMovesViewmodel=true;app.directionalTilt={};app.sniperZoomIn=gameplay::view::zoomInDefaults();app.sniperZoomOut=gameplay::view::ResponseCurve::linear(0,1);app.experimentalExoViewmodelJump=true;app.actorMantleSpeedMultiplier=1.0f;app.actorMantleFalloff=0.18f;if(version==1){app.sourceAutoJump=true;return true;}if(!(in>>app.sourceAutoJump>>app.sourceUnlimitedBunnyhop>>app.actorMaxMantleHeightIw>>app.actorMantleInputBuffer>>app.actorMantleReachIw>>app.actorMantleFacingConeDegrees>>app.experimentalUniversalFirstPersonMantle>>app.universalFirstPersonMantleScale))return false;if(version==2)return true;if(!(in>>app.sourceAutoStrafe>>app.autoStrafeKey>>app.mantleFallbackPositionIw.x>>app.mantleFallbackPositionIw.y>>app.mantleFallbackPositionIw.z>>app.mantleFallbackRotationDegrees.x>>app.mantleFallbackRotationDegrees.y>>app.mantleFallbackRotationDegrees.z>>app.mantleFallbackRecovery))return false;app.sourceAutoStrafe=false;app.mantleFallbackRecovery=std::clamp(app.mantleFallbackRecovery,0.05f,2.0f);if(version==3)return true;if(!(in>>app.sourceAirAccelerate>>app.actorMinMantleHeightIw))return false;app.sourceAirAccelerate=std::clamp(app.sourceAirAccelerate,1.0f,200.0f);app.actorMinMantleHeightIw=std::clamp(app.actorMinMantleHeightIw,18.0f,app.actorMaxMantleHeightIw);float mantleRate{},mantleFalloff{};in>>std::ws;if(!in.eof()){if(!(in>>mantleRate>>mantleFalloff)||!std::isfinite(mantleRate)||!std::isfinite(mantleFalloff))return false;app.actorMantleSpeedMultiplier=std::clamp(mantleRate,0.25f,3.0f);app.actorMantleFalloff=std::clamp(mantleFalloff,0.01f,1.0f);}if(version>=5){if(!(in>>app.mantleFallbackCurve))return false;}if(version>=6&&!(in>>app.experimentalExoViewmodelJump>>app.boostFallback>>app.slideFallback))return false;if(version>=7&&!(in>>app.boostCamera>>app.boostCameraMovesViewmodel>>app.directionalTilt>>app.sniperZoomIn>>app.sniperZoomOut))return false;if(version>=8){gameplay::view::MotionDefaults value;if(!(in>>value))return false;applyMotionDefaults(app,value);}if(version>=9&&!(in>>app.cameraControls))return false;return true;}
bool loadGameplayPreset(AppState& app,const std::filesystem::path& path){
    std::ifstream in(path);if(!in)return false;
    auto staged=std::make_unique<AppState>();copyGameplayPresetSettings(*staged,app);
    if(!parseGameplayPreset(*staged,in)||!std::isfinite(staged->actionBlendTime)||staged->actionBlendTime<0||!std::isfinite(staged->transitionDuration)||staged->transitionDuration<0)return false;
    copyGameplayPresetSettings(app,*staged);return true;
}
