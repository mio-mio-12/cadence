#include "assets/LocalAssetPaths.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <unordered_set>
#include "assets/AssetCatalog.h"
#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "weapon/WeaponProfile.h"

void verifyWeaponRig(const std::string& game,
                     const std::filesystem::path& handsPath,
                     const std::filesystem::path& weaponPath,
                     const std::filesystem::path& animPath) {
    std::cout << "\n========================================\n";
    std::cout << "VERIFYING GAME: " << game << "\n";
    std::cout << "========================================\n";
    
    // Check paths existence
    std::cout << "Hands file: " << handsPath << (std::filesystem::exists(handsPath) ? " [EXISTS]" : " [MISSING]") << "\n";
    std::cout << "Weapon file: " << weaponPath << (std::filesystem::exists(weaponPath) ? " [EXISTS]" : " [MISSING]") << "\n";
    std::cout << "Anim file: " << animPath << (std::filesystem::exists(animPath) ? " [EXISTS]" : " [MISSING]") << "\n";

    if (!std::filesystem::exists(handsPath) || !std::filesystem::exists(weaponPath) || !std::filesystem::exists(animPath)) {
        std::cout << "FAILED: Missing files for " << game << "\n";
        return;
    }

    // Check category & role
    std::string cat = assets::categoryFromPath(weaponPath);
    assets::Role role = assets::classifyModelPath(weaponPath, weaponPath.stem().string());
    std::cout << "Category resolved: '" << cat << "'\n";
    std::cout << "Role resolved: '" << assets::roleName(role) << "'\n";

    // Load hands scene
    auto handsDoc = cast::Document::load(handsPath);
    if (!handsDoc.valid()) {
        std::cout << "ERROR loading hands doc\n";
        return;
    }
    scene::CastScene scene = scene::buildScene(handsDoc);
    std::size_t handsBones = scene.skeleton.bones.size();
    std::cout << "Hands loaded: " << handsBones << " bones\n";
    if (scene.skeleton.boneByCanonicalName.contains("tag_weapon")) {
        std::size_t b = scene.skeleton.boneByCanonicalName.at("tag_weapon");
        std::cout << "  Hands tag_weapon restGlobal Z: " << scene.skeleton.bones[b].restGlobal.v[14] << "\n";
    }
    if (scene.skeleton.boneByCanonicalName.contains("tag_origin")) {
        std::size_t b = scene.skeleton.boneByCanonicalName.at("tag_origin");
        std::cout << "  Hands tag_origin restGlobal Z: " << scene.skeleton.bones[b].restGlobal.v[14] << "\n";
    }

    auto weaponDoc = cast::Document::load(weaponPath);
    if (!weaponDoc.valid()) {
        std::cout << "ERROR loading weapon doc\n";
        return;
    }
    auto rawWeaponScene = scene::buildScene(weaponDoc);
    std::cout << "Raw weapon bounds: min=[" << rawWeaponScene.bounds.minimum.x << ", " << rawWeaponScene.bounds.minimum.y << ", " << rawWeaponScene.bounds.minimum.z 
              << "] max=[" << rawWeaponScene.bounds.maximum.x << ", " << rawWeaponScene.bounds.maximum.y << ", " << rawWeaponScene.bounds.maximum.z << "]\n";
    if (rawWeaponScene.skeleton.boneByCanonicalName.contains("j_gun")) {
        auto b = rawWeaponScene.skeleton.boneByCanonicalName.at("j_gun");
        std::cout << "Raw weapon j_gun restGlobal Z: " << rawWeaponScene.skeleton.bones[b].restGlobal.v[14] << "\n";
    } else if (rawWeaponScene.skeleton.boneByCanonicalName.contains("tag_origin")) {
        auto b = rawWeaponScene.skeleton.boneByCanonicalName.at("tag_origin");
        std::cout << "Raw weapon tag_origin restGlobal Z: " << rawWeaponScene.skeleton.bones[b].restGlobal.v[14] << "\n";
    }

    scene::appendRigModel(weaponDoc, scene, weaponPath.stem().string());
    std::size_t mergedBones = scene.skeleton.bones.size();
    std::cout << "Weapon merged successfully: " << mergedBones << " total bones (" 
              << (mergedBones - handsBones) << " weapon-specific joints added)\n";
    if (scene.skeleton.boneByCanonicalName.contains("j_gun")) {
        auto b = scene.skeleton.boneByCanonicalName.at("j_gun");
        std::cout << "Merged weapon j_gun restGlobal Z: " << scene.skeleton.bones[b].restGlobal.v[14] << "\n";
        std::cout << "Merged weapon j_gun parent: " << scene.skeleton.bones[b].parent << " (" 
                  << (scene.skeleton.bones[b].parent >= 0 ? scene.skeleton.bones[scene.skeleton.bones[b].parent].name : "NONE") << ")\n";
    }

    // Check tag_weapon / tag_weapon_right presence
    bool hasTagWeapon = scene.skeleton.boneByCanonicalName.contains("tag_weapon") ||
                        scene.skeleton.boneByCanonicalName.contains("tag_weapon_right");
    std::cout << "Mount socket present: " << (hasTagWeapon ? "YES" : "NO") << "\n";

    // Load animation
    auto animDoc = cast::Document::load(animPath);
    if (!animDoc.valid()) {
        std::cout << "ERROR loading anim doc\n";
        return;
    }
    scene::appendAnimations(animDoc, scene);
    const auto& clip = scene.animations.back();
    std::cout << "Animation clip: '" << clip.name << "' (" << clip.durationFrames << " frames, " 
              << clip.tracks.size() << " authored tracks)\n";

    // Evaluate track resolution against merged skeleton
    std::size_t matchedTracks = 0;
    for (const auto& track : clip.tracks) {
        if (track.boneIndex != static_cast<std::uint32_t>(-1) && track.boneIndex < scene.skeleton.bones.size()) {
            ++matchedTracks;
        }
    }
    float matchPct = (clip.tracks.empty() ? 0.0f : (100.0f * matchedTracks / clip.tracks.size()));
    std::cout << "Track binding: " << matchedTracks << "/" << clip.tracks.size() 
              << " (" << matchPct << "%) tracks successfully mapped to rig joints\n";

    // Sample a pose
    auto pose = scene.samplePose(0, 0.0f);
    std::cout << "Pose sampling: SUCCESS (" << pose.size() << " matrices calculated)\n";

    // Simulate evaluateCurrentPose viewmodel transform
    auto cameraBoneIt = scene.skeleton.boneByCanonicalName.find("tag_camera");
    bool hasCameraBone = cameraBoneIt != scene.skeleton.boneByCanonicalName.end() && cameraBoneIt->second < pose.size();
    scene::Mat4 desiredCamera = scene::Mat4::identity();
    desiredCamera.v[14] = 60.0f * 2.54f; // 152.4 cm standing eye height
    scene::Mat4 bindToGameplayCamera = hasCameraBone
        ? (desiredCamera * scene::inverseAffine(scene.skeleton.bones[cameraBoneIt->second].restGlobal))
        : desiredCamera;
    
    std::cout << "tag_camera restGlobal Z: " << (hasCameraBone ? scene.skeleton.bones[cameraBoneIt->second].restGlobal.v[14] : -999.0f) << "\n";
    std::cout << "desiredCamera Z: " << desiredCamera.v[14] << "\n";
    std::cout << "bindToGameplayCamera translation Z: " << bindToGameplayCamera.v[14] << "\n";

    auto printBone = [&](std::string_view name) {
        if (scene.skeleton.boneByCanonicalName.contains(std::string{name})) {
            auto b = scene.skeleton.boneByCanonicalName.at(std::string{name});
            auto rawP = pose[b];
            auto finalP = bindToGameplayCamera * rawP;
            std::cout << "  " << name << " raw pos: [" << rawP.v[12] << ", " << rawP.v[13] << ", " << rawP.v[14] 
                      << "] -> gameplay pos: [" << finalP.v[12] << ", " << finalP.v[13] << ", " << finalP.v[14] << "]\n";
        } else {
            std::cout << "  " << name << ": NOT FOUND\n";
        }
    };
    printBone("tag_camera");
    printBone("tag_weapon");
    printBone("j_gun");
    printBone("tag_origin");
    printBone("j_wrist_ri");
    printBone("j_wrist_le");
}

int main() {
    std::filesystem::path root = cadence::local_assets::exportPath("");

    // 1. BO2
    verifyWeaponRig("BO2",
        root / "bo2/models/viewhands/seal6/c_usa_mp_seal6_longsleeve_viewhands/c_usa_mp_seal6_longsleeve_viewhands_LOD0.cast",
        root / "bo2/models/weapons/view/assault rifles/t6_wpn_ar_an94_view/t6_wpn_ar_an94_view_LOD0.cast",
        root / "bo2/animations/viewmodel/an94/viewmodel_an94_idle.cast");

    // 2. Ghosts
    verifyWeaponRig("Ghosts",
        root / "ghosts/models/viewhands/elite pmc/viewhands_elite_pmc_urban/viewhands_elite_pmc_urban_LOD0.cast",
        root / "ghosts/models/weapons/view/marksman rifles/viewmodel_mk14_ebr/viewmodel_mk14_ebr_LOD0.cast",
        root / "ghosts/animations/viewmodel/mk14/viewmodel_mk14_idle.cast");

    // 3. AW
    verifyWeaponRig("AW",
        root / "aw/models/viewhands/viewmodel_base_viewhands/viewmodel_base_viewhands_LOD0.cast",
        root / "aw/models/weapons/view/assault rifles/vm_bal27_base_standard/vm_bal27_base_standard_LOD0.cast",
        root / "aw/animations/viewmodel/bal27/vm_bal27_idle.cast");

    // 4. MW3
    verifyWeaponRig("MW3",
        root / "mw3/models/viewhands/viewhands_delta/viewhands_delta_LOD0.cast",
        root / "mw3/models/weapons/view/rifles/viewmodel_m4_iw5/viewmodel_m4_iw5_LOD0.cast",
        root / "mw3/animations/viewmodel/m4a1/viewmodel_m4_idle.cast");

    // 5. AW Catalog inspection
    std::cout << "\n========================================\n";
    std::cout << "TESTING AW CATALOG SCAN:\n";
    std::cout << "========================================\n";
    assets::Catalog awCat;
    std::string err;
    assets::scan(root / "aw", awCat, err);
    std::cout << "AW scannedCastFiles: " << awCat.scannedCastFiles << "\n";
    std::cout << "AW entries: " << awCat.entries.size() << "\n";
    std::size_t morsCount = 0;
    std::size_t viewWeapons = 0;
    for (const auto& entry : awCat.entries) {
        if (entry.role == assets::Role::ViewWeapon) viewWeapons++;
        if (entry.name.find("mors") != std::string::npos) {
            morsCount++;
            std::cout << "  MORS: " << entry.name << " | role: " << assets::roleName(entry.role) << " | cat: " << entry.category << "\n";
        }
    }
    std::cout << "Total ViewWeapons in AW: " << viewWeapons << "\n";
    std::cout << "Total MORS in AW: " << morsCount << "\n";

    return 0;
}
