#pragma once
#include "gameplay/ProceduralViewMotion.h"
#include "gameplay/ResponseCurve.h"
#include "gameplay/ShoulderCamera.h"
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace gameplay::view {
inline ResponseCurve zoomInDefaults(){auto c=ResponseCurve::linear(0,1);c.count=3;c.points={{{0,0},{.718f,0},{1,1}}};return c;}
enum class MotionSection {Mantle,Boost,Slide,Camera,Tilt,ZoomIn,ZoomOut,Shoulder};
struct MotionDefaults {
    bool authoredMantle{true},authoredBoost{true},linkCamera{true};
    MotionProfile mantle{},boost{boostDefaults()},slide{slideDefaults()},camera{boostCameraDefaults()};
    DirectionalTilt tilt{};
    ResponseCurve zoomIn{zoomInDefaults()},zoomOut{ResponseCurve::linear(0,1)};
    CameraControls controls{};
};
inline std::ostream& operator<<(std::ostream& out,const MotionDefaults& s){return out<<s.authoredMantle<<' '<<s.authoredBoost<<' '<<s.linkCamera<<'\n'<<s.mantle<<'\n'<<s.boost<<'\n'<<s.slide<<'\n'<<s.camera<<'\n'<<s.tilt<<'\n'<<s.zoomIn<<'\n'<<s.zoomOut<<'\n';}
inline std::istream& operator>>(std::istream& in,MotionDefaults& s){MotionDefaults value;if(in>>value.authoredMantle>>value.authoredBoost>>value.linkCamera>>value.mantle>>value.boost>>value.slide>>value.camera>>value.tilt>>value.zoomIn>>value.zoomOut){for(auto* c:{&value.zoomIn,&value.zoomOut}){c->points[0].y=0;c->points[c->count-1].y=1;}s=value;}return in;}
inline bool loadMotionDefaults(const std::filesystem::path& path,MotionDefaults& value){std::ifstream in(path);std::string magic;int version{};MotionDefaults loaded;if(!(in>>magic>>version)||magic!="CADENCEMOTION"||version<1||version>2||!(in>>loaded)||(version>=2&&!(in>>loaded.controls)))return false;value=loaded;return true;}
inline bool saveMotionDefaults(const std::filesystem::path& path,const MotionDefaults& current,MotionSection section){
    MotionDefaults saved;loadMotionDefaults(path,saved);
    switch(section){case MotionSection::Mantle:saved.mantle=current.mantle;saved.authoredMantle=current.authoredMantle;break;case MotionSection::Boost:saved.boost=current.boost;saved.authoredBoost=current.authoredBoost;break;case MotionSection::Slide:saved.slide=current.slide;break;case MotionSection::Camera:saved.camera=current.camera;saved.linkCamera=current.linkCamera;break;case MotionSection::Tilt:saved.tilt=current.tilt;break;case MotionSection::ZoomIn:saved.zoomIn=current.zoomIn;saved.controls.zoomInDuration=current.controls.zoomInDuration;saved.controls.zoomIntensity=current.controls.zoomIntensity;break;case MotionSection::ZoomOut:saved.zoomOut=current.zoomOut;saved.controls.zoomOutDuration=current.controls.zoomOutDuration;saved.controls.zoomIntensity=current.controls.zoomIntensity;break;case MotionSection::Shoulder:saved.controls.shoulder=current.controls.shoulder;break;}
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return false;
    const auto temp=std::filesystem::path(path.string()+".tmp");{std::ofstream out(temp);if(!(out<<std::setprecision(9)<<"CADENCEMOTION 2\n"<<saved<<saved.controls<<'\n'))return false;out.close();if(!out)return false;}
    // Keep the existing defaults intact if writing fails; replacement occurs only after a complete write.
    std::filesystem::copy_file(temp,path,std::filesystem::copy_options::overwrite_existing,error);std::error_code cleanup;std::filesystem::remove(temp,cleanup);return !error;
}
}
