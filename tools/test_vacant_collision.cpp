
#include "src/scene/C2MMap.h"
#include "src/scene/GlbMap.h"
#include <iostream>
#include <chrono>

int main() {
    scene::glb::Map map;
    std::string err;
    std::cout << "Loading mp_vacant..." << std::endl;
    auto t0 = std::chrono::steady_clock::now();
    bool ok = scene::c2m::load(R"(G:\COD Modding\tools\C2M_COD\C2MBeta 082026\C2MBeta\exported_maps\modern_warfare_rm\mp_vacant\mp_vacant.c2m)", map, err);
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "Load result: " << ok << " time: " << std::chrono::duration<float>(t1 - t0).count() << "s" << std::endl;
    std::cout << "Total collision triangles: " << map.collision.size() << std::endl;
    std::cout << "Grid width: " << map.gridWidth << " height: " << map.gridHeight << std::endl;
    std::cout << "Global collision: " << map.globalCollision.size() << std::endl;
    std::cout << "Global walkable: " << map.globalWalkable.size() << std::endl;
    std::cout << "Global blocking: " << map.globalBlocking.size() << std::endl;

    scene::Vec3 spawn = map.defaultSpawnPoint;
    std::cout << "Spawn point: " << spawn.x << ", " << spawn.y << ", " << spawn.z << std::endl;

    auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) {
        scene::Vec3 proposed = spawn + scene::Vec3{5.0f, 5.0f, 0.0f};
        spawn = map.constrainMove(spawn, proposed, 35.0f, 180.0f, 45.0f);
    }
    auto t3 = std::chrono::steady_clock::now();
    float avgMs = std::chrono::duration<float, std::milli>(t3 - t2).count() / 20.0f;
    std::cout << "constrainMove avg time: " << avgMs << " ms" << std::endl;

    return 0;
}
