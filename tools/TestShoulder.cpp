#include "assets/LocalAssetPaths.h"
#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include <iostream>
#include <iomanip>
#include <set>
#include <vector>

int main() {
    std::string bo2Hands = cadence::local_assets::exportPath("bo2/models/viewhands/cordis/c_mul_mp_cordis_assault_viewhands/c_mul_mp_cordis_assault_viewhands_LOD0.cast");
    std::string iw7Hands = cadence::local_assets::exportPath("iw_sp/models/viewhands/viewmodel_base_viewhands/viewmodel_base_viewhands_LOD0.cast");
    std::string mwHands = cadence::local_assets::exportPath("mw/models/viewhands/viewmodel_base_viewhands/viewmodel_base_viewhands_LOD0.cast");

    auto docBo2 = cast::Document::load(bo2Hands);
    auto docIw7 = cast::Document::load(iw7Hands);
    auto docMw = cast::Document::load(mwHands);

    std::string animPath = cadence::local_assets::exportPath("iw_sp/animations/vm/vm_1887_ads_fire.cast");
    auto docAnim = cast::Document::load(animPath);
    if (docAnim.valid() && docBo2.valid()) {
        auto scBo2 = scene::buildScene(docBo2);
        std::size_t animIdx = scene::appendAnimations(docAnim, scBo2);
        std::cout << "Appended animIdx=" << animIdx << ", anim count=" << scBo2.animations.size() << std::endl;
        if (!scBo2.animations.empty()) {
            // Find clavicle curves in docAnim
            scene::Track clavLe, clavRi;
            for (const auto& root : docAnim.roots()) {
                for (const auto& n : root.children) {
                    if (n.identifier == 0x76727563) { // kCurve
                        // find nn and kp
                        std::string nn = "", kp = "";
                        for (const auto& prop : n.properties) {
                            if (prop.name == "nn" && prop.stringValue) nn = *prop.stringValue;
                            if (prop.name == "kp" && prop.stringValue) kp = *prop.stringValue;
                        }
                        if (kp == "rq" || kp == "r") {
                            std::string canon = scene::canonicalName(nn);
                            auto kb = n.findProperty("kb");
                            auto kv = n.findProperty("kv");
                            // In test shoulder, we already have parsed values
                        }
                    }
                }
            }
        }
        if (!scBo2.animations.empty()) {
            const auto& a = scBo2.animations[0];
            std::cout << "Anim name=" << a.name << ", tracks=" << a.tracks.size() << std::endl;
            for (const auto& tr : a.tracks) {
                std::string bn = scBo2.skeleton.bones[tr.boneIndex].name;
                if (bn.find("shoulder") != std::string::npos || bn.find("elbow") != std::string::npos ||
                    bn.find("wrist") != std::string::npos || bn.find("weapon") != std::string::npos) {
                    std::cout << "Track: " << std::setw(15) << bn << " prop=" << (int)tr.property << " mode=" << (int)tr.mode;
                    if (tr.property == scene::TrackProperty::Rotation && !tr.rotationValues.empty()) {
                        auto q = tr.rotationValues[0];
                        std::cout << " rot0=[" << q.x << ", " << q.y << ", " << q.z << ", " << q.w << "]";
                    } else if (!tr.scalarValues.empty()) {
                        std::cout << " val0=" << tr.scalarValues[0];
                    }
                    std::cout << std::endl;
                }
            }
            auto localPose = scBo2.sampleLocalPose(0, 0.0f);
            std::cout << "\n--- Sample pose WITH rest positions on arm/hand bones ---" << std::endl;
            for (std::size_t i = 0; i < scBo2.skeleton.bones.size(); ++i) {
                const auto& b = scBo2.skeleton.bones[i];
                if (b.name.starts_with("j_shoulder") || b.name.starts_with("j_elbow") ||
                    b.name.starts_with("j_wrist") || b.name.starts_with("j_index") ||
                    b.name.starts_with("j_mid") || b.name.starts_with("j_ring") ||
                    b.name.starts_with("j_pinky") || b.name.starts_with("j_thumb")) {
                    localPose[i].position = b.restLocal.position;
                }
            }
            auto posePreserved = scBo2.globalPose(localPose);
            for (const auto& name : {"tag_torso", "tag_weapon", "j_shoulder_le", "j_shoulder_ri", "j_elbow_le", "j_elbow_ri", "j_wrist_le", "j_wrist_ri"}) {
                auto it = scBo2.skeleton.boneByName.find(name);
                if (it != scBo2.skeleton.boneByName.end()) {
                    std::size_t idx = it->second;
                    std::cout << "Without clavicle " << std::setw(15) << name << ": global pos=[" 
                              << posePreserved[idx].v[12] << ", " << posePreserved[idx].v[13] << ", " << posePreserved[idx].v[14] << "]" << std::endl;
                }
            }

            // Now test with clavicle multiplied into shoulder
            scene::Quat qClavLe{-0.136108f, 0.760620f, -0.186127f, 0.606812f};
            scene::Quat qClavRi{0.002838f, -0.639709f, 0.033966f, 0.767822f};
            auto localPoseWithClav = localPose;
            auto itShLe = scBo2.skeleton.boneByName.find("j_shoulder_le");
            auto itShRi = scBo2.skeleton.boneByName.find("j_shoulder_ri");
            if (itShLe != scBo2.skeleton.boneByName.end()) {
                localPoseWithClav[itShLe->second].rotation = scene::normalize(scene::multiply(qClavLe, localPose[itShLe->second].rotation));
            }
            if (itShRi != scBo2.skeleton.boneByName.end()) {
                localPoseWithClav[itShRi->second].rotation = scene::normalize(scene::multiply(qClavRi, localPose[itShRi->second].rotation));
            }
            auto poseWithClav = scBo2.globalPose(localPoseWithClav);
            std::cout << "\n--- Sample pose WITH clavicle * shoulder ---" << std::endl;
            for (const auto& name : {"tag_torso", "tag_weapon", "j_shoulder_le", "j_shoulder_ri", "j_elbow_le", "j_elbow_ri", "j_wrist_le", "j_wrist_ri"}) {
                auto it = scBo2.skeleton.boneByName.find(name);
                if (it != scBo2.skeleton.boneByName.end()) {
                    std::size_t idx = it->second;
                    std::cout << "With clavicle    " << std::setw(15) << name << ": global pos=[" 
                              << poseWithClav[idx].v[12] << ", " << poseWithClav[idx].v[13] << ", " << poseWithClav[idx].v[14] << "]" << std::endl;
                }
            }
        }
    }

    if (docAnim.valid() && docIw7.valid()) {
        auto scIw7 = scene::buildScene(docIw7);
        std::size_t animIdx = scene::appendAnimations(docAnim, scIw7);
        if (!scIw7.animations.empty()) {
            auto localPose = scIw7.sampleLocalPose(0, 0.0f);
            for (std::size_t i = 0; i < scIw7.skeleton.bones.size(); ++i) {
                const auto& b = scIw7.skeleton.bones[i];
                if (b.name.starts_with("j_shoulder") || b.name.starts_with("j_elbow") ||
                    b.name.starts_with("j_wrist") || b.name.starts_with("j_index") ||
                    b.name.starts_with("j_mid") || b.name.starts_with("j_ring") ||
                    b.name.starts_with("j_pinky") || b.name.starts_with("j_thumb")) {
                    localPose[i].position = b.restLocal.position;
                }
            }
            auto posePreserved = scIw7.globalPose(localPose);
            for (const auto& name : {"tag_torso", "tag_weapon", "j_shoulder_le", "j_shoulder_ri", "j_elbow_le", "j_elbow_ri", "j_wrist_le", "j_wrist_ri"}) {
                auto it = scIw7.skeleton.boneByName.find(name);
                if (it != scIw7.skeleton.boneByName.end()) {
                    std::size_t idx = it->second;
                    std::cout << "Preserved IW7 " << std::setw(15) << name << ": global pos=[" 
                              << posePreserved[idx].v[12] << ", " << posePreserved[idx].v[13] << ", " << posePreserved[idx].v[14] << "]" << std::endl;
                }
            }
        }
    }

    if (docBo2.valid()) {
        auto scBo2 = scene::buildScene(docBo2);
        std::cout << "\n=== BO2 Viewhands Bones ===" << std::endl;
        for (std::size_t i = 0; i < scBo2.skeleton.bones.size(); ++i) {
            const auto& b = scBo2.skeleton.bones[i];
            std::string pName = (b.parent >= 0 && b.parent < (int)scBo2.skeleton.bones.size()) 
                                ? scBo2.skeleton.bones[b.parent].name : "ROOT";
            if (b.name.find("shoulder") != std::string::npos || b.name.find("clavicle") != std::string::npos ||
                b.name.find("elbow") != std::string::npos || b.name.find("wrist") != std::string::npos ||
                b.name.find("weapon") != std::string::npos || b.name.find("torso") != std::string::npos) {
                std::cout << "BO2 [" << i << "] " << std::setw(20) << b.name 
                          << " (parent: " << std::setw(15) << pName << ")"
                          << " pos=[" << b.restLocal.position.x << ", " << b.restLocal.position.y << ", " << b.restLocal.position.z << "]"
                          << " rot=[" << b.restLocal.rotation.x << ", " << b.restLocal.rotation.y << ", " << b.restLocal.rotation.z << ", " << b.restLocal.rotation.w << "]"
                          << std::endl;
            }
        }
    }

    if (docIw7.valid()) {
        auto scIw7 = scene::buildScene(docIw7);
        std::cout << "\n=== IW7 Viewhands Bones ===" << std::endl;
        for (std::size_t i = 0; i < scIw7.skeleton.bones.size(); ++i) {
            const auto& b = scIw7.skeleton.bones[i];
            std::string pName = (b.parent >= 0 && b.parent < (int)scIw7.skeleton.bones.size()) 
                                ? scIw7.skeleton.bones[b.parent].name : "ROOT";
            if (b.name.find("shoulder") != std::string::npos || b.name.find("clavicle") != std::string::npos ||
                b.name.find("elbow") != std::string::npos || b.name.find("wrist") != std::string::npos ||
                b.name.find("weapon") != std::string::npos || b.name.find("torso") != std::string::npos) {
                std::cout << "IW7 [" << i << "] " << std::setw(20) << b.name 
                          << " (parent: " << std::setw(15) << pName << ")"
                          << " pos=[" << b.restLocal.position.x << ", " << b.restLocal.position.y << ", " << b.restLocal.position.z << "]"
                          << " rot=[" << b.restLocal.rotation.x << ", " << b.restLocal.rotation.y << ", " << b.restLocal.rotation.z << ", " << b.restLocal.rotation.w << "]"
                          << std::endl;
            }
        }
    }

    if (docMw.valid()) {
        auto scMw = scene::buildScene(docMw);
        std::cout << "\n=== MW (IW4) Viewhands Bones ===" << std::endl;
        for (std::size_t i = 0; i < scMw.skeleton.bones.size(); ++i) {
            const auto& b = scMw.skeleton.bones[i];
            std::string pName = (b.parent >= 0 && b.parent < (int)scMw.skeleton.bones.size()) 
                                ? scMw.skeleton.bones[b.parent].name : "ROOT";
            if (b.name.find("shoulder") != std::string::npos || b.name.find("clavicle") != std::string::npos ||
                b.name.find("elbow") != std::string::npos || b.name.find("wrist") != std::string::npos ||
                b.name.find("weapon") != std::string::npos || b.name.find("torso") != std::string::npos) {
                std::cout << "MW  [" << i << "] " << std::setw(20) << b.name 
                          << " (parent: " << std::setw(15) << pName << ")"
                          << " pos=[" << b.restLocal.position.x << ", " << b.restLocal.position.y << ", " << b.restLocal.position.z << "]"
                          << " rot=[" << b.restLocal.rotation.x << ", " << b.restLocal.rotation.y << ", " << b.restLocal.rotation.z << ", " << b.restLocal.rotation.w << "]"
                          << std::endl;
            }
        }
    }

    return 0;
}
