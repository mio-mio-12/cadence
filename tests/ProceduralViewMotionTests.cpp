#include "gameplay/ProceduralViewMotion.h"
#include "gameplay/ZoomResponse.h"
#include "gameplay/MotionDefaults.h"
#include <sstream>
#include <iostream>
#include <chrono>
int main(){int failures=0;auto check=[&](bool b){if(!b)++failures;};
    gameplay::view::MotionProfile p;check(p.intensity==.19f&&p.duration==1.05f&&p.curve.count==5);
    const auto error=[](const scene::Mat4&a,const scene::Mat4&b){float e=0;for(int i=0;i<16;++i)e=std::max(e,std::abs(a.v[i]-b.v[i]));return e;};
    const auto identity=scene::Mat4::identity();check(error(gameplay::view::motionTransform(p,0,2.54f),identity)==0);check(error(gameplay::view::motionTransform(p,2,2.54f),identity)==0);
    check(p.curve.sample(.286f)<0);const auto motion=gameplay::view::motionTransform(p,.126f,2.54f);check(error(motion,identity)>.1f);
    p.position={-2,2,-3};p.rotation={8,-6,0};const auto left=gameplay::view::motionTransform(p,.126f,2.54f,{0,1},true),right=gameplay::view::motionTransform(p,.126f,2.54f,{0,-1},true);check(left.v[13]>0&&right.v[13]<0);check(std::abs(left.v[14]-right.v[14])<.001f);
    // Camera-relative presentation must be invariant under source bind axes.
    auto camera=scene::trs({10,20,30},scene::fromEulerRadians({.2f,.4f,.8f}),{1,1,1});
    for(auto angle:{0.f,1.f,2.f}){auto native=scene::trs({4,8,12},scene::fromEulerRadians({angle,.3f,0}),{1,1,1});auto anchor=camera*scene::inverseAffine(native);auto posed=camera*motion*scene::inverseAffine(camera)*anchor*native;check(error(posed,camera*motion)<.0001f);}
    std::stringstream ss;ss<<p;gameplay::view::MotionProfile q;ss>>q;check(bool(ss)&&q.curve.count==p.curve.count&&q.position.y==p.position.y&&q.duration==p.duration);
    auto boost=gameplay::view::boostDefaults(),slide=gameplay::view::slideDefaults();
    check(boost.position.x==2.7f&&boost.rotation.y==21.7f&&slide.duration==3&&slide.intensity==.39f);
    std::stringstream saved;saved<<slide;saved>>q;check(q.duration==3&&q.rotation.x==90);
    gameplay::view::DirectionalTilt tilt;tilt.enabled=true;
    const auto l=gameplay::view::tiltTarget(tilt,{0,300,0},0,0),r=gameplay::view::tiltTarget(tilt,{0,-300,0},0,0);
    check(l.x==-r.x&&l.x!=0);
    auto one=gameplay::view::advanceTilt({},l,.1f,tilt),two=gameplay::view::advanceTilt(gameplay::view::advanceTilt({},l,.05f,tilt),l,.05f,tilt);
    check(scene::length(one-two)<.00001f);
    auto faded=one;for(int i=0;i<200;++i)faded=gameplay::view::advanceTilt(faded,{},.01f,tilt);check(scene::length(faded)<.001f);
    gameplay::view::ZoomResponseState zoom;auto curve=gameplay::view::ResponseCurve::linear(0,1);
    zoom.update(true,.5f,1,curve,curve);check(std::abs(zoom.value-.5f)<.00001f);
    zoom.update(false,0,1,curve,curve);check(std::abs(zoom.value-.5f)<.00001f);
    zoom.update(false,.5f,1,curve,curve);check(std::abs(zoom.value-.25f)<.00001f);
    zoom.update(false,.5f,1,curve,curve);check(zoom.value==0);
    check(gameplay::view::zoomDuration(0,.25f)==.25f&&gameplay::view::zoomDuration(.8f,.25f)==.8f);
    zoom={};zoom.update(true,.2f,gameplay::view::zoomDuration(.8f,.25f),curve,curve);check(std::abs(zoom.value-.25f)<.00001f);
    check(gameplay::view::zoomFov(90,30,zoom.value,0)==90&&gameplay::view::zoomFov(90,30,1,1)==30&&gameplay::view::zoomFov(90,30,1,2)==1);
    zoom.update(false,0,.4f,curve,curve);check(std::abs(zoom.value-.25f)<.00001f);
    zoom.update(false,.4f,.4f,curve,curve);check(zoom.value==0);
    gameplay::view::ShoulderCameraSettings shoulder;
    check(scene::length(gameplay::view::shoulderBoom(shoulder,0,0,0)-scene::Vec3{-228.6f,-45.72f,12.7f})<.001f);
    // Any orbit/roll retains an orthonormal view centered on the gameplay aim target.
    for(float yaw:{0.f,1.f,2.f})for(float orbit:{-50.f,0.f,50.f}){
        shoulder.rotation={20,orbit,35};const scene::Vec3 pivot{10,20,160},aim{1000,400,180};
        const auto eye=pivot+gameplay::view::shoulderBoom(shoulder,yaw,.2f,.4f),forward=scene::normalize(aim-eye),up=gameplay::view::shoulderUp(forward,shoulder.rotation.z);
        check(std::abs(scene::dot(forward,up))<.00001f&&std::abs(scene::length(up)-1)<.00001f);
        const auto matrix=scene::lookAtDirection(eye,forward,up);const auto local=scene::transformPoint(matrix,aim);
        check(std::abs(local.x)<.001f&&std::abs(local.y)<.001f);
    }
    auto taper=gameplay::view::ResponseCurve::linear(.2f,1.f);check(std::abs(taper.sample(.5f)-.6f)<.00001f&&taper.sample(0)==.2f&&taper.sample(1)==1);
    taper.count=3;taper.points={{{0,.2f},{.4f,2.f},{1,.1f}}};taper.sanitize();check(taper.sample(.4f)==2.f);
    std::stringstream curveText;curveText<<taper;gameplay::view::ResponseCurve taperRead;curveText>>taperRead;check(bool(curveText)&&taperRead.count==3&&taperRead.sample(1)==.1f);
    std::stringstream invalidCurve("1 17");invalidCurve>>taperRead;check(!invalidCurve&&taperRead.count==3);
    gameplay::view::MotionDefaults defaults;
    check(defaults.mantle.position.z==-10.8f&&defaults.mantle.rotation.y==-19.3f&&defaults.camera.intensity==.70f&&defaults.camera.duration==.74f);
    check(defaults.tilt.enabled&&!defaults.tilt.viewmodel&&defaults.tilt.intensity==.07f&&defaults.zoomIn.sample(.7f)==0&&defaults.zoomIn.sample(1)==1);
    const auto folder=std::filesystem::temp_directory_path()/("cadence_motion_defaults_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path=folder/"motion.defaults";
    defaults.mantle.intensity=.51f;defaults.camera.intensity=.92f;
    check(gameplay::view::saveMotionDefaults(path,defaults,gameplay::view::MotionSection::Mantle));
    gameplay::view::MotionDefaults restarted;check(gameplay::view::loadMotionDefaults(path,restarted));
    check(restarted.mantle.intensity==.51f&&restarted.camera.intensity==.70f);
    check(gameplay::view::saveMotionDefaults(path,defaults,gameplay::view::MotionSection::Camera));
    check(gameplay::view::loadMotionDefaults(path,restarted)&&restarted.mantle.intensity==.51f&&restarted.camera.intensity==.92f);
    std::stringstream preset;preset<<defaults;gameplay::view::MotionDefaults loadedPreset;preset>>loadedPreset;
    defaults.controls.shoulder.position.y=-25;defaults.controls.shoulder.rotation.z=17;defaults.controls.zoomInDuration=.8f;defaults.controls.zoomOutDuration=.45f;defaults.controls.zoomIntensity=.6f;
    check(gameplay::view::saveMotionDefaults(path,defaults,gameplay::view::MotionSection::Shoulder));
    check(gameplay::view::saveMotionDefaults(path,defaults,gameplay::view::MotionSection::ZoomIn));
    check(gameplay::view::saveMotionDefaults(path,defaults,gameplay::view::MotionSection::ZoomOut));
    check(gameplay::view::loadMotionDefaults(path,restarted)&&restarted.controls.shoulder.position.y==-25&&restarted.controls.shoulder.rotation.z==17&&restarted.controls.zoomInDuration==.8f&&restarted.controls.zoomOutDuration==.45f&&restarted.controls.zoomIntensity==.6f);
    // Legacy v1 remains readable with compatibility defaults for new controls.
    {std::ofstream legacy(path);legacy<<"CADENCEMOTION 1\n"<<defaults;}
    check(gameplay::view::loadMotionDefaults(path,restarted)&&restarted.controls.zoomIntensity==0&&restarted.controls.zoomInDuration==0&&restarted.controls.shoulder.fov==75);
    std::stringstream controls;controls<<defaults.controls;gameplay::view::CameraControls roundtrip;controls>>roundtrip;
    check(bool(controls)&&roundtrip.zoomOutDuration==.45f&&roundtrip.shoulder.position.y==-25);
    check(bool(preset)&&loadedPreset.mantle.intensity==.51f&&loadedPreset.camera.intensity==.92f&&loadedPreset.zoomIn.count==3);
    {std::ofstream damaged(path);damaged<<"CADENCEMOTION 1 invalid";}
    check(!gameplay::view::loadMotionDefaults(path,restarted)&&restarted.camera.intensity==.92f);
    std::error_code cleanup;std::filesystem::remove_all(folder,cleanup);
    std::cout<<"Procedural view motion failures: "<<failures<<'\n';return failures?1:0;
}
