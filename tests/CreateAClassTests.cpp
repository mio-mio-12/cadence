#include "scene/CastScene.h"
#include "weapon/WeaponProfile.h"
#include "assets/AssetCatalog.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

int main() {
    int failures = 0;
    const auto expect = [&](bool value, const char* message) {
        if (!value) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
            return false;
        }
        return true;
    };

    const auto startTime = std::chrono::steady_clock::now();

    // 1. Setup mock asset catalog containing requested models
    assets::Catalog catalog;
    assets::Asset hands;
    hands.name = "c_mul_mp_pmc_shortsleeve_viewhands_LOD0";
    hands.game = "bo2";
    hands.role = assets::Role::ViewHands;
    hands.compatibilityKeys = {"bo2", "viewhands", "shortsleeve"};
    catalog.entries.push_back(hands);

    assets::Asset mors;
    mors.name = "vm_mors_base_lod0";
    mors.game = "aw";
    mors.role = assets::Role::ViewWeapon;
    mors.compatibilityKeys = {"aw", "mors", "sniper", "view"};
    catalog.entries.push_back(mors);

    assets::Asset an94;
    an94.name = "t6_wpn_ar_an94_view_lod0";
    an94.game = "bo2";
    an94.role = assets::Role::ViewWeapon;
    an94.compatibilityKeys = {"bo2", "an94", "ar", "view"};
    catalog.entries.push_back(an94);

    assets::Asset sealBody;
    sealBody.name = "c_usa_mp_seal6_assault_fb_LOD0";
    sealBody.game = "bo2";
    sealBody.role = assets::Role::PlayerModel;
    sealBody.compatibilityKeys = {"bo2", "seal6", "assault"};
    catalog.entries.push_back(sealBody);

    // 2. Setup Playermodel Animation Library covering standard directions
    scene::CastScene playerActorScene;
    
    // T6 Locomotion clips for Rifle / Any
    const std::pair<const char*, const char*> clipDefinitions[] = {
        // Rifle locomotion
        {"pb_stand_alert_rifle", "bo2"},
        {"pb_combatrun_forward_rifle", "bo2"},
        {"pb_combatrun_left_rifle", "bo2"},
        {"pb_combatrun_right_rifle", "bo2"},
        {"pb_combatrun_back_rifle", "bo2"},
        {"pb_combatwalk_forward_rifle", "bo2"},
        {"pb_combatwalk_left_rifle", "bo2"},
        {"pb_combatwalk_right_rifle", "bo2"},
        {"pb_combatwalk_back_rifle", "bo2"},
        
        // Generic long gun / sniper compatible locomotion
        {"pb_stand_alert", "bo2"},
        {"pb_combatrun_forward", "bo2"},
        {"pb_combatrun_left", "bo2"},
        {"pb_combatrun_right", "bo2"},
        {"pb_combatrun_back", "bo2"},
        
        // Excluded noisy / zombie animations to ensure resolver filters them
        {"mp_zombie_hit", "aw"},
        {"mp_zombie_walk", "aw"},
        {"dog_run_forward", "iw6"},
        {"pb_flinch_heavy", "bo2"}
    };

    for (const auto& [name, game] : clipDefinitions) {
        scene::Animation clip;
        scene::classifyAnimationName(name, clip);
        clip.sourceName = name;
        clip.sourceGame = game;
        scene::Track dummyTrack;
        dummyTrack.boneIndex = 0;
        dummyTrack.property = scene::TrackProperty::TranslationX;
        dummyTrack.frames = {0, 1};
        dummyTrack.scalarValues = {0.0f, 1.0f};
        clip.tracks.push_back(dummyTrack);
        playerActorScene.animations.push_back(clip);
    }

    // 3. Verify Archetype and Classification for MORS (Primary) & AN94 (Secondary)
    const auto morsArchetype = weapon::inferArchetype(mors.name);
    expect(morsArchetype == weapon::Archetype::BoltSniper, "MORS inferred archetype must be BoltSniper");
    const auto morsClass = (morsArchetype == weapon::Archetype::BoltSniper || morsArchetype == weapon::Archetype::SemiSniper) ? scene::WeaponClass::Sniper : scene::WeaponClass::Rifle;
    expect(morsClass == scene::WeaponClass::Sniper, "MORS gameplay weapon class must be Sniper");

    const auto an94Archetype = weapon::inferArchetype(an94.name);
    expect(an94Archetype == weapon::Archetype::Rifle, "AN94 inferred archetype must be Rifle");
    const auto an94Class = scene::WeaponClass::Rifle;

    // 4. Test Playermodel Locomotion Animation Resolution for Primary Weapon (MORS)
    {
        // 4a. Idle
        scene::AnimationQuery idleQuery;
        idleQuery.domain = scene::AnimationDomain::PlayerBody;
        idleQuery.action = scene::ActionRole::None;
        idleQuery.motion = scene::MotionRole::Idle;
        idleQuery.weapon = morsClass;
        idleQuery.stance = scene::Stance::Stand;
        idleQuery.direction = scene::Direction::Any;
        idleQuery.forceT6Locomotion = true;
        auto idleMatch = scene::findBestAnimation(playerActorScene, idleQuery);
        expect(idleMatch.has_value(), "Primary (MORS) must resolve an Idle animation clip");
        if (idleMatch) {
            const auto& clip = playerActorScene.animations[*idleMatch];
            expect(clip.action == scene::ActionRole::None, "Primary Idle must not have active Action (e.g. Flinch/Hit)");
            expect(clip.sourceName.find("zombie") == std::string::npos, "Primary Idle must never select a zombie clip");
            expect(clip.sourceName.find("alert") != std::string::npos || clip.sourceName.find("idle") != std::string::npos, "Primary Idle must match alert/idle clip");
        }

        // 4b. Forward
        scene::AnimationQuery fwdQuery;
        fwdQuery.domain = scene::AnimationDomain::PlayerBody;
        fwdQuery.action = scene::ActionRole::None;
        fwdQuery.motion = scene::MotionRole::Run;
        fwdQuery.weapon = morsClass;
        fwdQuery.stance = scene::Stance::Stand;
        fwdQuery.direction = scene::Direction::Forward;
        fwdQuery.forceT6Locomotion = true;
        auto fwdMatch = scene::findBestAnimation(playerActorScene, fwdQuery);
        expect(fwdMatch.has_value(), "Primary (MORS) must resolve a Forward locomotion clip");
        if (fwdMatch) {
            const auto& clip = playerActorScene.animations[*fwdMatch];
            expect(clip.direction == scene::Direction::Forward, "Primary Forward clip direction mismatch");
            expect(clip.sourceName.find("zombie") == std::string::npos, "Primary Forward must not be zombie");
        }

        // 4c. Left
        scene::AnimationQuery leftQuery = fwdQuery;
        leftQuery.direction = scene::Direction::Left;
        auto leftMatch = scene::findBestAnimation(playerActorScene, leftQuery);
        expect(leftMatch.has_value(), "Primary (MORS) must resolve a Left locomotion clip");
        if (leftMatch) {
            const auto& clip = playerActorScene.animations[*leftMatch];
            expect(clip.direction == scene::Direction::Left, "Primary Left clip direction mismatch");
        }

        // 4d. Right
        scene::AnimationQuery rightQuery = fwdQuery;
        rightQuery.direction = scene::Direction::Right;
        auto rightMatch = scene::findBestAnimation(playerActorScene, rightQuery);
        expect(rightMatch.has_value(), "Primary (MORS) must resolve a Right locomotion clip");
        if (rightMatch) {
            const auto& clip = playerActorScene.animations[*rightMatch];
            expect(clip.direction == scene::Direction::Right, "Primary Right clip direction mismatch");
        }

        // 4e. Back
        scene::AnimationQuery backQuery = fwdQuery;
        backQuery.direction = scene::Direction::Backward;
        auto backMatch = scene::findBestAnimation(playerActorScene, backQuery);
        expect(backMatch.has_value(), "Primary (MORS) must resolve a Back locomotion clip");
        if (backMatch) {
            const auto& clip = playerActorScene.animations[*backMatch];
            expect(clip.direction == scene::Direction::Backward, "Primary Back clip direction mismatch");
        }
    }

    // 5. Test Playermodel Locomotion Animation Resolution for Secondary Weapon (AN94)
    {
        // 5a. Idle
        scene::AnimationQuery idleQuery;
        idleQuery.domain = scene::AnimationDomain::PlayerBody;
        idleQuery.action = scene::ActionRole::None;
        idleQuery.motion = scene::MotionRole::Idle;
        idleQuery.weapon = an94Class;
        idleQuery.stance = scene::Stance::Stand;
        idleQuery.direction = scene::Direction::Any;
        idleQuery.forceT6Locomotion = true;
        auto idleMatch = scene::findBestAnimation(playerActorScene, idleQuery);
        expect(idleMatch.has_value(), "Secondary (AN94) must resolve an Idle animation clip");
        if (idleMatch) {
            const auto& clip = playerActorScene.animations[*idleMatch];
            expect(clip.action == scene::ActionRole::None, "Secondary Idle must not have active Action");
            expect(clip.sourceName.find("zombie") == std::string::npos, "Secondary Idle must never select a zombie clip");
            expect(clip.sourceName.find("alert") != std::string::npos || clip.sourceName.find("idle") != std::string::npos, "Secondary Idle must match alert/idle clip");
        }

        // 5b. Forward
        scene::AnimationQuery fwdQuery;
        fwdQuery.domain = scene::AnimationDomain::PlayerBody;
        fwdQuery.action = scene::ActionRole::None;
        fwdQuery.motion = scene::MotionRole::Run;
        fwdQuery.weapon = an94Class;
        fwdQuery.stance = scene::Stance::Stand;
        fwdQuery.direction = scene::Direction::Forward;
        fwdQuery.forceT6Locomotion = true;
        auto fwdMatch = scene::findBestAnimation(playerActorScene, fwdQuery);
        expect(fwdMatch.has_value(), "Secondary (AN94) must resolve a Forward locomotion clip");
        if (fwdMatch) {
            const auto& clip = playerActorScene.animations[*fwdMatch];
            expect(clip.direction == scene::Direction::Forward, "Secondary Forward clip direction mismatch");
            expect(clip.sourceName.find("zombie") == std::string::npos, "Secondary Forward must not be zombie");
        }

        // 5c. Left
        scene::AnimationQuery leftQuery = fwdQuery;
        leftQuery.direction = scene::Direction::Left;
        auto leftMatch = scene::findBestAnimation(playerActorScene, leftQuery);
        expect(leftMatch.has_value(), "Secondary (AN94) must resolve a Left locomotion clip");
        if (leftMatch) {
            const auto& clip = playerActorScene.animations[*leftMatch];
            expect(clip.direction == scene::Direction::Left, "Secondary Left clip direction mismatch");
        }

        // 5d. Right
        scene::AnimationQuery rightQuery = fwdQuery;
        rightQuery.direction = scene::Direction::Right;
        auto rightMatch = scene::findBestAnimation(playerActorScene, rightQuery);
        expect(rightMatch.has_value(), "Secondary (AN94) must resolve a Right locomotion clip");
        if (rightMatch) {
            const auto& clip = playerActorScene.animations[*rightMatch];
            expect(clip.direction == scene::Direction::Right, "Secondary Right clip direction mismatch");
        }

        // 5e. Back
        scene::AnimationQuery backQuery = fwdQuery;
        backQuery.direction = scene::Direction::Backward;
        auto backMatch = scene::findBestAnimation(playerActorScene, backQuery);
        expect(backMatch.has_value(), "Secondary (AN94) must resolve a Back locomotion clip");
        if (backMatch) {
            const auto& clip = playerActorScene.animations[*backMatch];
            expect(clip.direction == scene::Direction::Backward, "Secondary Back clip direction mismatch");
        }
    }

    const auto endTime = std::chrono::steady_clock::now();
    const double elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();

    expect(elapsedSeconds < 20.0, "Class loading and animation resolution took >20.0 seconds");
    std::cout << "Create-a-Class test completed in " << elapsedSeconds << "s (< 20s budget).\n";

    if (failures == 0) {
        std::cout << "All Create-a-Class tests passed successfully.\n";
    }
    return failures ? 1 : 0;
}
