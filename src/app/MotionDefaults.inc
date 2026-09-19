gameplay::view::MotionDefaults captureMotionDefaults(const AppState& app){
    gameplay::view::MotionDefaults value;
    value.authoredMantle=app.experimentalAuthoredViewmodelMantle;value.authoredBoost=app.experimentalExoViewmodelJump;
    value.mantle={app.experimentalUniversalFirstPersonMantle,app.universalFirstPersonMantleScale,app.mantleFallbackRecovery,app.mantleFallbackPositionIw,app.mantleFallbackRotationDegrees,app.mantleFallbackCurve};
    value.boost=app.boostFallback;value.slide=app.slideFallback;value.camera=app.boostCamera;value.linkCamera=app.boostCameraMovesViewmodel;value.tilt=app.directionalTilt;value.zoomIn=app.sniperZoomIn;value.zoomOut=app.sniperZoomOut;value.controls=app.cameraControls;return value;
}
void applyMotionDefaults(AppState& app,const gameplay::view::MotionDefaults& value){
    app.experimentalAuthoredViewmodelMantle=value.authoredMantle;app.experimentalExoViewmodelJump=value.authoredBoost;
    app.experimentalUniversalFirstPersonMantle=value.mantle.enabled;app.universalFirstPersonMantleScale=value.mantle.intensity;app.mantleFallbackRecovery=value.mantle.duration;app.mantleFallbackPositionIw=value.mantle.position;app.mantleFallbackRotationDegrees=value.mantle.rotation;app.mantleFallbackCurve=value.mantle.curve;
    app.boostFallback=value.boost;app.slideFallback=value.slide;app.boostCamera=value.camera;app.boostCameraMovesViewmodel=value.linkCamera;app.directionalTilt=value.tilt;app.sniperZoomIn=value.zoomIn;app.sniperZoomOut=value.zoomOut;app.cameraControls=value.controls;
}
std::filesystem::path motionDefaultsPath(){return cadenceAssetsDirectory()/"settings"/"motion.defaults";}
void loadUserMotionDefaults(AppState& app){gameplay::view::MotionDefaults value;if(gameplay::view::loadMotionDefaults(motionDefaultsPath(),value))applyMotionDefaults(app,value);}
void saveUserMotionDefaults(AppState& app,gameplay::view::MotionSection section){app.status=gameplay::view::saveMotionDefaults(motionDefaultsPath(),captureMotionDefaults(app),section)?"Saved motion section defaults for future launches":"Could not save motion defaults";}
