#include "app/WorkspaceMode.h"
int main(){
    using namespace cadence::workspace;
    static_assert(runsSceneRuntime(0)&&runsSceneRuntime(1));
    static_assert(!runsSceneRuntime(2)&&!runsSceneRuntime(3)&&!runsSceneRuntime(-1)&&!runsSceneRuntime(4));
    static_assert(!acceptsGameplayInput(static_cast<int>(Mode::AnimationSets)));
    static_assert(canEnterAnimationSets(false,false,false));
    static_assert(!canEnterAnimationSets(true,false,false)&&!canEnterAnimationSets(false,true,false)&&!canEnterAnimationSets(false,false,true));
    return 0;
}
