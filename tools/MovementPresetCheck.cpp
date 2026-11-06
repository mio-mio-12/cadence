#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(){
    auto source=std::make_unique<AppState>(),target=std::make_unique<AppState>();
    auto& a=*source;auto& b=*target;
    a.exoDoubleJumpEnabled=false;a.exoInfiniteJumps=true;a.exoSingleTapChain=true;
    a.exoSingleTapAirborne=true;
    a.playerClipHeightCm=137.25f;a.playerClipWidthCm=43.25f;a.playerClipDepthCm=67.75f;
    a.traversalSettings={65,115,17.25f,true,2.5f};
    a.experimentalExoWallBounce=true;a.exoWallBounceReachIw=31.125f;a.exoWallBounceSpeedIw=312.5f;
    a.actorBlockedMantleSmoothing=.2345678f;a.weaponTiming.yyReturnScale=2.125f;
    a.sniperTrailEnabled=false;a.sniperTrailLifetime=.87654f;a.sniperTrailWidth=3.125f;
    a.projectileTrailEnabled=true;a.projectileTrailSpeed=12345.25f;a.projectileTrailLength=112.5f;a.projectileTrailWidth=2.875f;
    a.smokeTrailEnabled=false;a.muzzleFlashDuration=.01234567f;a.smokeEmissionDuration=.8765f;
    a.movementAlgorithm=3;a.sourceAirAccelerate=123;a.actorJumpHeightIw=87;
    a.actorMantleReachIw=47;a.actorMantleInputBuffer=.2345678f;a.transitionDuration=.12345678f;
    a.weaponSwitchAlgorithm=4;a.actionBlendTime=.123f;a.sourceAutoJump=false;
    const auto folder=std::filesystem::path("diagnostics/v354/movement");
    CHECK(saveGameplayPreset(a,folder/"source.cadencegame"));
    CHECK(loadGameplayPreset(b,folder/"source.cadencegame"));
    CHECK(movementPresetExtras(a)==movementPresetExtras(b));
    CHECK(b.exoSingleTapAirborne);
    CHECK(b.playerClipHeightCm==a.playerClipHeightCm);
    CHECK(b.playerClipWidthCm==a.playerClipWidthCm&&b.playerClipDepthCm==a.playerClipDepthCm);
    const auto halfExtents=actorCollisionHalfExtents(b);
    CHECK(halfExtents.x==21.625f&&halfExtents.y==33.875f);
    CHECK(std::abs(actorCollisionHeight(b,scene::Stance::Stand)-137.25f)<.001f);
    CHECK(std::abs(playerCurrentCollisionHeight(b,scene::Stance::Crouch)-68.625f)<.001f);
    {
        auto state=std::make_unique<AppState>();auto& h=*state;
        h.playerClipHeightCm=140;h.playerClipWidthCm=30;h.playerClipDepthCm=90;
        h.botCollisionBaseHeights={220};h.botModelScale=2;
        gameplay::bot::Actor bot;bot.standingCollisionHeight=botVariantCollisionHeight(h,0);
        bot.collisionHalfExtents=actorCollisionHalfExtents(h);
        CHECK(bot.standingCollisionHeight==140&&actorCollisionHeight(h,scene::Stance::Stand)==140);
        CHECK(bot.collisionHalfExtents.x==15&&bot.collisionHalfExtents.y==45);
        const auto wall=[](bool alongX){scene::glb::Map map;
            const scene::Vec3 a=alongX?scene::Vec3{100,-500,-100}:scene::Vec3{-500,100,-100};
            const scene::Vec3 b=alongX?scene::Vec3{100,500,-100}:scene::Vec3{500,100,-100};
            const scene::Vec3 c=alongX?scene::Vec3{100,500,400}:scene::Vec3{500,100,400};
            const scene::Vec3 d=alongX?scene::Vec3{100,-500,400}:scene::Vec3{-500,100,400};
            for(auto p:{std::array{a,b,c},std::array{a,c,d}}){scene::glb::CollisionTriangle t;t.a=p[0];t.b=p[1];t.c=p[2];t.normal=scene::normalize(scene::cross(t.b-t.a,t.c-t.a));t.minimum=t.maximum=t.a;
                for(auto v:p){t.minimum={std::min(t.minimum.x,v.x),std::min(t.minimum.y,v.y),std::min(t.minimum.z,v.z)};t.maximum={std::max(t.maximum.x,v.x),std::max(t.maximum.y,v.y),std::max(t.maximum.z,v.z)};}t.blocking=true;t.walkable=false;map.collision.push_back(t);}
            map.buildCollisionIndex();return map;};
        auto xwall=wall(true),ywall=wall(false);
        const auto xhit=actorCollisionSweep(h,xwall,{0,0,0},{200,0,0},scene::course::kPlayerRadius,140);
        const auto yhit=actorCollisionSweep(h,ywall,{0,0,0},{0,200,0},scene::course::kPlayerRadius,140);
        CHECK(xhit.fraction>.40f&&xhit.fraction<.45f);CHECK(yhit.fraction>.25f&&yhit.fraction<.30f);
        const auto xp=constrainCollisionMap(h,xwall,{0,0,0},{200,0,0},scene::course::kPlayerRadius,140,0);
        const auto yp=constrainCollisionMap(h,ywall,{0,0,0},{0,200,0},scene::course::kPlayerRadius,140,0);
        const auto xb=gameplay::bot::constrainActorMove(xwall,bot,{0,0,0},{200,0,0},0);
        const auto yb=gameplay::bot::constrainActorMove(ywall,bot,{0,0,0},{0,200,0},0);
        CHECK(std::abs(xp.x-xb.x)<.001f&&std::abs(yp.y-yb.y)<.001f);
        CHECK(xp.x>80&&xp.x<90&&yp.y>50&&yp.y<60);
        h.playerClipWidthCm=90;h.playerClipDepthCm=30;
        const auto swapped=actorCollisionSweep(h,xwall,{0,0,0},{200,0,0},scene::course::kPlayerRadius,140);
        CHECK(swapped.fraction>.25f&&swapped.fraction<.30f);
        h.playerClipWidthCm=0;CHECK(actorCollisionHalfExtents(h,17).x==17&&actorCollisionHalfExtents(h,17).y==15);
    }
    {
        auto state=std::make_unique<AppState>();auto& h=*state;
        h.renderDebugView=10;h.actorMode=true;h.actorNoclip=false;h.actorPosition={10,20,30};
        h.actorRenderPosition={100,200,300};h.playerClipHeightCm=140;h.movementAlgorithm=4;
        h.gameplayStance=scene::Stance::Crouch;
        auto boxes=liveCollisionOverlayBoxes(h);CHECK(boxes.size()==1);
        CHECK(boxes[0].feet.x==10&&boxes[0].feet.y==20&&boxes[0].feet.z==30);
        CHECK(std::abs(boxes[0].height-70)<.001f&&boxes[0].radius==gameplay::iw::worldUnits(16));
        h.actorMantling=true;boxes=liveCollisionOverlayBoxes(h);CHECK(boxes[0].height==140&&boxes[0].radius==scene::course::kPlayerRadius);h.actorMantling=false;
        h.movementAlgorithm=5;boxes=liveCollisionOverlayBoxes(h);CHECK(std::abs(boxes[0].height-100)<.001f&&boxes[0].radius==scene::course::kPlayerRadius);
        h.botCollisionBaseHeights={180};h.botModelScale=1.5f;h.bots.emplace_back();auto& bot=h.bots.back();
        bot.position={40,50,60};bot.alive=true;bot.stance=scene::Stance::Crouch;bot.standingCollisionHeight=botVariantCollisionHeight(h,0);
        boxes=liveCollisionOverlayBoxes(h);CHECK(boxes.size()==2&&boxes[1].feet.x==40);
        CHECK(std::abs(boxes[1].height-140.f*40.f/72.f)<.001f);
        CHECK(boxes[1].radius==scene::course::kPlayerRadius); // bot size never changes physics radius
        bot.alive=false;bot.grounded=true;CHECK(liveCollisionOverlayBoxes(h).size()==1);
        bot.grounded=false;CHECK(liveCollisionOverlayBoxes(h).size()==2);
        h.actorNoclip=true;CHECK(liveCollisionOverlayBoxes(h).size()==1);h.actorNoclip=false;
        h.renderDebugView=0;CHECK(liveCollisionOverlayBoxes(h).empty());h.renderDebugView=10;
        h.takePreview=true;CHECK(liveCollisionOverlayBoxes(h).empty());h.takePreview=false;
        CHECK(liveCollisionOverlayBoxes(h,true).empty());
        h.workspaceMode=3;CHECK(liveCollisionOverlayBoxes(h).empty());
    }
    CHECK(a.traversalSettings==b.traversalSettings);
    CHECK(a.movementAlgorithm==b.movementAlgorithm&&a.sourceAirAccelerate==b.sourceAirAccelerate);
    CHECK(a.actorMantleInputBuffer==b.actorMantleInputBuffer&&a.transitionDuration==b.transitionDuration);
    CHECK(b.weaponProfile.stats.yyReturnScale==a.weaponTiming.yyReturnScale);
    CHECK(saveGameplayPreset(b,folder/"roundtrip.cadencegame"));
    const auto read=[](auto p){std::ifstream in(p);return std::string(std::istreambuf_iterator<char>(in),{});};
    const auto text=read(folder/"source.cadencegame");CHECK(text==read(folder/"roundtrip.cadencegame"));
    auto legacy=text.substr(0,text.find_last_of('\n',text.size()-2)+1);legacy.replace(0,14,"CADENCEGAME 13");
    {std::ofstream out(folder/"v13.cadencegame");out<<legacy;}
    CHECK(loadGameplayPreset(b,folder/"v13.cadencegame"));CHECK(b.playerClipWidthCm==0&&b.playerClipDepthCm==0);CHECK(b.playerClipHeightCm==a.playerClipHeightCm);
    legacy=legacy.substr(0,legacy.find_last_of('\n',legacy.size()-2)+1);legacy.replace(0,14,"CADENCEGAME 12");
    {std::ofstream out(folder/"v12.cadencegame");out<<legacy;}
    CHECK(loadGameplayPreset(b,folder/"v12.cadencegame"));CHECK(b.playerClipHeightCm==0);CHECK(a.traversalSettings==b.traversalSettings);
    legacy=legacy.substr(0,legacy.find_last_of('\n',legacy.size()-2)+1);legacy.replace(0,14,"CADENCEGAME 11");
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
    const float retained=b.playerClipHeightCm,retainedWidth=b.playerClipWidthCm,retainedDepth=b.playerClipDepthCm;
    {std::ofstream out(folder/"invalid-height.cadencegame");out<<text.substr(0,text.find_last_of('\n',text.size()-2)+1)<<"-1 50\n";}
    CHECK(!loadGameplayPreset(b,folder/"invalid-height.cadencegame"));CHECK(b.playerClipHeightCm==retained);CHECK(b.playerClipWidthCm==retainedWidth&&b.playerClipDepthCm==retainedDepth);
    {std::ofstream out(folder/"invalid-depth.cadencegame");out<<text.substr(0,text.find_last_of('\n',text.size()-2)+1)<<"45 -1\n";}
    CHECK(!loadGameplayPreset(b,folder/"invalid-depth.cadencegame"));CHECK(b.playerClipDepthCm==retainedDepth&&b.playerClipWidthCm==retainedWidth);
    std::cout<<"PASS movement v14 exact round trip, v9/v10/v11/v12/v13 compatibility, transactional truncated-file and invalid-dimension rejection\n";
}
