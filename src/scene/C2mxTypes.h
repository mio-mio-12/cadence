#pragma once
#include "scene/CastScene.h"
#include <cstdint>
#include <string>
#include <vector>
namespace scene::c2m {
struct LoadOptions {bool allowApproximateCollision{};unsigned primitiveSegments{16};std::uint64_t solidLayerMask{0xffffffffffffffffull};};
}
namespace scene::glb {
struct AuthoredCollider {
    std::string id,name,kind,physicsMaterialJson,sourceJson;
    bool enabled{true},trigger{},approximate{};unsigned layer{};
    Mat4 matrix{Mat4::identity()};Vec3 center{},size{};float radius{},height{};int axis{};
    Bounds bounds;std::size_t firstTriangle{},triangleCount{};
};
struct AuthoredCollisionMetadata {
    bool present{},complete{},physicsReady{true},approximate{},nativeNavigationReady{};
    std::size_t colliderCount{},triggerCount{};
    unsigned primitiveSegments{};std::uint64_t solidLayerMask{0xffffffffffffffffull};
    std::string status,navigationStatus,metaJson,collisionJson,navigationJson;
};
}
