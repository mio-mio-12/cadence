#pragma once
#include "scene/Math3D.h"
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <unordered_map>
namespace gameplay::bot {
// Exact inputs, including elevation: no merging of different floors or edges.
// The owner invalidates this on map collision/navigation-policy changes.
class RouteProbeCache {
    using Key=std::array<std::uint32_t,6>;
    struct Hash {std::size_t operator()(const Key& key)const{std::size_t h=0;for(auto v:key)h=(h*16777619u)^v;return h;}};
    std::unordered_map<Key,std::optional<scene::Vec3>,Hash> entries_;
    std::uint64_t revision_{};
public:
    unsigned hits{},misses{};
    void reset(){entries_.clear();hits=misses=0;}
    void setRevision(std::uint64_t value){if(value!=revision_){revision_=value;reset();}}
    template<class Probe> std::optional<scene::Vec3> query(scene::Vec3 from,scene::Vec3 to,Probe&& probe){
        const Key key{std::bit_cast<std::uint32_t>(from.x),std::bit_cast<std::uint32_t>(from.y),std::bit_cast<std::uint32_t>(from.z),std::bit_cast<std::uint32_t>(to.x),std::bit_cast<std::uint32_t>(to.y),std::bit_cast<std::uint32_t>(to.z)};
        if(auto it=entries_.find(key);it!=entries_.end()){++hits;return it->second;}
        ++misses;auto result=probe(from,to);if(entries_.size()>=32768)entries_.clear();entries_.emplace(key,result);return result;
    }
};
}
