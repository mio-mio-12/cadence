#pragma once

#include "scene/CastScene.h"
#include "scene/C2mxTypes.h"
#include "scene/AuthoredGameplay.h"

#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace scene::glb {

struct CollisionTriangle {
    Vec3 a{},b{},c{},normal{};
    Vec3 minimum{},maximum{};
    bool walkable{};
    bool blocking{};
    bool bounce{false};
    bool speedboost{false};
    bool speedboost2{false};
    bool ladder{false};
};

// Bounds over contiguous ranges of the existing cell lists. Never reorder
// contacts: the wall solver and equal-distance hits depend on that ordering.
struct CollisionRangeIndex {
    struct Node {
        Vec3 minimum{}, maximum{};
        float groundMinimum{-std::numeric_limits<float>::infinity()};
        std::uint32_t begin{}, end{}, escape{};
        bool leaf{};
    };
    std::vector<Node> nodes;
    // Last slot describes the global list. Small lists use the linear path.
    std::vector<std::uint32_t> roots;
};

struct Map {
    authored::Data gameplay;
    CastScene scene;
    std::uint64_t collisionRevision{};
    AuthoredCollisionMetadata authoredCollision;
    std::vector<AuthoredCollider> authoredColliders;
    std::vector<CollisionTriangle> authoredTriggerTriangles;
    std::vector<CollisionTriangle> collision;
    // Authored movement hulls may close visible openings. Shots use a separate
    // immutable geometry index; physics and navigation retain the authored hulls.
    std::shared_ptr<Map> shotGeometry;
    float boundsMinX{-10000.0f}, boundsMinY{-10000.0f}, boundsMaxX{10000.0f}, boundsMaxY{10000.0f};
    int gridWidth{0}, gridHeight{0};
    std::vector<std::vector<std::uint32_t>> gridCollision;
    std::vector<std::vector<std::uint32_t>> gridWalkable;
    std::vector<std::vector<std::uint32_t>> gridBlocking;
    std::vector<std::uint32_t> globalCollision;
    std::vector<std::uint32_t> globalWalkable;
    std::vector<std::uint32_t> globalBlocking;
    CollisionRangeIndex walkableRanges, blockingRanges;
    mutable std::vector<std::uint32_t> triangleTag;
    mutable std::uint32_t queryEpoch{1};
    float collisionCellSize{512.0f};

    [[nodiscard]] std::size_t activeCellCount() const {
        std::size_t count = 0;
        for (const auto& c : gridCollision) {
            if (!c.empty()) ++count;
        }
        return count;
    }
    std::filesystem::path sourcePath;
    float scaleMultiplier{1.0f};
    Vec3 defaultSpawnPoint{};
    bool hasDefaultSpawnPoint{false};
    std::optional<Vec3> collisionPreviewPoint;

    [[nodiscard]] inline int gridCoordX(float x) const {
        return static_cast<int>(std::floor((x - boundsMinX) / collisionCellSize));
    }
    [[nodiscard]] inline int gridCoordY(float y) const {
        return static_cast<int>(std::floor((y - boundsMinY) / collisionCellSize));
    }
    [[nodiscard]] inline int cellIndex(int cx, int cy) const {
        if (cx < 0 || cx >= gridWidth || cy < 0 || cy >= gridHeight) return -1;
        return cy * gridWidth + cx;
    }

    struct RaycastHit {
        Vec3 position{};
        Vec3 normal{0.0f, 0.0f, 1.0f};
        float distance{};
    };
    [[nodiscard]] float groundHeight(float x,float y,float referenceZ,float fallback) const;
    [[nodiscard]] float navigationGroundHeight(float x,float y,float referenceZ,float fallback,float footprintRadius) const;
    [[nodiscard]] std::optional<Vec3> raycastWalkable(Vec3 origin,Vec3 direction,float maxDistance) const;
    [[nodiscard]] std::optional<RaycastHit> raycastSurface(Vec3 origin,Vec3 direction,float maxDistance) const;
    [[nodiscard]] std::optional<RaycastHit> raycastShot(Vec3 origin,Vec3 direction,float maxDistance) const {
        return shotGeometry ? shotGeometry->raycastSurface(origin,direction,maxDistance) : raycastSurface(origin,direction,maxDistance);
    }
    [[nodiscard]] bool lineOfSight(Vec3 from,Vec3 to) const;
    [[nodiscard]] bool navigationSegmentClear(Vec3 from,Vec3 to,float radius,float height,float stepHeight) const;
    [[nodiscard]] Vec3 constrainMove(Vec3 oldPosition,Vec3 proposed,float radius,float height,float stepHeight,Vec3* outContactNormal=nullptr) const;
    struct SurfContact {
        Vec3 normal{0.0f, 0.0f, 1.0f};
        float distance{0.0f};
        float separation{0.0f};
        bool hit{false};
    };
    [[nodiscard]] SurfContact findSurfContact(Vec3 position, float radius, float height, float searchMargin) const;
    [[nodiscard]] std::optional<Vec3> mantleTarget(Vec3 position,Vec3 forward,float radius,float height,float stepHeight,float maxHeight,float checkRange,float minHeight=-std::numeric_limits<float>::max(),bool useAuthored=true) const;
    [[nodiscard]] bool isBounceSurface(float x, float y, float z, float radius) const;
    [[nodiscard]] int speedBoostTier(float x, float y, float z, float radius) const;
    [[nodiscard]] std::optional<Vec3> findLadderContact(Vec3 position, float radius, float height) const;
    void buildCollisionIndex();
};

[[nodiscard]] bool load(const std::filesystem::path& path,Map& map,std::string& error,float scaleMultiplier=1.0f,bool buildRenderCollision=true);

} // namespace scene::glb
