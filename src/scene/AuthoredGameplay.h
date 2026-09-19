#pragma once
#include "scene/CastScene.h"
#include <filesystem>
#include <optional>
#include <unordered_map>

namespace scene::authored {
enum class Kind { Ladder, Entrance, Mantle, Crouch, Door };
struct Volume {
    Kind kind{};
    std::string id, target;
    Vec3 origin{}, edge[3]{}, reciprocal[3]{}, minimum{}, maximum{}, center{}, facing{};
    float angle{45}, strength{50};
    bool forbidDown{};
    bool overlaps(Vec3 feet,float radius,float height) const;
};
struct ClimbLink { Vec3 start{}, end{}; };
struct Destination { Vec3 position{}; float weight{1}; std::string kind; };
struct Data {
    bool enabled{true}, ladders{true}, mantles{true}, assistance{true};
    std::vector<Volume> volumes;
    std::vector<ClimbLink> climbs;
    std::vector<Destination> destinations;
    std::vector<std::string> warnings;
    std::unordered_map<std::int64_t,std::vector<std::size_t>> cells;
    std::vector<std::size_t> global;
    void index();
    std::vector<std::size_t> nearby(Vec3 position,float radius) const;
    const Volume* ladder(Vec3 feet,float radius,float height) const;
    bool crouch(Vec3 feet,float radius,float height) const;
    Vec3 assistedWish(Vec3 feet,Vec3 wish,float radius,float height) const;
    std::vector<Vec3> mantleDirections(Vec3 feet,Vec3 looking,float radius,float height,float reach) const;
};
// Independent optional sidecars: invalid files never replace collision or abort a map.
Data load(const std::filesystem::path& folder,float mapScale=1);
}
