#pragma once
#include "scene/Math3D.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <cmath>

namespace cadence::capture {
struct CameraFrame {
    scene::Vec3 position{}, forward{1,0,0}, up{0,0,1};
    float verticalFov{80}, nearClip{2.5f}, farClip{100000};
    int width{},height{};
    double sourceTime{};
};
// Explicit basis vectors avoid Euler order ambiguity and preserve procedural roll.
class CameraCsv {
    std::ofstream out_;
public:
    bool open(const std::filesystem::path& path) {
        close(); if(std::filesystem::exists(path))return false;
        out_.open(path); out_.imbue(std::locale::classic()); out_<<std::setprecision(10);
        out_<<"# cadence-camera/1; right-handed Z-up; positions/clips centimetres; forward/up world-space; square pixels; FOV degrees\n"
              "frame,output_seconds,source_seconds,width,height,vertical_fov,horizontal_fov,position_x,position_y,position_z,forward_x,forward_y,forward_z,up_x,up_y,up_z,near_cm,far_cm\n";
        return bool(out_);
    }
    bool write(std::uint64_t frame,double seconds,const CameraFrame& c) {
        if(c.width<=0||c.height<=0)return false;
        const float values[]={c.position.x,c.position.y,c.position.z,c.forward.x,c.forward.y,c.forward.z,c.up.x,c.up.y,c.up.z,c.verticalFov,c.nearClip,c.farClip};
        for(float v:values)if(!std::isfinite(v))return false;
        if(!std::isfinite(seconds)||!std::isfinite(c.sourceTime))return false;
        const double hfov=2*std::atan(std::tan(c.verticalFov*scene::kPi/360.0)*c.width/c.height)*180/scene::kPi;
        out_<<frame<<','<<seconds<<','<<c.sourceTime<<','<<c.width<<','<<c.height<<','<<c.verticalFov<<','<<hfov<<','
            <<c.position.x<<','<<c.position.y<<','<<c.position.z<<','<<c.forward.x<<','<<c.forward.y<<','<<c.forward.z<<','
            <<c.up.x<<','<<c.up.y<<','<<c.up.z<<','<<c.nearClip<<','<<c.farClip<<'\n';
        return bool(out_);
    }
    void close(){if(out_.is_open())out_.close();}
};
inline std::uint64_t frameAt(double elapsed,int fps){return static_cast<std::uint64_t>(std::max(0.0,std::floor(elapsed*fps)));}
}
