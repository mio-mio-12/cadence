#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace render::reshade_native {
struct Settings {bool enabled{};std::string preset{"Default.ini"};};
struct Technique {std::uint64_t handle{};std::string name,effect;bool enabled{};};
struct Uniform {std::uint64_t handle{};std::string name,label,effect,ui;int type{},count{1};std::array<float,4> value{};float minimum{},maximum{1};bool ranged{};};
class Runtime {
public:
    Runtime();~Runtime();Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
    bool process(const std::filesystem::path& root,const std::string& preset,unsigned colorTexture,int width,int height,unsigned depthTexture=0,float nearPlane=.1f,float farPlane=1000,bool foreground=false,bool separate=false);
    void shutdown();void reload();void savePreset();void exportPreset(const std::filesystem::path& path);
    bool ready()const;const std::string& error()const;
    std::vector<Technique> techniques()const;std::vector<Uniform> uniforms(const std::string& effect)const;
    void enableTechnique(std::uint64_t handle,bool enabled);void setUniform(const Uniform& uniform);
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
