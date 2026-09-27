#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(){
    auto source=std::make_unique<AppState>(),target=std::make_unique<AppState>();
    auto& a=*source;auto& b=*target;
    a.exoDoubleJumpEnabled=false;a.exoInfiniteJumps=true;a.exoSingleTapChain=true;
    a.exoSingleTapAirborne=true;
    a.traversalSettings={65,115,17.25f,true,2.5f};
    a.experimentalExoWallBounce=true;a.exoWallBounceReachIw=31.125f;a.exoWallBounceSpeedIw=312.5f;
    a.actorBlockedMantleSmoothing=.2345678f;a.weaponTiming.yyReturnScale=2.125f;
    a.sniperTrailEnabled=false;a.sniperTrailLifetime=.87654f;a.sniperTrailWidth=3.125f;
    a.projectileTrailEnabled=true;a.projectileTrailSpeed=12345.25f;a.projectileTrailLength=112.5f;a.projectileTrailWidth=2.875f;
    a.smokeTrailEnabled=false;a.muzzleFlashDuration=.01234567f;a.smokeEmissionDuration=.8765f;
    a.movementAlgorithm=3;a.sourceAirAccelerate=123;a.actorJumpHeightIw=87;
    a.actorMantleReachIw=47;a.actorMantleInputBuffer=.2345678f;a.transitionDuration=.12345678f;
    a.weaponSwitchAlgorithm=4;a.actionBlendTime=.123f;a.sourceAutoJump=false;
    const auto folder=std::filesystem::path("diagnostics/v223/movement");
    CHECK(saveGameplayPreset(a,folder/"source.cadencegame"));
    CHECK(loadGameplayPreset(b,folder/"source.cadencegame"));
    CHECK(movementPresetExtras(a)==movementPresetExtras(b));
    CHECK(b.exoSingleTapAirborne);
    CHECK(a.traversalSettings==b.traversalSettings);
    CHECK(a.movementAlgorithm==b.movementAlgorithm&&a.sourceAirAccelerate==b.sourceAirAccelerate);
    CHECK(a.actorMantleInputBuffer==b.actorMantleInputBuffer&&a.transitionDuration==b.transitionDuration);
    CHECK(b.weaponProfile.stats.yyReturnScale==a.weaponTiming.yyReturnScale);
    CHECK(saveGameplayPreset(b,folder/"roundtrip.cadencegame"));
    const auto read=[](auto p){std::ifstream in(p);return std::string(std::istreambuf_iterator<char>(in),{});};
    const auto text=read(folder/"source.cadencegame");CHECK(text==read(folder/"roundtrip.cadencegame"));
    auto legacy=text.substr(0,text.find_last_of('\n',text.size()-2)+1);legacy.replace(0,14,"CADENCEGAME 11");
    {std::ofstream out(folder/"v11.cadencegame");out<<legacy;}
    CHECK(loadGameplayPreset(b,folder/"v11.cadencegame"));CHECK(b.traversalSettings==gameplay::traversal::Settings{});
    legacy=legacy.substr(0,legacy.find_last_of('\n',legacy.size()-2)+1);legacy.replace(0,14,"CADENCEGAME 10");
    {std::ofstream out(folder/"v10.cadencegame");out<<legacy;}
    CHECK(loadGameplayPreset(b,folder/"v10.cadencegame"));CHECK(b.exoSingleTapAirborne);
    legacy=legacy.substr(0,legacy.find_last_of('\n',legacy.size()-2)+1);legacy.replace(0,14,"CADENCEGAME 9");
    {std::ofstream out(folder/"legacy.cadencegame");out<<legacy;}
    CHECK(loadGameplayPreset(b,folder/"legacy.cadencegame"));
    CHECK(movementPresetExtras(a)==movementPresetExtras(b));
    {std::ofstream out(folder/"truncated.cadencegame");out<<text.substr(0,text.size()-12);}
    CHECK(!loadGameplayPreset(b,folder/"truncated.cadencegame"));
    CHECK(movementPresetExtras(a)==movementPresetExtras(b));
    std::cout<<"PASS movement v12 exact round trip, v9/v10/v11 compatibility, transactional truncated-file rejection\n";
}
