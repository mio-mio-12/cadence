#pragma once
namespace cadence::workspace {
enum class Mode : int { Gameplay=0, AnimationViewer=1, Benchmark=2, AnimationSets=3 };
// Keep unknown/new workspaces isolated until their runtime ownership is explicit.
constexpr bool runsSceneRuntime(int mode){return mode==static_cast<int>(Mode::Gameplay)||mode==static_cast<int>(Mode::AnimationViewer);}
constexpr bool runtime(int mode){return runsSceneRuntime(mode);}
constexpr bool acceptsGameplayInput(int mode){return runsSceneRuntime(mode);}
constexpr bool canEnterAnimationSets(bool recording,bool capturing,bool exporting){return !recording&&!capturing&&!exporting;}
}
