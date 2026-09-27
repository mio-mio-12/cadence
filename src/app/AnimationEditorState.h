#pragma once
#include "AnimationSet.h"
#include <array>
#include <string>
#include <limits>

namespace cadence {
struct AnimationEditorState {
    AnimationSet draft, saved;
    bool initialized{}, dirty{}, loop{}, matchSource{true}, pendingLoad{}, showContents{};
    bool useOnBots{}, pendingBots{}, confirmDiscard{}, confirmDelete{}, saveAs{}, confirmOverwrite{};
    bool rootMotion{};
    std::string preset, sourceGame, modelGame, pendingPreset, status;
    std::array<char,96> name{};
    std::array<char,160> search{};
    std::size_t model{std::numeric_limits<std::size_t>::max()};
    int action{static_cast<int>(scene::ActionRole::Death)};
    int motionFilter{-1};
    AnimationSet::Slot assignment{0,1,1,0,0,-1};
    int pendingOperation{};
    float speed{1.f}, pitch{-.15f}, yaw{scene::kPi}, zoom{1.f};
    scene::Vec3 pan{};
};
}
