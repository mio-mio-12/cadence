#pragma once
#include "scene/GlbMap.h"
#include "scene/BoundedJson.h"
#include <span>
namespace scene::c2mx {
struct Extension {bool present{};std::size_t baseEnd{};codm::Json meta,collision,navigation;};
Extension readExtension(std::span<const std::uint8_t> bytes);
bool applyCollision(const codm::Json& collision,const codm::Json& meta,const codm::Json& navigation,
                    glb::Map& map,std::string& error,const c2m::LoadOptions& options={},bool rebuildIndex=true);
bool loadCollisionSidecar(const std::filesystem::path& path,glb::Map& map,std::string& error,const c2m::LoadOptions& options={});
bool chooseAuthoredSpawn(glb::Map& map);
std::vector<std::string> triggerIdsAtPoint(const glb::Map& map,Vec3 point);
}
