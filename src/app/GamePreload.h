#pragma once
#include "cast/CastDocument.h"
#include "content/AssetPaths.h"
#include "scene/CastScene.h"
#include "render/PreparedTexture.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace cadence {
// Only immutable CPU assets are retained. No renderer, GL, app state or rigs
// cross onto this worker. Destruction/eviction also happens off the UI thread.
class GamePreload {
public:
    struct Status {
        bool requested{},running{},estimateRequested{};
        std::string stage{"Not loaded"};
        std::size_t done{},total{},failed{},bytes{},sourceBytes{},textureBytes{};
        std::deque<std::string> log;
    };
    GamePreload():worker_([this]{run();}){}
    ~GamePreload(){stop_=true;cv_.notify_all();worker_.join();}
    GamePreload(const GamePreload&)=delete;
    void select(const std::filesystem::path& path,bool enabled){
        std::lock_guard lock(mutex_);auto& s=states_[path];s.requested=enabled;s.estimateRequested=false;pending_.insert(path);cv_.notify_all();
    }
    void estimate(const std::filesystem::path& path){std::lock_guard lock(mutex_);auto& s=states_[path];if(!s.requested){s.estimateRequested=true;pending_.insert(path);cv_.notify_all();}}
    Status status(const std::filesystem::path& path){std::lock_guard lock(mutex_);return states_[path];}
    void pause(bool value){if(paused_.exchange(value)!=value)cv_.notify_all();}
    void budget(std::size_t bytes){budget_=bytes;}
    std::size_t bytes() const{return resident_;}
private:
    struct Owned {std::vector<std::filesystem::path> documents;std::vector<render::texture::Request> textures;std::size_t bytes{};};
    std::mutex mutex_;std::condition_variable cv_;
    std::map<std::filesystem::path,Status> states_;
    std::map<std::filesystem::path,Owned> owned_;
    std::set<std::filesystem::path> pending_;
    std::atomic<bool> stop_{},paused_{};
    std::atomic<std::size_t> budget_{4ull*1024*1024*1024},resident_{};
    std::thread worker_;
    template<class F>void update(const std::filesystem::path& p,F f){std::lock_guard lock(mutex_);f(states_[p]);}
    void log(const std::filesystem::path& p,const std::string& message){update(p,[&](Status& s){s.log.push_back(message);if(s.log.size()>200)s.log.pop_front();});}
    bool checkpoint(const std::filesystem::path& p){
        std::unique_lock lock(mutex_);
        cv_.wait(lock,[&]{return stop_||(!states_[p].requested&&!states_[p].estimateRequested)||!paused_;});
        return !stop_&&(states_[p].requested||states_[p].estimateRequested);
    }
    bool room(std::size_t extra)const{
        if(resident_>=budget_||extra>budget_-resident_)return false;
#ifdef _WIN32
        MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);
        if(GlobalMemoryStatusEx(&memory)&&memory.ullAvailPhys<extra+1024ull*1024*1024)return false;
#endif
        return true;
    }
    void unload(const std::filesystem::path& p){
        auto& o=owned_[p];for(const auto& f:o.documents)cast::Document::releasePreload(f);
        for(const auto& r:o.textures)render::texture::releasePreload(r);
        resident_-=o.bytes;o={};update(p,[](Status& s){s.bytes=0;s.running=false;s.stage="Not loaded";});
    }
    void load(const std::filesystem::path& p,bool estimateOnly){
        content::ObservationPause suppressUnusedDependencies;
        unload(p);update(p,[](Status& s){s.running=true;s.stage="Discovering files";s.done=s.total=s.failed=s.sourceBytes=s.textureBytes=0;s.log.clear();});
        std::vector<std::filesystem::path> files;std::error_code ec;
        for(std::filesystem::recursive_directory_iterator it(p,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
            if(!checkpoint(p))return;
            if(ec){log(p,"Discovery error: "+ec.message());ec.clear();continue;}
            auto ext=it->path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(it->is_regular_file(ec)&&ext==".cast"){
                files.push_back(it->path());const auto n=it->file_size(ec);
                update(p,[&](Status& s){s.total=files.size();if(!ec)s.sourceBytes+=n;});
            }else if(estimateOnly&&(ext==".png"||ext==".dds"||ext==".tga"||ext==".webp"||ext==".jpg")){
                const auto n=it->file_size(ec);if(!ec)update(p,[&](Status& s){s.textureBytes+=n;});
            }
        }
        if(ec)throw std::runtime_error(ec.message());
        if(estimateOnly){update(p,[](Status& s){s.stage="Estimate ready";s.running=false;s.estimateRequested=false;});return;}
        std::sort(files.begin(),files.end());
        update(p,[](Status& s){s.stage="Caching models and animations";});
        auto& o=owned_[p];std::vector<render::texture::Request> textures;std::set<std::string> keys;
        const auto add=[&](const std::filesystem::path& path,bool opaque,bool srgb,bool independent=false){
            if(path.empty())return;
            const auto key=path.string()+std::to_string(opaque)+std::to_string(srgb)+std::to_string(independent);
            if(keys.insert(key).second){textures.push_back({path,opaque,false,srgb,1024,16384,true,independent});std::error_code e;const auto n=std::filesystem::file_size(path,e);if(!e)update(p,[&](Status& s){s.textureBytes+=n;});}
        };
        for(const auto& f:files){
            if(!checkpoint(p))return;
            const auto n=std::filesystem::file_size(f,ec);if(ec){ec.clear();continue;}
            if(n>256ull*1024*1024||!room(n*4+128ull*1024*1024))throw std::runtime_error("Memory limit reached; partial cache retained");
            log(p,"Reading / parsing: "+f.filename().string());
            const auto bytes=cast::Document::preload(f);
            if(bytes){o.documents.push_back(content::resolve(f));o.bytes+=bytes;resident_+=bytes;
                const auto document=cast::Document::load(f);
                if(document.statistics().models){
                    log(p,"Resolving model materials: "+f.filename().string());
                    const auto scene=scene::buildScene(document,false);
                    for(const auto& m:scene.meshes){
                        const bool opaque=(m.ignoreAlbedoAlpha||m.viewmodelWeapon||m.camoBlend)&&!m.forceAlpha&&!m.decal&&!m.alphaTest&&!(m.camoBlend&&m.camoUseAlpha);
                        add(m.albedoPath,opaque,m.materialPolicyExplicit,m.camoBlend&&m.camoUseAlpha&&!m.forceAlpha&&!m.decal&&!m.alphaTest);
                        // Weapon/viewhand classification can change after assembly.
                        add(m.albedoPath,!opaque,m.materialPolicyExplicit);
                        for(const auto* path:{&m.normalPath,&m.specularPath,&m.metalnessPath,&m.roughnessPath})add(*path,false,false,true);
                        add(m.emissivePath,false,m.materialPolicyExplicit);
                    }
                }
            }else log(p,"Failed to cache: "+f.string());
            update(p,[&](Status& s){++s.done;if(!bytes)++s.failed;s.bytes=o.bytes;});
        }
        update(p,[&](Status& s){s.stage="Preparing textures";s.done=0;s.total=textures.size();});
        for(const auto& r:textures){
            if(!checkpoint(p))return;
            if(!room(render::texture::preparationBufferBudget))throw std::runtime_error("Memory limit reached; partial cache retained");
            log(p,"Decoding texture: "+r.path.filename().string());const auto bytes=render::texture::preload(r);
            if(bytes){o.textures.push_back(r);o.bytes+=bytes;resident_+=bytes;}else log(p,"Not cached (normal loader will handle): "+r.path.string());
            update(p,[&](Status& s){++s.done;if(!bytes)++s.failed;s.bytes=o.bytes;});
        }
        update(p,[](Status& s){s.running=false;s.stage=s.failed?"Finished with uncached assets":"CPU assets cached";});
        log(p,"Finished. GPU upload and cross-rig assembly still run when needed.");
    }
    void run(){
        while(!stop_){std::filesystem::path path;bool enabled{},estimateOnly{};
            {std::unique_lock lock(mutex_);cv_.wait(lock,[&]{return stop_||!pending_.empty();});if(stop_)break;
            path=*pending_.begin();pending_.erase(pending_.begin());enabled=states_[path].requested;estimateOnly=states_[path].estimateRequested;}
            try{if(enabled||estimateOnly)load(path,!enabled);else unload(path);}catch(const std::exception& e){log(path,e.what());update(path,[&](Status& s){s.running=false;s.estimateRequested=false;s.stage=e.what();s.bytes=owned_[path].bytes;});}
        }
        for(auto& [p,o]:owned_)unload(p);
    }
};
}
