void steerSpBotOnMap(AppState& app,gameplay::bot::Actor& bot,float delta,float yawBeforePolicy){
    using namespace gameplay::bot;
    if(!app.loadedMap||bot.mantling)return;
    if(app.botSpRoutes.size()!=app.bots.size())app.botSpRoutes.resize(app.bots.size());
    auto& route=app.botSpRoutes[static_cast<std::size_t>(&bot-app.bots.data())];
    route.crowdTime=std::max(0.f,route.crowdTime-delta);
    if(route.crowdTime<=0||(route.crowdWaypoint&&horizontalDistance(bot.position,*route.crowdWaypoint)<30.f))route.crowdWaypoint.reset();
    if(route.crowdWaypoint)route.noProgressTime=0;
    const auto botIndex=static_cast<std::size_t>(&bot-app.bots.data());
    const bool coverSeeking=botIndex<app.botSpContextStates.size()&&app.botSpContextStates[botIndex].coverSeek>0;
    const bool workTurn=static_cast<std::size_t>(&bot-app.bots.data())==app.botRouteWorkTurn;
    std::uint64_t probeRevision=app.loadedMap->collisionRevision;
    const auto addRevision=[&](float v){probeRevision=(probeRevision*1099511628211ULL)^std::bit_cast<std::uint32_t>(v);};
    for(const auto& block:activeBotNavigation(app).blocks)for(float v:{block.minimum.x,block.minimum.y,block.minimum.z,block.maximum.x,block.maximum.y,block.maximum.z})addRevision(v);
    addRevision(float(activeBotNavigation(app).boundary.enabled));addRevision(activeBotNavigation(app).boundary.floorZ);addRevision(activeBotNavigation(app).boundary.ceilingZ);
    for(const auto& p:activeBotNavigation(app).boundary.points){addRevision(p.x);addRevision(p.y);}
    app.botRouteProbeCache.setRevision(probeRevision);
    bot.spMantleProbeTime=std::max(0.f,bot.spMantleProbeTime-delta);
    bot.spCrowdProbeTime=std::max(0.f,bot.spCrowdProbeTime-delta);
    const bool crowdCheck=workTurn&&bot.spCrowdProbeTime<=0;
    if(crowdCheck)bot.spCrowdProbeTime=.2f;
    const auto walkProbe=[&](scene::Vec3 from,scene::Vec3 desired)->std::optional<scene::Vec3>{
        constexpr float absent=-1.e20f;
        const float length=scene::length(desired-from);if(length>6000)return std::nullopt;
        const int count=std::max(1,static_cast<int>(std::ceil(length/40.f)));
        const auto origin=from;
        for(int i=1;i<=count;++i){
            auto next=scene::lerp(origin,desired,static_cast<float>(i)/count);
            next.z=app.loadedMap->navigationGroundHeight(next.x,next.y,from.z+scene::course::kStepHeight+1-45.f,absent,scene::course::kPlayerRadius*.55f);
            const float rise=next.z-from.z;
            if(next.z<absent*.5f||rise>scene::course::kStepHeight+.1f||rise<-gameplay::iw::worldUnits(128)||navigationBlocked(activeBotNavigation(app),next))return std::nullopt;
            const auto& boundary=activeBotNavigation(app).boundary;
            if(boundary.enabled&&(next.z<boundary.floorZ||next.z>boundary.ceilingZ||(!boundary.points.empty()&&!pointInPolygon({next.x,next.y},boundary.points))))return std::nullopt;
            if(rise<-scene::course::kStepHeight){
                // Clear the sill with the full body before descending. Testing
                // a vertical drop at the first footprint sample clips its rim.
                auto landing=desired;
                landing.z=app.loadedMap->navigationGroundHeight(landing.x,landing.y,from.z+.1f-45.f,absent,scene::course::kPlayerRadius*.55f);
                if(landing.z<from.z-gameplay::iw::worldUnits(128)||landing.z>from.z||horizontalDistance(from,landing)>180.f||navigationBlocked(activeBotNavigation(app),landing))return std::nullopt;
                if(boundary.enabled&&(landing.z<boundary.floorZ||landing.z>boundary.ceilingZ||(!boundary.points.empty()&&!pointInPolygon({landing.x,landing.y},boundary.points))))return std::nullopt;
                const scene::Vec3 over{landing.x,landing.y,from.z};
                const auto across=gameplay::mantle::sweepRounded(*app.loadedMap,from,over,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72));
                const auto down=gameplay::mantle::sweepRounded(*app.loadedMap,over,landing,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72));
                if(across.startSolid||across.fraction<.9999f||down.startSolid||down.fraction<.9999f)return std::nullopt;
                return landing;
            }
            // A level segment can be validated by one continuous full-body
            // sweep. Avoid the legacy step solver inventing a raised landing
            // while merely moving away from an existing wall contact.
            const bool authoredCrouch=app.loadedMap->gameplay.crouch(from,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72))||app.loadedMap->gameplay.crouch(next,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72));
            const float routeHeight=gameplay::iw::worldUnits(authoredCrouch?40:72);
            if(std::abs(rise)<.1f){
                const auto clear=gameplay::mantle::sweepRounded(*app.loadedMap,from+scene::Vec3{0,0,.04f},next+scene::Vec3{0,0,.04f},scene::course::kPlayerRadius,routeHeight);
                if(!clear.startSolid&&clear.fraction>=.9999f){from=next;continue;}
            }
            // Support was checked above. Do not run navigationSegmentClear's
            // second, looser ground-height query: an overlaid lip can mask the
            // supported lower floor there. Physics' capsule solver below owns
            // blockers, body clearance and permitted steps.
            const auto constrained=app.loadedMap->constrainMove(from,next,scene::course::kPlayerRadius,routeHeight,scene::course::kStepHeight/1.25f);
            if(horizontalDistance(constrained,next)>1.f||constrained.z>from.z+scene::course::kStepHeight+.1f)return std::nullopt;
            from=next;
        }
        return from;
    };
    const auto probe=[&](scene::Vec3 from,scene::Vec3 desired)->std::optional<scene::Vec3>{
        if(auto walked=walkProbe(from,desired))return walked;
        if(horizontalDistance(from,desired)<8.f||horizontalDistance(from,desired)>180.f)return {};
        const float top=app.loadedMap->groundHeight(desired.x,desired.y,from.z+gameplay::iw::worldUnits(57)-45.f,-1e9f);
        if(top<from.z+scene::course::kStepHeight||top>from.z+gameplay::iw::worldUnits(57))return {};
        const auto direction=scene::normalize(scene::Vec3{desired.x-from.x,desired.y-from.y,0});
        auto climb=gameplay::mantle::findClimbTo(*app.loadedMap,from,{desired.x,desired.y,top},scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight,gameplay::iw::worldUnits(57),gameplay::iw::worldUnits(42),gameplay::iw::worldUnits(24));
        if(!climb)climb=gameplay::authored::climb(*app.loadedMap,from,direction,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight,gameplay::iw::worldUnits(57),gameplay::iw::worldUnits(42),gameplay::iw::worldUnits(24),1,false);
        if(!climb)climb=gameplay::mantle::findClimb(*app.loadedMap,from,direction,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight,gameplay::iw::worldUnits(57),gameplay::iw::worldUnits(42),gameplay::iw::worldUnits(24));
        if(climb){
            const auto& boundary=activeBotNavigation(app).boundary;
            const bool inside=!boundary.enabled||(climb->target.z>=boundary.floorZ&&climb->target.z<=boundary.ceilingZ&&(boundary.points.empty()||pointInPolygon({climb->target.x,climb->target.y},boundary.points)));
            if(inside&&!navigationBlocked(activeBotNavigation(app),climb->target)){
                // The swept mantle target includes footprint clearance. The
                // body settles onto actual support after playback; continue
                // graph search at that grounded height, not floating above a
                // rim where an otherwise impossible exit appears clear.
                auto landing=climb->target;
                landing.z=app.loadedMap->navigationGroundHeight(landing.x,landing.y,landing.z+.1f-45.f,-1e9f,scene::course::kPlayerRadius*.55f);
                if(std::abs(landing.z-climb->target.z)<=scene::course::kStepHeight)return landing;
            }
        }
        return {};
    };
    // Move out of an occupied firing spot instead of making every following
    // teammate wait. This is a local collision-checked choice, not a shove.
    if(crowdCheck&&!coverSeeking&&!bot.spWantsMove&&!bot.spReactionWindow&&!bot.spMovingScenario){
        for(const auto& other:app.bots)if(&other!=&bot&&other.alive&&horizontalDistance(bot.position,other.position)<140.f){
            auto away=bot.position-other.position;away.z=0;
            if(scene::length(away)<1.f)away={0,bot.id%2?1.f:-1.f,0};
            const auto candidate=bot.position+scene::normalize(away)*190.f;
            if(walkProbe(bot.position,candidate)){bot.spMoveGoal=candidate;bot.spWantsMove=true;bot.spCombatMoving=bot.targetVisible;bot.spMoveThrottle=std::max(.6f,bot.spMoveThrottle);++bot.spDecisionSequence;break;}
        }
    }
    if(!bot.spWantsMove)return;
    bot.yaw=yawBeforePolicy;bot.input.forward=bot.input.right=0;bot.input.sprint=false;
    auto goal=bot.spMoveGoal;
    if(workTurn&&!coverSeeking&&bot.spProjectedGoalSequence!=bot.spDecisionSequence){
        bot.spProjectedGoalSequence=bot.spDecisionSequence;
        for(int i=0;i<25;++i){const float angle=(i-1)*scene::kPi/4;const float radius=i<=8?150.f:i<=16?400.f:800.f;auto candidate=goal+(i?scene::Vec3{std::cos(angle),std::sin(angle),0}*radius:scene::Vec3{});
            candidate.z=app.loadedMap->groundHeight(candidate.x,candidate.y,goal.z+scene::course::kStepHeight-45,-1e9f);
            if(candidate.z<-1e8f)candidate.z=app.loadedMap->groundHeight(candidate.x,candidate.y,goal.z+300-45,-1e9f);
            if(candidate.z<-1e8f||navigationBlocked(activeBotNavigation(app),candidate))continue;
            const auto& boundary=activeBotNavigation(app).boundary;
            if(boundary.enabled&&(candidate.z<boundary.floorZ||candidate.z>boundary.ceilingZ||(!boundary.points.empty()&&!pointInPolygon({candidate.x,candidate.y},boundary.points))))continue;
            const float support=app.loadedMap->navigationGroundHeight(candidate.x,candidate.y,candidate.z+scene::course::kStepHeight+1,-1e9f,scene::course::kPlayerRadius*.55f);
            if(std::abs(support-candidate.z)>20.f)continue;
            if(gameplay::mantle::sweepRounded(*app.loadedMap,candidate,candidate,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72)).startSolid)continue;
            if(scene::length(candidate-goal)>1.f)route.reset();
            goal=bot.spMoveGoal=candidate;break;
        }
    }
    // Respect usable authored guidance first; automatic routing fills gaps.
    if(!coverSeeking&&bot.navigation&&!bot.navigation->nodes.empty()){
        const auto start=nearestNavigationNode(*bot.navigation,bot.position),end=nearestNavigationNode(*bot.navigation,goal);
        if(start&&end&&horizontalDistance(bot.navigation->nodes[*end].position,goal)<400){
            if(bot.repathTime<=0||bot.navigationRoute.empty()){buildNavigationRoute(bot,*bot.navigation,*start,*end);bot.repathTime=1;}
            if(bot.navigationRouteCursor<bot.navigationRoute.size()){
                auto index=bot.navigationRoute[bot.navigationRouteCursor];
                if(horizontalDistance(bot.position,bot.navigation->nodes[index].position)<55&&bot.navigationRouteCursor+1<bot.navigationRoute.size())index=bot.navigationRoute[++bot.navigationRouteCursor];
                if(horizontalDistance(bot.position,bot.navigation->nodes[index].position)>55)goal=bot.navigation->nodes[index].position;
            }
        }
    }
    // Climb execution below belongs to a completed route's next waypoint.
    // A merely nearby sill is not evidence of a usable exit toward the goal.
    // A new committed intention must not wait behind an obsolete search (for
    // example a blocked lateral combat point after the player rounds a wall).
    if(route.intentTag!=bot.spDecisionSequence&&scene::length(goal-route.goal)>50)route.reset();
    route.intentTag=bot.spDecisionSequence;
    if(workTurn&&route.status==MapRouteStatus::Idle){
        route.preferredSide=bot.id%2?1:-1;route.avoidedCount=0;
        for(std::size_t peer=0;peer<app.bots.size()&&route.avoidedCount<route.avoided.size();++peer)if(peer!=botIndex&&app.bots[peer].alive){
            route.avoided[route.avoidedCount++]=app.bots[peer].position;
            if(peer<app.botSpRoutes.size()){const auto& other=app.botSpRoutes[peer];for(std::size_t k=other.cursor;k<other.path.size()&&k<other.cursor+10&&route.avoidedCount<route.avoided.size();k+=3)route.avoided[route.avoidedCount++]=other.path[k];}
        }
    }
    // Fair cooperative navigation: only one bot spends collision-query work
    // this frame. All bots still advance steering, physics and poses each frame.
    // Endpoint validation is owned by the resumable search, not repeated here.
    const MapRouteBudget budget{workTurn?256u:0u,std::chrono::steady_clock::now()+std::chrono::microseconds(1200),500.f,true};
    const auto cachedProbe=[&](scene::Vec3 from,scene::Vec3 to){return app.botRouteProbeCache.query(from,to,probe);};
    if(workTurn&&route.status!=MapRouteStatus::Following){
        unsigned attempts=0;
        for(const auto& peer:app.botSpRoutes){
            if(std::chrono::steady_clock::now()>=budget.deadline||attempts>=4)break;
            const auto joinProbe=[&](scene::Vec3 from,scene::Vec3 to){++attempts;return cachedProbe(from,to);};
            if(joinMapRoute(route,bot.position,goal,peer,joinProbe))break;
        }
    }
    const auto result=updateMapRoute(route,bot.position,goal,delta,cachedProbe,128,budget);
    if(result.waypoint){
        steerSpMovement(bot,*result.waypoint,delta);
        const auto to=*result.waypoint-bot.position;
        if(result.status==MapRouteStatus::Following&&workTurn&&bot.spMantleProbeTime<=0&&bot.grounded&&bot.reloadTime<=0&&!bot.spMovingScenario&&to.z>scene::course::kStepHeight&&horizontalDistance(bot.position,*result.waypoint)<180.f){
            bot.spMantleProbeTime=.2f;const auto direction=scene::normalize(scene::Vec3{to.x,to.y,0});
            if(scene::dot(direction,scene::Vec3{std::cos(bot.yaw),std::sin(bot.yaw),0})>.85f)
            if(auto climb=gameplay::mantle::findClimbTo(*app.loadedMap,bot.position,*result.waypoint,scene::course::kPlayerRadius,gameplay::iw::worldUnits(72),scene::course::kStepHeight,gameplay::iw::worldUnits(57),gameplay::iw::worldUnits(42),gameplay::iw::worldUnits(24))){
                bot.mantling=true;bot.mantleStart=bot.position;bot.mantleEnd=climb->target;bot.mantleClearanceZ=climb->clearanceZ;bot.mantleDuration=climb->duration;bot.mantleElapsed=0;bot.velocity={};bot.input={};return;
            }
        }
        // Yield to a bot already occupying the next few body widths. A stable
        // ID priority prevents two approaching bots from stopping each other.
        const scene::Vec3 forward=scene::normalize(scene::Vec3{result.waypoint->x-bot.position.x,result.waypoint->y-bot.position.y,0});
        if(route.crowdWaypoint){steerSpMovement(bot,*route.crowdWaypoint,delta,true);bot.input.sprint=false;}
        else {
        for(const auto& other:app.bots)if(&other!=&bot&&other.alive&&std::abs(other.position.z-bot.position.z)<70){
            const auto separation=other.position-bot.position;const float ahead=scene::dot(separation,forward);
            const float lateral=std::abs(separation.x*forward.y-separation.y*forward.x);
            if(ahead>0&&ahead<140&&lateral<scene::course::kPlayerRadius*2&&horizontalDistance(other.position,bot.position)<110&&
               (other.id<bot.id||!other.spWantsMove)){
                bool bypass=false;
                for(int attempt=0;attempt<4&&!bypass;++attempt){
                    const float side=((attempt&1)?-1.f:1.f)*(bot.id%2?1.f:-1.f);
                    const auto candidate=bot.position+forward*(attempt<2?100.f:-100.f)+scene::Vec3{-forward.y,forward.x,0}*(side*140.f);
                    bool occupied=false;for(const auto& peer:app.bots)if(&peer!=&bot&&peer.alive&&horizontalDistance(peer.position,candidate)<95.f)occupied=true;
                    if(crowdCheck&&!occupied)if(auto clear=walkProbe(bot.position,candidate)){route.crowdWaypoint=*clear;route.crowdTime=.8f;steerSpMovement(bot,*clear,delta,true);bypass=true;break;}
                }
                if(!bypass){const float space=std::clamp((ahead-55.f)/65.f,0.f,1.f);bot.input.forward*=space;bot.input.right*=space;}
                bot.input.sprint=false;break;
            }
        }
        }
    }else if(result.status==MapRouteStatus::Unreachable){
        // Retain a ping through temporary route failures. The route's bounded
        // retry timer prevents per-frame searches; a failed attempt is not arrival.
        if(app.botAreaPing.contains(bot.id))return;
        const auto index=static_cast<std::size_t>(&bot-app.bots.data());
        if(index<app.botSpContextStates.size())app.botSpContextStates[index].coverSeek=0;
        // Deliberate scan/hold instead of repeatedly reversing into a bad goal.
        bot.spPhase=bot.targetVisible?SpPhase::Reposition:SpPhase::Patrol;bot.spPhaseTime=0.f;
        bot.spWantsMove=false;bot.spIdleWindow=false;bot.spCombatMoving=false;
        // The pause itself supplies retry backoff. Retaining a retry timer here
        // would freeze it while paused and reject every later intention.
        route.reset();
    }
}
