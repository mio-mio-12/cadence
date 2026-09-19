#include "scene/C2MMap.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::filesystem::path> testPaths;
    if (argc >= 2) {
        testPaths.push_back(std::filesystem::u8path(argv[1]));
    } else {
        const std::filesystem::path defaultRaid = R"(G:\COD Modding\tools\C2M_COD\C2MBeta 082026\C2MBeta\exported_maps\black_ops_2\mp_raid\mp_raid.c2m)";
        const std::filesystem::path defaultCrossfire = R"(G:\COD Modding\tools\C2M_COD\C2MBeta 082026\C2MBeta\exported_maps\modern_warfare\mp_crossfire\mp_crossfire.c2m)";
        const std::filesystem::path defaultSkyway = R"(G:\COD Modding\tools\C2M_COD\C2MBeta 082026\C2MBeta\exported_maps\infinite_warfare\mp_skyway\mp_skyway.c2m)";
        if (std::filesystem::is_regular_file(defaultRaid)) testPaths.push_back(defaultRaid);
        if (std::filesystem::is_regular_file(defaultSkyway)) testPaths.push_back(defaultSkyway);
        if (std::filesystem::is_regular_file(defaultCrossfire)) testPaths.push_back(defaultCrossfire);
    }
    for (const auto& testPath : testPaths) {
        if (!std::filesystem::is_regular_file(testPath)) continue;
        scene::glb::Map map;
        std::string error;
        const bool loaded = scene::c2m::load(testPath, map, error, 1.0f, [](std::string_view msg, float p) {
            std::cout << "  [" << static_cast<int>(p * 100) << "%] " << msg << "\n";
        });
        if (!loaded) {
            std::cerr << "Failed to load C2M (" << testPath.filename().string() << "): " << error << "\n";
            return 1;
        }

        const auto size = map.scene.bounds.maximum - map.scene.bounds.minimum;
        std::cout << "Probing key points for " << testPath.filename().string() << ":\n";
        const std::vector<std::pair<std::string, scene::Vec3>> probePoints = {
            {"Raid Waterfall Fountain", {9350.56f, 8620.975f, 88.9f}},
            {"Raid Passageway Stairs", {8173.03f, 10035.10f, 91.44f}},
            {"Raid Flowers Wall", {3789.76f, 9224.47f, 309.43f}},
            {"Raid Bougainvillea", {3622.439f, 9222.781f, 233.415f}},
            {"Raid Rocks Slope", {2851.081f, 9336.905f, 337.014f}},
            {"Raid Elevating Stairs", {932.6868f, 9175.016f, 48.768f}},
            {"Skyway Blocked Passage 1", {875.53f, 1878.03f, 18.415f}},
            {"Skyway Blocked Passage 2", {1754.32f, 1868.78f, 381.36f}},
            {"Skyway TV Screens", {-1971.02f, 7142.19f, 173.22f}}
        };
        for (const auto& [name, pt] : probePoints) {
            constexpr float noGround = -1e9f;
            const float g = map.groundHeight(pt.x, pt.y, pt.z + 50.0f, noGround);
            std::cout << "  [" << name << "] (" << pt.x << ", " << pt.y << ", " << pt.z << ") -> Ground: " << (g > noGround*0.5f ? std::to_string(g) : "NO GROUND") << "\n";
            int foundTri = 0;
            for (const auto& mesh : map.scene.meshes) {
                for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                    const auto& v0 = mesh.vertices[mesh.indices[i]].position;
                    const auto& v1 = mesh.vertices[mesh.indices[i+1]].position;
                    const auto& v2 = mesh.vertices[mesh.indices[i+2]].position;
                    const float minX = std::min({v0.x, v1.x, v2.x}) - 150.0f, maxX = std::max({v0.x, v1.x, v2.x}) + 150.0f;
                    const float minY = std::min({v0.y, v1.y, v2.y}) - 150.0f, maxY = std::max({v0.y, v1.y, v2.y}) + 150.0f;
                    const float minZ = std::min({v0.z, v1.z, v2.z}) - 150.0f, maxZ = std::max({v0.z, v1.z, v2.z}) + 150.0f;
                    if (pt.x >= minX && pt.x <= maxX && pt.y >= minY && pt.y <= maxY && pt.z >= minZ && pt.z <= maxZ) {
                        std::cout << "    Near Mesh: " << mesh.name << " (mat=" << mesh.materialName << ", decal=" << mesh.decal << ", forceAlpha=" << mesh.forceAlpha << ")\n";
                        if (++foundTri >= 15) break;
                    }
                }
                if (foundTri >= 6) break;
            }
        }

        std::size_t texturedCount = 0;
        int printCount = 0;
        for (const auto& mesh : map.scene.meshes) {
            if (!mesh.albedoPath.empty()) {
                texturedCount++;
            } else if (printCount < 10) {
                std::cout << "  Untextured batch: " << mesh.name << " (mat=" << mesh.materialName << ")\n";
                printCount++;
            }
        }

        std::cout << "C2M Test Passed (" << testPath.filename().string() << "): "
                  << map.scene.meshes.size() << " material batches (" << texturedCount << " textured), "
                  << map.collision.size() << " collision triangles, bounds=("
                  << size.x << "x" << size.y << "x" << size.z << " cm), spawn=("
                  << map.defaultSpawnPoint.x << ", " << map.defaultSpawnPoint.y << ", " << map.defaultSpawnPoint.z << ")\n";
        std::cout << "  Grid: " << map.gridWidth << "x" << map.gridHeight
                  << ", GlobalCollision: " << map.globalCollision.size()
                  << ", GlobalWalkable: " << map.globalWalkable.size()
                  << ", GlobalBlocking: " << map.globalBlocking.size() << "\n";

        auto tStart = std::chrono::steady_clock::now();
        scene::Vec3 pos = map.defaultSpawnPoint;
        for (int i = 0; i < 50; ++i) {
            scene::Vec3 proposed = pos + scene::Vec3{5.0f, 5.0f, 0.0f};
            pos = map.constrainMove(pos, proposed, 35.0f, 180.0f, 45.0f);
        }
        auto tEnd = std::chrono::steady_clock::now();
        float avgMs = std::chrono::duration<float, std::milli>(tEnd - tStart).count() / 50.0f;
        std::cout << "  constrainMove avg time: " << avgMs << " ms per call\n";
    }

    std::cout << "C2MMapTests all passed successfully.\n";
    return 0;
}
