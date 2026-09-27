// Shared ordering for serialization, staging and round-trip tests.
template<class T> auto movementPresetExtras(T& app){
    return std::tie(app.actorBlockedMantleSmoothing,app.exoDoubleJumpEnabled,
        app.exoInfiniteJumps,app.exoSingleTapChain,app.experimentalExoWallBounce,
        app.exoWallBounceReachIw,app.exoWallBounceSpeedIw,
        app.sniperTrailEnabled,app.sniperTrailLifetime,app.sniperTrailWidth,
        app.projectileTrailEnabled,app.projectileTrailSpeed,app.projectileTrailLength,
        app.projectileTrailWidth,app.smokeTrailEnabled,app.muzzleFlashDuration,
        app.smokeEmissionDuration,app.weaponTiming.yyReturnScale);
}
