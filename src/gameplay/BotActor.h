#pragma once

#include "gameplay/IwPresentation.h"
#include "gameplay/AirborneAnimation.h"
#include "gameplay/BotSpVariety.h"
#include "scene/TestCourse.h"
#include "weapon/WeaponProfile.h"

#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <vector>
#include "scene/AuthoredGameplay.h"

namespace gameplay::bot {

struct Input {
    float forward{},right{};
    bool sprint{},jump{},ads{},fire{},reload{},melee{},equipment{};
};

enum class BehaviorState { Idle, Patrol, Chase, Flank, Engage, Dead };

struct BehaviorConfig {
    const std::vector<scene::authored::Destination>* mapDestinations{};
    bool spPlayerPolicy{false};
    bool spUseSceneVisibilityOnly{false};
    float spCombatMobilityChance{0.75f},spPatrolWalkChance{0.35f};
    float spSprintChance{0.70f},spJumpChance{0.30f},spIdlePauseChance{0.12f};
    float detectionRange{25.0f * iw::kMetersToUnits};
    float hearingRange{7.0f * iw::kMetersToUnits};
    float playerAttraction{0.18f};
    float tacticalReposition{0.72f};
    float preferredRange{8.0f * iw::kMetersToUnits};
    float aimResponse{7.0f};
    float fireInterval{0.105f};
    float reloadDuration{2.3f};
    int magazineSize{30};
    float reactionDelay{0.25f};
    float aimErrorDegrees{1.5f};
    bool allowEquipment{true};
    float equipmentChance{0.16f};
    float equipmentMinInterval{1.0f},equipmentMaxInterval{5.0f};
    bool allowSidequests{true},avoidDeadEnds{true};
    float sidequestChance{0.12f},sidequestMinDuration{2.5f},sidequestMaxDuration{7.0f};
    float nodeArrivalRadiusMultiplier{1.0f};
    float universalNodeArrivalRadius{0.0f};
};

struct NavigationNode { scene::Vec3 position{};float weight{1.0f},cost{1.0f},radius{iw::worldUnits(16.0f)};int lane{};bool priority{},disabled{};float attraction{},attractionRadius{iw::worldUnits(192.0f)};bool mantlePriority{},mantleOver{}; };
struct NavigationLink { std::size_t from{},to{};float cost{1.0f};bool bidirectional{true}; };
struct NavigationBlock { scene::Vec3 minimum{},maximum{}; };
struct PlayablePolygon {
    std::vector<scene::Vec2> points;
    float floorZ{-2000.0f};
    float ceilingZ{6000.0f};
    bool enabled{false};
};

inline bool pointInPolygon(scene::Vec2 p, const std::vector<scene::Vec2>& poly) {
    if (poly.size() < 3) return true;
    bool inside = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
            (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y + 1e-8f) + poly[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

struct NavigationGoalArea {
    scene::Vec3 position{};
    float radius{iw::worldUnits(350.0f)};
    float weight{1.0f};
    bool enabled{true};
    std::string name{"Goal Area"};
};

struct NavigationGraph {
    std::vector<NavigationNode> nodes;
    std::vector<NavigationLink> links;
    std::vector<NavigationBlock> blocks;
    PlayablePolygon boundary;
    std::vector<NavigationGoalArea> goalAreas;
};

inline bool navigationBlocked(const NavigationGraph& graph,scene::Vec3 point){for(const auto& block:graph.blocks)if(point.x>=block.minimum.x&&point.x<=block.maximum.x&&point.y>=block.minimum.y&&point.y<=block.maximum.y&&point.z>=block.minimum.z&&point.z<=block.maximum.z)return true;return false;}
inline std::optional<std::size_t> nearestNavigationNode(const NavigationGraph& graph,scene::Vec3 point){std::optional<std::size_t> best;float distance=std::numeric_limits<float>::max();for(std::size_t i=0;i<graph.nodes.size();++i){const auto& node=graph.nodes[i];if(node.disabled||navigationBlocked(graph,node.position))continue;const float candidate=scene::length(node.position-point);if(candidate<distance){distance=candidate;best=i;}}return best;}
inline float horizontalDistance(scene::Vec3 a,scene::Vec3 b){const float x=a.x-b.x,y=a.y-b.y;return std::sqrt(x*x+y*y);}
inline float personalityValue(std::uint32_t value){value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;return static_cast<float>(value&0xffffu)/65535.0f;}
inline std::size_t navigationDegree(const NavigationGraph& graph,std::size_t node){std::size_t degree{};for(const auto& link:graph.links)if(link.from==node||(link.bidirectional&&link.to==node))++degree;return degree;}
inline std::size_t nextNavigationNode(const NavigationGraph& graph,std::size_t current,std::uint32_t seed,std::size_t avoid=std::numeric_limits<std::size_t>::max()){
    std::size_t best=current;float bestScore=-1.0f;bool foundAlternative=false;
    const bool leaveDeadEnd=current<graph.nodes.size()&&navigationDegree(graph,current)<=1;
    bool hasThroughNeighbor=false;for(const auto& link:graph.links){std::optional<std::size_t> candidate;if(link.from==current)candidate=link.to;else if(link.bidirectional&&link.to==current)candidate=link.from;if(candidate&&*candidate<graph.nodes.size()&&!graph.nodes[*candidate].disabled&&!navigationBlocked(graph,graph.nodes[*candidate].position)&&navigationDegree(graph,*candidate)>1)hasThroughNeighbor=true;}
    for(const auto& link:graph.links){std::optional<std::size_t> candidate;if(link.from==current)candidate=link.to;else if(link.bidirectional&&link.to==current)candidate=link.from;if(!candidate||*candidate>=graph.nodes.size())continue;const auto& node=graph.nodes[*candidate];if(node.disabled||navigationBlocked(graph,node.position))continue;if(!leaveDeadEnd&&hasThroughNeighbor&&navigationDegree(graph,*candidate)<=1)continue;const bool alternative=*candidate!=avoid;if(foundAlternative&&!alternative)continue;const float score=(node.priority?2.0f:1.0f)*std::max(0.01f,node.weight)/(std::max(0.01f,node.cost)*std::max(0.01f,link.cost));if((alternative&&!foundAlternative)||score>bestScore||(std::abs(score-bestScore)<1e-5f&&((*candidate+seed)%graph.nodes.size()<(best+seed)%graph.nodes.size()))){bestScore=score;best=*candidate;foundAlternative=alternative;}}
    return best;
}

inline std::size_t nextNavigationNodeAttracted(const NavigationGraph& graph,std::size_t current,std::uint32_t seed,std::size_t avoid,scene::Vec3 target,float attraction){
    if(attraction<=0.001f)return nextNavigationNode(graph,current,seed,avoid);std::size_t best=current;float bestScore=-1.0f;bool foundAlternative=false;const float currentDistance=current<graph.nodes.size()?horizontalDistance(graph.nodes[current].position,target):0.0f;const bool leaveDeadEnd=current<graph.nodes.size()&&navigationDegree(graph,current)<=1;bool hasThroughNeighbor=false;for(const auto& link:graph.links){std::optional<std::size_t> candidate;if(link.from==current)candidate=link.to;else if(link.bidirectional&&link.to==current)candidate=link.from;if(candidate&&*candidate<graph.nodes.size()&&!graph.nodes[*candidate].disabled&&!navigationBlocked(graph,graph.nodes[*candidate].position)&&navigationDegree(graph,*candidate)>1)hasThroughNeighbor=true;}
    for(const auto& link:graph.links){std::optional<std::size_t> candidate;if(link.from==current)candidate=link.to;else if(link.bidirectional&&link.to==current)candidate=link.from;if(!candidate||*candidate>=graph.nodes.size())continue;const auto& node=graph.nodes[*candidate];if(node.disabled||navigationBlocked(graph,node.position)||(!leaveDeadEnd&&hasThroughNeighbor&&navigationDegree(graph,*candidate)<=1))continue;const bool alternative=*candidate!=avoid;if(foundAlternative&&!alternative)continue;const float distDiff=currentDistance-horizontalDistance(node.position,target),progress=std::clamp(distDiff/iw::worldUnits(160.0f),-1.0f,1.0f),pull=attraction>=0.8f?(distDiff>0.0f?3.0f+progress*3.0f:std::max(0.01f,1.0f+progress*0.95f)):std::max(0.15f,1.0f+progress*2.2f*std::clamp(attraction,0.0f,1.0f)),personality=(attraction>=0.8f)?1.0f:(0.74f+0.52f*personalityValue(seed*3571u+static_cast<std::uint32_t>(*candidate)*97u)),attractor=1.0f+std::max(0.0f,node.attraction)*(1.0f-std::clamp(horizontalDistance(node.position,graph.nodes[current].position)/std::max(1.0f,node.attractionRadius),0.0f,1.0f));const float score=(node.priority?2.0f:1.0f)*std::max(0.01f,node.weight)*pull*personality*attractor/(std::max(0.01f,node.cost)*std::max(0.01f,link.cost));if((alternative&&!foundAlternative)||score>bestScore){bestScore=score;best=*candidate;foundAlternative=alternative;}}
    return bestScore<0.0f?nextNavigationNode(graph,current,seed,avoid):best;
}

inline constexpr std::array<scene::Vec3,12> kWaypoints{{
    {iw::worldUnits(-720),iw::worldUnits(-520),0},{0,iw::worldUnits(-520),0},{iw::worldUnits(720),iw::worldUnits(-520),0},{iw::worldUnits(920),0,0},{iw::worldUnits(720),iw::worldUnits(520),0},{0,iw::worldUnits(520),0},
    {iw::worldUnits(-720),iw::worldUnits(520),0},{iw::worldUnits(-920),0,0},{iw::worldUnits(-420),iw::worldUnits(-120),0},{0,iw::worldUnits(-120),0},{iw::worldUnits(420),iw::worldUnits(120),0},{0,iw::worldUnits(120),0}
}};
inline constexpr std::array<std::array<int,4>,12> kWaypointLinks{{
    {{1,7,-1,-1}},{{0,2,9,-1}},{{1,3,-1,-1}},{{2,4,10,-1}},
    {{3,5,10,-1}},{{4,6,11,-1}},{{5,7,8,-1}},{{6,0,8,-1}},
    {{7,6,9,-1}},{{1,8,11,-1}},{{3,4,11,-1}},{{5,9,10,-1}}
}};

enum class SpPhase { Patrol, Approach, Aim, Burst, Reposition, Search, Pause, React };

struct Actor {
    SpPhase spPhase{SpPhase::Patrol};
    float spPhaseTime{},spQuietTime{},spPolicyLastHealth{100.0f};
    unsigned spDecisionSequence{};
    int spBurstShots{};
    scene::Vec3 spMoveGoal{};
    scene::Vec3 spLastSeenDirection{},spSearchGoal{};
    scene::Vec3 spHuntGoal{};
    float spHuntRefresh{};
    unsigned spProjectedGoalSequence{std::numeric_limits<unsigned>::max()};
    bool spSearchExtended{};
    bool spWantsMove{},spIdleWindow{},spReactionWindow{},spPolicyInitialized{};
    bool spPatrolWalking{},spCombatMoving{},spSprintWanted{},spJumpApprovalHeld{};
    float spMoveThrottle{},spJumpCooldown{},spAnimatedSpeed{};
    scene::Vec3 spLedgeTravel{};
    float spLedgeTime{},spLedgeProbeTime{};
    float spMantleProbeTime{},spJumpProbeTime{},spCrowdProbeTime{};
    unsigned spJumpSequence{};
    std::size_t spScenario{std::numeric_limits<std::size_t>::max()};
    float spScenarioFrame{},spScenarioWeight{},spScenarioCooldown{},spLastHealth{100.0f};
    bool spPreviouslyVisible{},spScenarioInterrupted{};
    float spPendingPain{},spPendingAlert{};
    unsigned spScenarioSequence{};
    unsigned spChanceSequence{};
    scene::Stance spScenarioStance{scene::Stance::Stand};
    bool spMovingScenario{},spMovingPain{};
    float spMotionYaw{},spMotionForward{},spMotionRight{},spMotionCooldown{},spMotionBlocked{};
    unsigned spMotionSequence{};
    SpVarietyClock spVarietyClock;
    bool spMotionOpportunity{},spAmbientScenario{};
    float spMotionDamagePending{};
    std::uint32_t id{};
    int modelVariant{};
    int weaponSlot{};
    std::size_t viewWeaponAsset{std::numeric_limits<std::size_t>::max()};
    std::size_t worldWeaponAsset{std::numeric_limits<std::size_t>::max()};
    scene::WeaponClass weaponClass{scene::WeaponClass::Rifle};
    std::string team;
    scene::Vec3 position{},velocity{};
    scene::Vec3 previousPhysicsPosition{};
    bool physicsPresentationValid{},fixedPresentationThisUpdate{};
    float physicsDeltaThisUpdate{};
    float yaw{},pitch{},fixedAccumulator{};
    scene::Direction locomotionDirection{scene::Direction::Any};
    float locomotionDirectionHold{},smoothedForward{},smoothedRight{},combatStrafeTime{};
    int combatStrafeDirection{1};
    scene::Stance stance{scene::Stance::Stand};
    bool grounded{true},previousJump{},mantling{};
    scene::Vec3 mantleStart{},mantleEnd{};
    float mantleClearanceZ{-std::numeric_limits<float>::max()};
    float mantleElapsed{},mantleDuration{};
    Input input;
    std::size_t animation{};
    float animationFrame{};
    presentation::AirborneAnimation airborneAnimation;
    BehaviorState behavior{BehaviorState::Idle};
    std::size_t waypoint{};
    std::size_t previousWaypoint{std::numeric_limits<std::size_t>::max()};
    std::array<std::size_t,kWaypoints.size()> route{};
    std::size_t routeCount{},routeCursor{};
    std::vector<std::size_t> navigationRoute;
    std::size_t navigationRouteCursor{};
    float repathTime{};
    float stuckTime{},recoveryTime{};
    int recoveryDirection{1};
    bool targetVisible{};
    float health{100.0f},respawnTime{};
    bool alive{true};
    scene::Vec3 spawnPosition{};
    int ammo{30};
    float reloadTime{};
    bool reloadStarted{};
    float targetVisibleTime{},behaviorClock{};
    float fireCooldown{},firePoseTime{},muzzleFlashTime{};
    std::size_t actionAnimation{std::numeric_limits<std::size_t>::max()};
    float actionFrame{},actionBlendWeight{};
    float lastActionShotTime{-1e9f};
    const NavigationGraph* navigation{};
    float movementIntentTime{},groundGrace{};
    bool sprintIntent{};
    float awareness{},awarenessMemory{};
    scene::Vec3 lastKnownPlayer{};
    float perceptionScale{},pursuitCommitment{},routePersonality{};
    float obstacleAvoidTime{},obstacleProbeTime{},visibilityCheckTime{};
    float crowdAvoidTime{},movementThrottle{};
    float jumpOpportunityTime{},equipmentOpportunityTime{};
    float equipmentPoseTime{},equipmentRunTime{},jumpDropCooldown{},stanceBehaviorTime{},stanceRecoveryTime{},sidequestTime{},sidequestOpportunityTime{};int equipmentSequence{};
    std::size_t sidequestGoal{std::numeric_limits<std::size_t>::max()};
    float combatGoalTime{};
    std::size_t combatGoal{std::numeric_limits<std::size_t>::max()};
    std::size_t previousAnimation{std::numeric_limits<std::size_t>::max()};
    float previousAnimationFrame{},animationBlendElapsed{1.0f};
    int obstacleAvoidDirection{1};
    bool cachedSceneVisibility{true};
    weapon::Stats cachedWeaponStats{};
    float wallContactTime{};
    float routeDiversionTime{};
    float diversionYaw{};
    float blockedHeading{};
    float blockedHeadingMemory{};
    int preferredCircumventionSide{};
    scene::Vec3 wanderTarget{};
    float wanderTargetTime{};
    scene::Vec2 lastStuckCheckPos{};
    float stuckCheckTimer{0.0f};
    int consecutiveStuckEvents{0};
    bool initializedStuckPos{false};
    float combatMoveTimer{0.0f};
    float combatStrafeDir{0.0f};
    float combatStanceTimer{0.0f};
    std::size_t targetGoalArea{std::numeric_limits<std::size_t>::max()};
    std::size_t authoredDestination{std::numeric_limits<std::size_t>::max()};
    float authoredDestinationUntil{};
    bool authoredCrouch{};
};

// A new life retains identity and loadout, never an old action or trajectory.
inline void respawnActor(Actor& actor,scene::Vec3 spawn){
    Actor fresh;
    fresh.id=actor.id;fresh.modelVariant=actor.modelVariant;fresh.weaponSlot=actor.weaponSlot;
    fresh.viewWeaponAsset=actor.viewWeaponAsset;fresh.worldWeaponAsset=actor.worldWeaponAsset;
    fresh.weaponClass=actor.weaponClass;fresh.team=actor.team;
    fresh.cachedWeaponStats=actor.cachedWeaponStats;fresh.navigation=actor.navigation;
    fresh.perceptionScale=actor.perceptionScale;fresh.pursuitCommitment=actor.pursuitCommitment;
    fresh.routePersonality=actor.routePersonality;
    fresh.position=fresh.spawnPosition=fresh.previousPhysicsPosition=spawn;
    fresh.yaw=actor.yaw;fresh.behavior=BehaviorState::Patrol;
    fresh.ammo=actor.weaponClass==scene::WeaponClass::Pistol?12:actor.weaponClass==scene::WeaponClass::Sniper?5:actor.weaponClass==scene::WeaponClass::Shotgun?8:actor.weaponClass==scene::WeaponClass::LMG?75:30;
    fresh.equipmentOpportunityTime=8.f+static_cast<float>(actor.id%7)*2.3f;
    actor=std::move(fresh);
}

inline float wrapAngle(float angle){while(angle>scene::kPi)angle-=2.0f*scene::kPi;while(angle<-scene::kPi)angle+=2.0f*scene::kPi;return angle;}

inline std::size_t nearestWaypoint(const scene::Vec3& position){std::size_t best{};float bestDistance=std::numeric_limits<float>::max();for(std::size_t i=0;i<kWaypoints.size();++i){const float distance=scene::length(kWaypoints[i]-position);if(distance<bestDistance){best=i;bestDistance=distance;}}return best;}

inline bool lineOfSight(const scene::Vec3& from,const scene::Vec3& to){const scene::Vec3 eyeFrom=from+scene::Vec3{0,0,iw::worldUnits(60)},eyeTo=to+scene::Vec3{0,0,iw::worldUnits(60)};for(int sample=1;sample<32;++sample){const float t=static_cast<float>(sample)/32.0f;const auto point=eyeFrom+(eyeTo-eyeFrom)*t;for(const auto& box:scene::course::kBoxes)if(scene::course::inside(point.x,point.y,box,scene::course::scaled(1))&&point.z<box.height+scene::course::scaled(2))return false;}return true;}

inline void buildRoute(Actor& actor,std::size_t start,std::size_t goal){
    std::array<float,kWaypoints.size()> distance;distance.fill(std::numeric_limits<float>::max());std::array<int,kWaypoints.size()> previous;previous.fill(-1);std::array<bool,kWaypoints.size()> visited{};distance[start]=0;
    for(std::size_t pass=0;pass<kWaypoints.size();++pass){std::size_t current=kWaypoints.size();float best=std::numeric_limits<float>::max();for(std::size_t i=0;i<kWaypoints.size();++i)if(!visited[i]&&distance[i]<best){best=distance[i];current=i;}if(current==kWaypoints.size()||current==goal)break;visited[current]=true;for(const int linked:kWaypointLinks[current])if(linked>=0){const auto next=static_cast<std::size_t>(linked);const float candidate=distance[current]+scene::length(kWaypoints[next]-kWaypoints[current]);if(candidate<distance[next]){distance[next]=candidate;previous[next]=static_cast<int>(current);}}}
    std::array<std::size_t,kWaypoints.size()> reverse{};std::size_t count{};for(int node=static_cast<int>(goal);node>=0&&count<reverse.size();node=previous[static_cast<std::size_t>(node)]){reverse[count++]=static_cast<std::size_t>(node);if(static_cast<std::size_t>(node)==start)break;}actor.routeCount=0;actor.routeCursor=0;while(count>0)actor.route[actor.routeCount++]=reverse[--count];if(actor.routeCount>1)actor.routeCursor=1;
}

inline void buildNavigationRoute(Actor& actor,const NavigationGraph& graph,std::size_t start,std::size_t goal){
    actor.navigationRoute.clear();actor.navigationRouteCursor=0;if(start>=graph.nodes.size()||goal>=graph.nodes.size())return;
    std::vector<std::vector<std::pair<std::size_t,float>>> adjacency(graph.nodes.size());for(const auto& link:graph.links)if(link.from<graph.nodes.size()&&link.to<graph.nodes.size()){adjacency[link.from].push_back({link.to,link.cost});if(link.bidirectional)adjacency[link.to].push_back({link.from,link.cost});}
    std::vector<float> distance(graph.nodes.size(),std::numeric_limits<float>::max());std::vector<int> previous(graph.nodes.size(),-1);using QueueItem=std::pair<float,std::size_t>;std::priority_queue<QueueItem,std::vector<QueueItem>,std::greater<QueueItem>> open;distance[start]=0.0f;open.push({0.0f,start});while(!open.empty()){const auto [known,current]=open.top();open.pop();if(known>distance[current]+1e-4f)continue;if(current==goal)break;if(graph.nodes[current].disabled||navigationBlocked(graph,graph.nodes[current].position))continue;for(const auto [next,linkCost]:adjacency[current]){if(graph.nodes[next].disabled||navigationBlocked(graph,graph.nodes[next].position))continue;const auto& node=graph.nodes[next];const float preference=0.62f+0.76f*personalityValue(actor.id*4099u+static_cast<std::uint32_t>(current)*131u+static_cast<std::uint32_t>(next)*17u);const float attractor=1.0f+std::max(0.0f,node.attraction);const float candidate=known+horizontalDistance(graph.nodes[current].position,node.position)*std::max(0.01f,linkCost)*std::max(0.01f,node.cost)*preference/attractor;if(candidate<distance[next]){distance[next]=candidate;previous[next]=static_cast<int>(current);open.push({candidate,next});}}}
    if(start!=goal&&previous[goal]<0)return;std::vector<std::size_t> reverse;for(int node=static_cast<int>(goal);node>=0;node=previous[static_cast<std::size_t>(node)]){reverse.push_back(static_cast<std::size_t>(node));if(static_cast<std::size_t>(node)==start)break;}while(!reverse.empty()){actor.navigationRoute.push_back(reverse.back());reverse.pop_back();}if(actor.navigationRoute.size()>1)actor.navigationRouteCursor=1;
}

inline std::optional<std::size_t> variedNavigationGoal(const NavigationGraph& graph,scene::Vec3 point,const Actor& actor){
    const auto nearest=nearestNavigationNode(graph,point);if(!nearest)return std::nullopt;const float nearestDistance=horizontalDistance(graph.nodes[*nearest].position,point),allowance=iw::worldUnits(80.0f+160.0f*actor.routePersonality);std::size_t best=*nearest;float bestScore=std::numeric_limits<float>::max();const auto epoch=static_cast<std::uint32_t>(actor.behaviorClock*0.25f);for(std::size_t i=0;i<graph.nodes.size();++i){const auto& node=graph.nodes[i];if(node.disabled||navigationBlocked(graph,node.position))continue;const float distance=horizontalDistance(node.position,point);if(distance>nearestDistance+allowance)continue;const float preference=0.72f+0.58f*personalityValue(actor.id*8191u+static_cast<std::uint32_t>(i)*313u+epoch*29u),score=distance*preference/(node.priority?1.08f:1.0f);if(score<bestScore){bestScore=score;best=i;}}return best;
}

// Included here after shared actor/navigation definitions, inside this namespace.
inline std::optional<scene::Vec3> mapPatrolDestination(Actor& a,const BehaviorConfig& c){
    if(!c.mapDestinations||c.mapDestinations->empty())return {};
    const auto& choices=*c.mapDestinations;
    if(a.authoredDestination>=choices.size()&&a.behaviorClock<a.authoredDestinationUntil)return {};
    if(a.authoredDestination<choices.size()&&a.behaviorClock<a.authoredDestinationUntil&&horizontalDistance(a.position,choices[a.authoredDestination].position)>90)return choices[a.authoredDestination].position;
    std::size_t best=choices.size();float score=-1;
    for(std::size_t i=0;i<choices.size();++i){const float distance=horizontalDistance(a.position,choices[i].position);if(distance<120||distance>6000||i==a.authoredDestination)continue;
        const float r=personalityValue(a.id*9143u+unsigned(i)*7919u+unsigned(a.behaviorClock)*17u);
        const float candidate=r*choices[i].weight/(1+distance/4000);if(candidate>score){score=candidate;best=i;}
    }
    a.authoredDestination=best;a.authoredDestinationUntil=a.behaviorClock+20;
    return best<choices.size()?std::optional<scene::Vec3>(choices[best].position):std::nullopt;
}
#include "gameplay/BotSpBehavior.inl"

inline void updateBehavior(Actor& actor,const scene::Vec3& player,const BehaviorConfig& config,float deltaSeconds,const NavigationGraph* navigation=nullptr,bool sceneVisibility=true){
    if(config.spPlayerPolicy){updateSpPlayerBehavior(actor,player,config,deltaSeconds,sceneVisibility);return;}
    if(!navigation)navigation=actor.navigation;
    actor.input={};actor.movementIntentTime=std::max(0.0f,actor.movementIntentTime-deltaSeconds);actor.groundGrace=std::max(0.0f,actor.groundGrace-deltaSeconds);actor.reloadStarted=false;actor.fireCooldown=std::max(0.0f,actor.fireCooldown-deltaSeconds);actor.firePoseTime=std::max(0.0f,actor.firePoseTime-deltaSeconds);actor.equipmentPoseTime=std::max(0.0f,actor.equipmentPoseTime-deltaSeconds);actor.muzzleFlashTime=std::max(0.0f,actor.muzzleFlashTime-deltaSeconds);if(actor.reloadTime>0){actor.reloadTime=std::max(0.0f,actor.reloadTime-deltaSeconds);actor.input.reload=true;if(actor.reloadTime<=0)actor.ammo=std::max(1,config.magazineSize);}
    if(actor.ammo<=0&&actor.reloadTime<=0){actor.reloadTime=std::max(0.2f,config.reloadDuration);actor.reloadStarted=true;actor.input.reload=true;actor.firePoseTime=actor.reloadTime;}
    if(actor.perceptionScale<=0.0f){actor.perceptionScale=0.72f+0.48f*personalityValue(actor.id*1013u+7u);actor.pursuitCommitment=0.30f+0.68f*personalityValue(actor.id*2039u+19u);actor.routePersonality=personalityValue(actor.id*4099u+31u);actor.lastKnownPlayer=actor.position;}
    actor.behaviorClock+=deltaSeconds;actor.repathTime=std::max(0.0f,actor.repathTime-deltaSeconds);actor.awarenessMemory=std::max(0.0f,actor.awarenessMemory-deltaSeconds);const scene::Vec3 toPlayer=player-actor.position;const float distance=scene::length(toPlayer);const scene::Vec3 facing{std::cos(actor.yaw),std::sin(actor.yaw),0},flatDirection=scene::normalize(scene::Vec3{toPlayer.x,toPlayer.y,0});const bool withinVisionCone=distance<iw::worldUnits(90.0f)||scene::dot(facing,flatDirection)>0.28f;    const bool isClear = sceneVisibility && lineOfSight(actor.position, player);
    actor.targetVisible=distance<=config.detectionRange*actor.perceptionScale&&withinVisionCone&&isClear;if(actor.targetVisible){actor.targetVisibleTime+=deltaSeconds;actor.awareness=std::min(1.0f,actor.awareness+deltaSeconds*(1.7f+actor.perceptionScale));actor.awarenessMemory=1.25f+3.75f*actor.pursuitCommitment;actor.lastKnownPlayer=player;}else{actor.targetVisibleTime=0.0f;actor.awareness=std::max(0.0f,actor.awareness-deltaSeconds*(0.16f+0.28f*(1.0f-actor.pursuitCommitment)));if(actor.awareness<0.12f&&distance<config.hearingRange*actor.perceptionScale){const float angle=personalityValue(actor.id*6151u+static_cast<std::uint32_t>(actor.behaviorClock))*2.0f*scene::kPi;actor.awareness=std::min(0.36f,actor.awareness+deltaSeconds*0.32f);actor.awarenessMemory=std::max(actor.awarenessMemory,0.6f+actor.pursuitCommitment);actor.lastKnownPlayer=player+scene::Vec3{std::cos(angle),std::sin(angle),0}*iw::worldUnits(48.0f+96.0f*(1.0f-actor.perceptionScale));}if(config.playerAttraction>=0.45f){actor.lastKnownPlayer=player;actor.awareness=std::max(actor.awareness,config.playerAttraction*0.95f);actor.awarenessMemory=std::max(actor.awarenessMemory,3.0f+5.0f*config.playerAttraction);}}
    const float reaction=std::max(0.02f,config.reactionDelay*(1.35f-0.45f*actor.perceptionScale));actor.sidequestTime=std::max(0.0f,actor.sidequestTime-deltaSeconds);actor.sidequestOpportunityTime=std::max(0.0f,actor.sidequestOpportunityTime-deltaSeconds);actor.combatGoalTime=std::max(0.0f,actor.combatGoalTime-deltaSeconds);actor.behavior=actor.targetVisible&&actor.targetVisibleTime>=reaction?BehaviorState::Engage:(actor.awarenessMemory>0.0f&&actor.awareness>0.08f?BehaviorState::Chase:BehaviorState::Patrol);
    if(config.playerAttraction<0.45f&&config.allowSidequests&&navigation&&!navigation->nodes.empty()&&actor.targetVisible&&actor.sidequestTime<=0.0f&&actor.sidequestOpportunityTime<=0.0f){const float roll=personalityValue(actor.id*19001u+static_cast<std::uint32_t>(actor.behaviorClock*3.0f));actor.sidequestOpportunityTime=3.5f+personalityValue(actor.id*7919u+static_cast<std::uint32_t>(actor.behaviorClock))*7.0f;if(roll<config.sidequestChance){std::vector<std::size_t> choices;for(std::size_t i=0;i<navigation->nodes.size();++i)if(!navigation->nodes[i].disabled&&!navigationBlocked(*navigation,navigation->nodes[i].position)&&navigationDegree(*navigation,i)>1&&horizontalDistance(navigation->nodes[i].position,actor.position)>iw::worldUnits(180.0f))choices.push_back(i);if(!choices.empty()){const auto pick=static_cast<std::size_t>(personalityValue(actor.id*29009u+static_cast<std::uint32_t>(actor.behaviorClock*11.0f))*choices.size())%choices.size();actor.sidequestGoal=choices[pick];actor.sidequestTime=config.sidequestMinDuration+(config.sidequestMaxDuration-config.sidequestMinDuration)*personalityValue(actor.id*13007u+static_cast<std::uint32_t>(actor.behaviorClock));}}}
    if(actor.sidequestTime>0.0f&&navigation&&actor.sidequestGoal<navigation->nodes.size())actor.behavior=BehaviorState::Flank;scene::Vec3 target=actor.behavior==BehaviorState::Engage?player:actor.behavior==BehaviorState::Flank?navigation->nodes[actor.sidequestGoal].position:actor.lastKnownPlayer;
    const auto getArrivalRadius = [&](const NavigationNode& node) -> float {
        if (config.universalNodeArrivalRadius > 0.0f) return config.universalNodeArrivalRadius;
        return std::max(iw::worldUnits(4.0f), node.radius * std::max(0.1f, config.nodeArrivalRadiusMultiplier));
    };
    const float fallbackArrival = config.universalNodeArrivalRadius > 0.0f ? config.universalNodeArrivalRadius : iw::worldUnits(24.0f) * std::max(0.1f, config.nodeArrivalRadiusMultiplier);

    if(actor.behavior==BehaviorState::Engage&&navigation&&!navigation->nodes.empty()&&config.tacticalReposition>0.01f){
        const bool reached=actor.combatGoal<navigation->nodes.size()&&horizontalDistance(actor.position,navigation->nodes[actor.combatGoal].position)<iw::worldUnits(42.0f);
        if(actor.combatGoalTime<=0.0f||actor.combatGoal>=navigation->nodes.size()||reached){
            float best=-std::numeric_limits<float>::max();std::size_t goal=std::numeric_limits<std::size_t>::max();const auto towardPlayer=scene::normalize(scene::Vec3{toPlayer.x,toPlayer.y,0});
            for(std::size_t i=0;i<navigation->nodes.size();++i){const auto& node=navigation->nodes[i];if(node.disabled||navigationBlocked(*navigation,node.position)||navigationDegree(*navigation,i)<=1)continue;const float travel=horizontalDistance(actor.position,node.position);if(travel<iw::worldUnits(90.0f)||travel>iw::worldUnits(850.0f))continue;const float playerRange=horizontalDistance(player,node.position),rangeFit=1.0f-std::clamp(std::abs(playerRange-config.preferredRange)/std::max(iw::worldUnits(100.0f),config.preferredRange),0.0f,1.0f);const auto towardNode=scene::normalize(scene::Vec3{node.position.x-actor.position.x,node.position.y-actor.position.y,0});const float lateral=std::abs(towardPlayer.x*towardNode.y-towardPlayer.y*towardNode.x),height=std::clamp((node.position.z-player.z)/iw::worldUnits(120.0f),-0.4f,1.0f),individual=0.72f+0.56f*personalityValue(actor.id*44017u+static_cast<std::uint32_t>(i)*193u+static_cast<std::uint32_t>(actor.behaviorClock));const float score=(rangeFit*1.25f+lateral*1.1f+height*0.35f+(node.priority?0.45f:0.0f))*individual*config.tacticalReposition-travel/iw::worldUnits(1800.0f);if(score>best){best=score;goal=i;}}
            actor.combatGoal=goal;actor.combatGoalTime=goal<navigation->nodes.size()?2.0f+4.0f*personalityValue(actor.id*53003u+static_cast<std::uint32_t>(actor.behaviorClock*2.0f)):0.8f;actor.navigationRoute.clear();actor.navigationRouteCursor=0;if(goal<navigation->nodes.size())if(const auto start=nearestNavigationNode(*navigation,actor.position))buildNavigationRoute(actor,*navigation,*start,goal);
        }
        if(actor.navigationRouteCursor<actor.navigationRoute.size()){actor.waypoint=actor.navigationRoute[actor.navigationRouteCursor];target=navigation->nodes[actor.waypoint].position;const float arrival=getArrivalRadius(navigation->nodes[actor.waypoint]);if(horizontalDistance(target,actor.position)<arrival&&actor.navigationRouteCursor+1<actor.navigationRoute.size())target=navigation->nodes[actor.navigationRoute[++actor.navigationRouteCursor]].position;}else if(actor.combatGoal<navigation->nodes.size())target=navigation->nodes[actor.combatGoal].position;
    }
    if(actor.behavior==BehaviorState::Patrol){
        if(navigation&&!navigation->nodes.empty()){
            std::vector<std::size_t> activeGoalAreas;
            for(std::size_t ga=0; ga<navigation->goalAreas.size(); ++ga){
                if(navigation->goalAreas[ga].enabled && navigation->goalAreas[ga].weight > 0.001f)
                    activeGoalAreas.push_back(ga);
            }
            if(!activeGoalAreas.empty()){
                bool validGoal = actor.targetGoalArea < navigation->goalAreas.size() && navigation->goalAreas[actor.targetGoalArea].enabled;
                if(!validGoal){
                    float totalWeight = 0.0f;
                    for(std::size_t ga : activeGoalAreas) totalWeight += navigation->goalAreas[ga].weight;
                    float roll = personalityValue(actor.id * 18803u + static_cast<std::uint32_t>(actor.behaviorClock * 0.1f)) * totalWeight;
                    float accum = 0.0f;
                    actor.targetGoalArea = activeGoalAreas.front();
                    for(std::size_t ga : activeGoalAreas){
                        accum += navigation->goalAreas[ga].weight;
                        if(roll <= accum){ actor.targetGoalArea = ga; break; }
                    }
                }
                const auto& curGoal = navigation->goalAreas[actor.targetGoalArea];
                const float distToGoal = horizontalDistance(actor.position, curGoal.position);
                if(distToGoal > curGoal.radius){
                    if(actor.repathTime <= 0.0f || actor.navigationRoute.empty() || actor.navigationRouteCursor >= actor.navigationRoute.size()){
                        std::optional<std::size_t> targetNode;
                        float bestDist = std::numeric_limits<float>::max();
                        for(std::size_t ni = 0; ni < navigation->nodes.size(); ++ni){
                            if(navigation->nodes[ni].disabled || navigationBlocked(*navigation, navigation->nodes[ni].position)) continue;
                            float d = horizontalDistance(navigation->nodes[ni].position, curGoal.position);
                            if(d < bestDist){ bestDist = d; targetNode = ni; }
                        }
                        if(targetNode){
                            const auto start = nearestNavigationNode(*navigation, actor.position);
                            if(start) buildNavigationRoute(actor, *navigation, *start, *targetNode);
                            actor.repathTime = 1.5f;
                        }
                    }
                    if(actor.navigationRouteCursor < actor.navigationRoute.size()){
                        actor.waypoint = actor.navigationRoute[actor.navigationRouteCursor];
                        target = navigation->nodes[actor.waypoint].position;
                        const float arrival = getArrivalRadius(navigation->nodes[actor.waypoint]);
                        if(horizontalDistance(target, actor.position) < arrival && actor.navigationRouteCursor + 1 < actor.navigationRoute.size()){
                            actor.waypoint = actor.navigationRoute[++actor.navigationRouteCursor];
                            target = navigation->nodes[actor.waypoint].position;
                        }
                    }
                } else {
                    actor.waypoint %= navigation->nodes.size();
                    if(navigation->nodes[actor.waypoint].disabled || navigationBlocked(*navigation, navigation->nodes[actor.waypoint].position))
                        if(const auto nearest = nearestNavigationNode(*navigation, actor.position)) actor.waypoint = *nearest;
                    target = navigation->nodes[actor.waypoint].position;
                    const float arrival = getArrivalRadius(navigation->nodes[actor.waypoint]);
                    if(horizontalDistance(target, actor.position) < arrival){
                        const auto reached = actor.waypoint;
                        actor.waypoint = nextNavigationNodeAttracted(*navigation, reached, actor.id, actor.previousWaypoint, curGoal.position, 0.45f);
                        actor.previousWaypoint = reached;
                        target = navigation->nodes[actor.waypoint].position;
                    }
                }
            } else {
                actor.waypoint%=navigation->nodes.size();
                if(navigation->nodes[actor.waypoint].disabled||navigationBlocked(*navigation,navigation->nodes[actor.waypoint].position))
                    if(const auto nearest=nearestNavigationNode(*navigation,actor.position))actor.waypoint=*nearest;
                target=navigation->nodes[actor.waypoint].position;
                const float arrival=getArrivalRadius(navigation->nodes[actor.waypoint]);
                if(horizontalDistance(target,actor.position)<arrival){
                    const auto reached=actor.waypoint;
                    actor.waypoint=nextNavigationNodeAttracted(*navigation,reached,actor.id,actor.previousWaypoint,player,config.playerAttraction);
                    actor.previousWaypoint=reached;
                    target=navigation->nodes[actor.waypoint].position;
                }
            }
        } else {
            target=kWaypoints[actor.waypoint%kWaypoints.size()];
            if(horizontalDistance(target,actor.position)<fallbackArrival){
                actor.waypoint=(actor.waypoint+1+actor.id%3)%kWaypoints.size();
                target=kWaypoints[actor.waypoint];
            }
        }
    }
    else if(actor.behavior==BehaviorState::Flank){if(navigation&&actor.sidequestGoal<navigation->nodes.size()){if(actor.repathTime<=0.0f||actor.navigationRoute.empty()){const auto start=nearestNavigationNode(*navigation,actor.position);if(start)buildNavigationRoute(actor,*navigation,*start,actor.sidequestGoal);actor.repathTime=0.65f;}if(actor.navigationRouteCursor<actor.navigationRoute.size()){actor.waypoint=actor.navigationRoute[actor.navigationRouteCursor];target=navigation->nodes[actor.waypoint].position;if(horizontalDistance(target,actor.position)<getArrivalRadius(navigation->nodes[actor.waypoint])&&actor.navigationRouteCursor+1<actor.navigationRoute.size())target=navigation->nodes[actor.navigationRoute[++actor.navigationRouteCursor]].position;}if(horizontalDistance(actor.position,navigation->nodes[actor.sidequestGoal].position)<iw::worldUnits(36.0f))actor.sidequestTime=0.0f;}else actor.sidequestTime=0.0f;}
    else if(actor.behavior==BehaviorState::Chase){
        const bool canDirectPursue = actor.targetVisible && distance < config.preferredRange * 1.5f;
        if(canDirectPursue)target=player;
        else if(navigation&&!navigation->nodes.empty()){
            if(actor.repathTime<=0||actor.navigationRoute.empty()){
                const auto start=nearestNavigationNode(*navigation,actor.position),goal=variedNavigationGoal(*navigation,actor.lastKnownPlayer,actor);
                if(start&&goal)buildNavigationRoute(actor,*navigation,*start,*goal);
                actor.repathTime=0.35f+0.55f*(1.0f-actor.pursuitCommitment);
            }
            if(actor.navigationRouteCursor<actor.navigationRoute.size()){
                actor.waypoint=actor.navigationRoute[actor.navigationRouteCursor];
                target=navigation->nodes[actor.waypoint].position;
                const float arrival=getArrivalRadius(navigation->nodes[actor.waypoint]);
                if(horizontalDistance(target,actor.position)<arrival&&actor.navigationRouteCursor+1<actor.navigationRoute.size()){
                    actor.waypoint=actor.navigationRoute[++actor.navigationRouteCursor];
                    target=navigation->nodes[actor.waypoint].position;
                }
            }
        }else {
            if(actor.repathTime<=0||actor.routeCursor>=actor.routeCount){
                buildRoute(actor,nearestWaypoint(actor.position),nearestWaypoint(actor.lastKnownPlayer));
                actor.repathTime=0.30f+0.45f*(1.0f-actor.pursuitCommitment);
            }
            if(actor.routeCursor<actor.routeCount){
                target=kWaypoints[actor.route[actor.routeCursor]];
                if(horizontalDistance(target,actor.position)<fallbackArrival&&actor.routeCursor+1<actor.routeCount)
                    target=kWaypoints[actor.route[++actor.routeCursor]];
            }
        }
    }
    actor.routeDiversionTime=std::max(0.0f,actor.routeDiversionTime-deltaSeconds);
    actor.blockedHeadingMemory=std::max(0.0f,actor.blockedHeadingMemory-deltaSeconds);
    actor.wanderTargetTime=std::max(0.0f,actor.wanderTargetTime-deltaSeconds);
    if(actor.behavior==BehaviorState::Patrol)if(const auto authored=mapPatrolDestination(actor,config))target=*authored;
    const scene::Vec3 direction=target-actor.position;
    float travelYaw=std::atan2(direction.y,direction.x);
    float desiredYaw=travelYaw;
    if(actor.routeDiversionTime>0.0f){
        travelYaw=actor.diversionYaw;
        desiredYaw=actor.diversionYaw;
    } else if(actor.blockedHeadingMemory>0.0f && actor.behavior!=BehaviorState::Engage){
        const float headingDiff = wrapAngle(travelYaw - actor.blockedHeading);
        if (std::abs(headingDiff) < 1.15f) {
            const float deflectSign = headingDiff >= 0.0f ? 1.0f : -1.0f;
            travelYaw = wrapAngle(actor.blockedHeading + deflectSign * 1.35f);
            desiredYaw = travelYaw;
        }
    }

    actor.obstacleProbeTime=std::max(0.0f,actor.obstacleProbeTime-deltaSeconds);
    actor.obstacleAvoidTime=std::max(0.0f,actor.obstacleAvoidTime-deltaSeconds);
    const float probeDist=iw::worldUnits(48.0f);
    auto probeBlocked=[&](scene::Vec3 from,float yawAngle)->bool{
        scene::Vec3 pt=from+scene::Vec3{std::cos(yawAngle),std::sin(yawAngle),0}*probeDist;
        for(const auto& box:scene::course::kBoxes)if(scene::course::inside(pt.x,pt.y,box,scene::course::scaled(1))&&pt.z<box.height+scene::course::scaled(2))return true;
        return false;
    };
    if(actor.grounded&&actor.obstacleProbeTime<=0.0f){
        actor.obstacleProbeTime=0.08f;
        if(probeBlocked(actor.position,travelYaw)){
            const bool leftOpen=!probeBlocked(actor.position,travelYaw+0.75f);
            const bool rightOpen=!probeBlocked(actor.position,travelYaw-0.75f);
            actor.obstacleAvoidDirection=(leftOpen&&!rightOpen)?1:((!leftOpen&&rightOpen)?-1:((actor.id%2==0)?1:-1));
            actor.obstacleAvoidTime=0.55f;
        }
    }
    if(actor.obstacleAvoidTime>0.0f){
        travelYaw=wrapAngle(travelYaw+static_cast<float>(actor.obstacleAvoidDirection)*0.85f);
        desiredYaw=travelYaw;
    }

    const bool movingToTacticalPoint = navigation && actor.combatGoal < navigation->nodes.size() && actor.combatGoalTime > 0.0f;
    if(actor.behavior==BehaviorState::Engage && (!movingToTacticalPoint || actor.targetVisible)){
        desiredYaw=std::atan2(toPlayer.y,toPlayer.x)+std::sin(actor.behaviorClock*1.71f+actor.id*2.17f)*config.aimErrorDegrees*scene::kPi/180.0f;
    } else if(actor.obstacleAvoidTime<=0.0f && actor.routeDiversionTime<=0.0f && actor.behavior!=BehaviorState::Flank && actor.behavior!=BehaviorState::Engage){
        const float scanStrength=0.08f+0.26f*personalityValue(actor.id*3253u+17u);
        desiredYaw+=std::sin(actor.behaviorClock*(0.31f+0.17f*actor.routePersonality)+actor.id*1.91f)*scanStrength;
    }

    // Natural smooth exponential turning with realistic angular velocity limits (mimics player 90th percentile cornering ~284 deg/s)
    const float turnRate = (actor.behavior == BehaviorState::Engage && actor.targetVisible) ? 7.5f : 4.5f;
    const float maxTurnRate = (actor.behavior == BehaviorState::Engage && actor.targetVisible) ? 8.5f : 4.9f;
    const float targetYawDelta = wrapAngle(desiredYaw - actor.yaw);
    const float turnResponse = 1.0f - std::exp(-turnRate * deltaSeconds);
    const float turnStep = std::clamp(targetYawDelta * turnResponse, -maxTurnRate * deltaSeconds, maxTurnRate * deltaSeconds);
    actor.yaw = wrapAngle(actor.yaw + turnStep);

    const float travelError = wrapAngle(travelYaw - actor.yaw);
    const float yawError = std::abs(wrapAngle(desiredYaw - actor.yaw));

    if(actor.behavior == BehaviorState::Engage && !movingToTacticalPoint){
        // Dynamic T6 Combat Out-movement: bots actively maneuver to out-move the player
        actor.combatMoveTimer = std::max(0.0f, actor.combatMoveTimer - deltaSeconds);
        if(actor.combatMoveTimer <= 0.0f){
            // 0.22s to 0.45s rapid lateral trims, matching empirical player trajectory data
            actor.combatMoveTimer = 0.22f + 0.23f * personalityValue(actor.id * 7129u + static_cast<std::uint32_t>(actor.behaviorClock * 5.0f));
            const float roll = personalityValue(actor.id * 13337u + static_cast<std::uint32_t>(actor.behaviorClock * 9.0f));
            actor.combatStrafeDir = roll < 0.38f ? -1.0f : (roll < 0.76f ? 1.0f : 0.0f);
        }

        // Distance keeping: advance when far, back off when too close, push forward when in sweet spot
        if(distance > config.preferredRange * 1.15f){
            actor.input.forward = 1.0f; // close distance
        } else if(distance < config.preferredRange * 0.55f){
            actor.input.forward = -0.7f; // backpedal
        } else {
            actor.input.forward = 0.5f; // tactical pressure
        }
        actor.input.right = actor.combatStrafeDir;

        // Occasional combat jump
        actor.combatStanceTimer = std::max(0.0f, actor.combatStanceTimer - deltaSeconds);
        if(actor.combatStanceTimer <= 0.0f && actor.grounded){
            actor.combatStanceTimer = 1.8f + 2.4f * personalityValue(actor.id * 9901u + static_cast<std::uint32_t>(actor.behaviorClock * 2.0f));
            const float stanceRoll = personalityValue(actor.id * 31121u + static_cast<std::uint32_t>(actor.behaviorClock * 3.0f));
            if(stanceRoll < 0.25f){
                actor.input.jump = true; // combat jump shot
            }
        }

        actor.input.ads = distance < config.detectionRange * 0.85f;
        if(actor.reloadTime <= 0 && actor.targetVisible && actor.targetVisibleTime >= reaction && yawError < 0.085f && actor.fireCooldown <= 0){
            actor.input.fire = true;
            actor.fireCooldown = std::max(0.02f, config.fireInterval);
            actor.firePoseTime = std::max(0.10f, config.fireInterval);
            actor.muzzleFlashTime = 0.055f;
            --actor.ammo;
        }

        // If out of ammo or reloading, immediately initiate tactical reposition to cover
        if((actor.ammo <= 3 || actor.reloadTime > 0.0f) && navigation && !navigation->nodes.empty()){
            actor.combatGoalTime = 0.0f;
        }
    } else {
        // Natural human-like W-dominant locomotion (steer with body yaw, 0 lateral sliding)
        const float absTravel = std::abs(travelError);
        if (absTravel < 1.4f) {
            actor.input.forward = 1.0f;
            actor.input.right = 0.0f;
        } else {
            actor.input.forward = std::max(0.2f, std::cos(travelError));
            actor.input.right = 0.0f;
        }
        actor.input.ads = false;
        actor.input.sprint = (actor.behavior == BehaviorState::Flank || actor.behavior == BehaviorState::Chase || scene::length(direction) > iw::worldUnits(280.0f)) && absTravel < 0.6f;
    }

    actor.equipmentOpportunityTime=std::max(0.0f,actor.equipmentOpportunityTime-deltaSeconds);
    if(config.allowEquipment&&actor.equipmentOpportunityTime<=0.0f&&actor.behavior!=BehaviorState::Engage&&actor.grounded){
        const float chance=personalityValue(actor.id*17401u+static_cast<std::uint32_t>(actor.behaviorClock*2.0f));const float intervalMix=personalityValue(actor.id*11003u+static_cast<std::uint32_t>(actor.behaviorClock));actor.equipmentOpportunityTime=config.equipmentMinInterval+(config.equipmentMaxInterval-config.equipmentMinInterval)*intervalMix;if(chance>1.0f-config.equipmentChance){const float style=personalityValue(actor.id*23117u+static_cast<std::uint32_t>(actor.behaviorClock*7.0f));actor.equipmentSequence=style<.20f?0:style<.65f?1:2;if(actor.equipmentSequence==1)actor.input.equipment=true;else if(actor.equipmentSequence==2)actor.equipmentRunTime=.32f;}
    }
    if(actor.equipmentRunTime>0.0f){actor.equipmentRunTime=std::max(0.0f,actor.equipmentRunTime-deltaSeconds);actor.input.forward=1.0f;actor.input.right=0.0f;actor.input.sprint=true;actor.input.fire=false;if(actor.equipmentRunTime<=0.0f){actor.input.jump=true;actor.input.equipment=true;}}if(actor.input.equipment||actor.equipmentPoseTime>0.0f){actor.input.forward=actor.input.right=0.0f;actor.input.sprint=actor.input.fire=false;if(actor.equipmentPoseTime>0.0f)actor.input.jump=false;}
    actor.recoveryTime=std::max(0.0f,actor.recoveryTime-deltaSeconds);if(actor.recoveryTime>0){actor.input.forward=1.0f;actor.input.right=0.0f;actor.input.sprint=false;actor.input.fire=false;}if(std::abs(actor.input.forward)+std::abs(actor.input.right)>0.2f){actor.movementIntentTime=0.22f;actor.sprintIntent=actor.input.sprint;}
}


inline void step(Actor& actor,float deltaSeconds,float moveScale=1.0f,bool useCourseCollision=true){
    actor.fixedPresentationThisUpdate=true;
    if(!actor.physicsPresentationValid){actor.previousPhysicsPosition=actor.position;actor.physicsPresentationValid=true;}
    const auto frameStart=actor.position;
    float simulatedDelta=0;
    actor.physicsDeltaThisUpdate=0;
    const float forwardScale=actor.input.forward<0?iw::kBackSpeedScale:1.0f;
    const float sideScale=actor.input.sprint?iw::kSprintStrafeSpeedScale:iw::kStrafeSpeedScale;
    const float stanceScale=iw::stanceSpeedScale(actor.stance),speedScale=(actor.input.sprint?iw::kSprintScale:stanceScale)*moveScale;
    scene::Vec3 wishLocal{actor.input.forward*forwardScale*speedScale,-actor.input.right*sideScale*speedScale,0};if(scene::length(wishLocal)>speedScale)wishLocal=scene::normalize(wishLocal)*speedScale;
    const float cy=std::cos(actor.yaw),sy=std::sin(actor.yaw);const scene::Vec3 wishVelocity{(wishLocal.x*cy-wishLocal.y*sy)*iw::kRunSpeed,(wishLocal.x*sy+wishLocal.y*cy)*iw::kRunSpeed,0};
    actor.fixedAccumulator=std::min(actor.fixedAccumulator+std::min(deltaSeconds,0.05f),0.1f);constexpr float fixedStep=1.0f/125.0f;
    while(actor.fixedAccumulator>=fixedStep){simulatedDelta+=fixedStep;actor.fixedAccumulator-=fixedStep;const auto oldPosition=actor.position;actor.previousPhysicsPosition=oldPosition;if(actor.grounded){const float speed=std::sqrt(actor.velocity.x*actor.velocity.x+actor.velocity.y*actor.velocity.y);if(speed>0){const float drop=std::max(speed,iw::kStopSpeed)*iw::kFriction*fixedStep,newSpeed=std::max(0.0f,speed-drop);if(newSpeed!=speed){actor.velocity.x*=newSpeed/speed;actor.velocity.y*=newSpeed/speed;}}}
        const float wishSpeed=scene::length(wishVelocity);if(wishSpeed>0){const auto direction=wishVelocity/wishSpeed;const float current=scene::dot({actor.velocity.x,actor.velocity.y,0},direction),add=wishSpeed-current;if(add>0){const float acceleration=actor.grounded?iw::groundAcceleration(actor.stance):iw::kAirAccelerate;const float amount=std::min(add,acceleration*fixedStep*wishSpeed);actor.velocity.x+=amount*direction.x;actor.velocity.y+=amount*direction.y;}}
        if(actor.grounded&&actor.spMovingScenario&&!actor.spScenarioInterrupted){actor.velocity.x=wishVelocity.x;actor.velocity.y=wishVelocity.y;}
        actor.velocity.z-=iw::kGravity*fixedStep;
        if(useCourseCollision){
            actor.position=scene::course::constrainMove(oldPosition,actor.position+actor.velocity*fixedStep);const float ground=scene::course::groundHeight(actor.position.x,actor.position.y);const bool snapDown=actor.velocity.z<=0&&actor.position.z-ground<=scene::course::kStepHeight+iw::worldUnits(4.0f);if(actor.position.z<=ground||snapDown){actor.position.z=ground;if(actor.velocity.z<0)actor.velocity.z=0;actor.grounded=true;actor.groundGrace=0.12f;}else actor.grounded=false;
        }else{
            actor.position=actor.position+actor.velocity*fixedStep;
        }
    }
    actor.physicsDeltaThisUpdate=simulatedDelta;
    if(actor.navigation&&navigationBlocked(*actor.navigation,actor.position)){actor.position=frameStart;actor.velocity.x=actor.velocity.y=0;actor.previousPhysicsPosition=actor.position;}
    if(actor.input.jump&&!actor.previousJump&&actor.grounded){actor.velocity.z=std::sqrt(2.0f*iw::kGravity*iw::kJumpHeight);actor.grounded=false;actor.stance=scene::Stance::Stand;}actor.previousJump=actor.input.jump;
    // Render-only frames are not failed movement attempts at slow timescales.
    if(simulatedDelta<=0)return;
    const scene::Vec3 moved=actor.position-frameStart;const float horizontalMoved=std::sqrt(moved.x*moved.x+moved.y*moved.y);
    if(useCourseCollision){
        const scene::Vec3 forwardDir{std::cos(actor.yaw),std::sin(actor.yaw),0};
        const float forwardMoved=scene::dot(moved,forwardDir);
        const float expected=iw::kRunSpeed*0.35f*simulatedDelta*std::max(0.0f,actor.input.forward);
        if(actor.input.forward>0.25f&&actor.grounded&&forwardMoved<expected*0.25f){
            actor.wallContactTime+=simulatedDelta;
        }else if(std::abs(actor.input.forward)+std::abs(actor.input.right)>0.3f&&actor.grounded&&horizontalMoved<iw::worldUnits(0.08f)){
            actor.wallContactTime+=simulatedDelta;
        }else{
            actor.wallContactTime=std::max(0.0f,actor.wallContactTime-simulatedDelta*2.0f);
        }
    }
    if(!actor.initializedStuckPos){
        actor.lastStuckCheckPos=scene::Vec2{actor.position.x,actor.position.y};
        actor.initializedStuckPos=true;
        actor.stuckCheckTimer=0.5f;
        actor.consecutiveStuckEvents=0;
    }
    actor.stuckCheckTimer-=simulatedDelta;
    bool intervalStuck=false;
    if(actor.stuckCheckTimer<=0.0f){
        actor.stuckCheckTimer=0.35f;
        const float dx=actor.position.x-actor.lastStuckCheckPos.x;
        const float dy=actor.position.y-actor.lastStuckCheckPos.y;
        const float distXY=std::sqrt(dx*dx+dy*dy);
        actor.lastStuckCheckPos=scene::Vec2{actor.position.x,actor.position.y};
        const bool hasIntent=(std::abs(actor.input.forward)+std::abs(actor.input.right))>0.25f;
        if(hasIntent&&actor.grounded&&!actor.mantling&&distXY<iw::worldUnits(14.0f)){
            actor.consecutiveStuckEvents++;
            if(actor.consecutiveStuckEvents>=1){
                intervalStuck=true;
            }
        }else{
            actor.consecutiveStuckEvents=0;
        }
    }
    if(std::abs(actor.input.forward)+std::abs(actor.input.right)>0.3f&&actor.grounded&&horizontalMoved<iw::worldUnits(0.08f))actor.stuckTime+=simulatedDelta;else actor.stuckTime=std::max(0.0f,actor.stuckTime-simulatedDelta*3.0f);
    if(actor.wallContactTime>0.22f||actor.stuckTime>0.22f||intervalStuck){
        actor.wallContactTime=0.0f;
        actor.stuckTime=0.0f;
        actor.consecutiveStuckEvents=0;
        actor.blockedHeading=actor.yaw;
        actor.blockedHeadingMemory=3.5f;
        actor.routeDiversionTime=1.6f;
        actor.recoveryTime=0.55f;
        actor.recoveryDirection=-actor.recoveryDirection;
        actor.diversionYaw=wrapAngle(actor.yaw+static_cast<float>(actor.recoveryDirection)*1.57f);
        // Do not snap yaw: natural smooth turn towards diversionYaw
        actor.velocity.x=actor.velocity.y=0;
        if(actor.navigation&&!actor.navigation->nodes.empty()){
            if(!actor.navigationRoute.empty()&&actor.navigationRouteCursor+1<actor.navigationRoute.size()){
                actor.navigationRouteCursor++;
                actor.waypoint=actor.navigationRoute[actor.navigationRouteCursor];
            }else{
                actor.waypoint=nextNavigationNodeAttracted(*actor.navigation,actor.waypoint,actor.id+1,actor.waypoint,actor.lastKnownPlayer,0.8f);
            }
            actor.repathTime=0.0f;
        }
    }
}

inline float horizontalSpeed(const Actor& actor){return std::sqrt(actor.velocity.x*actor.velocity.x+actor.velocity.y*actor.velocity.y);}

} // namespace gameplay::bot
