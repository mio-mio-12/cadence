#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv) try {
 const auto output=std::filesystem::absolute(argc>1?argv[1]:"diagnostics/bot-playback-audit");
 const bool pair=argc>2&&std::string(argv[2])=="pair";
 const char* requestedDeath=std::getenv("CADENCE_PAIR_DEATH");
 const bool deathAudit=requestedDeath&&*requestedDeath;
 const bool reloadAudit=std::getenv("CADENCE_PAIR_RELOAD")!=nullptr;
 const bool movingReloadAudit=std::getenv("CADENCE_PAIR_MOVING_RELOAD")!=nullptr;
 if(movingReloadAudit&&!reloadAudit)throw std::runtime_error("Moving reload audit requires CADENCE_PAIR_RELOAD");
 if(reloadAudit&&(!pair||deathAudit))throw std::runtime_error("CADENCE_PAIR_RELOAD requires a living pair audit");
 if(deathAudit&&!pair)throw std::runtime_error("CADENCE_PAIR_DEATH requires pair mode");
 const bool closePair=pair&&std::getenv("CADENCE_PAIR_CLOSE");
 const bool sidePair=pair&&std::getenv("CADENCE_PAIR_SIDE");
 const bool followPair=pair&&std::getenv("CADENCE_PAIR_FOLLOW");
 if(pair&&argc!=6){std::cerr<<"Usage: OUTPUT pair MODEL_GAME DRIVER_GAME FIXTURES_TSV\n";return 2;}
 if(!glfwInit())return 1;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(800,600,"Bot playback audit",nullptr,nullptr);glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& a=*state;a.window=w;a.defaultSalukiDirectory="exported_files";std::string error;
 if(!a.renderer.initialize(error))return 2;
 const bool imported=argc>2&&std::string(argv[2]).starts_with("imported-");
 const bool nativeImported=imported&&std::string(argv[2])=="imported-native";
 const std::vector<std::string> games=pair?std::vector<std::string>{argv[3]}:imported?std::vector<std::string>{"eldewrito","cs1.6","cz","css","csnz","cso2"}:std::vector<std::string>{"bo2","pointblank","mw3"};
 auto scanGames=games;if(imported)scanGames.push_back("bo2");
 if(pair)for(const std::string game:{std::string(argv[4]),std::string("bo2")})if(std::find(scanGames.begin(),scanGames.end(),game)==scanGames.end())scanGames.push_back(game);
 for(const auto& g:scanGames)if(!assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error))return 3;
 const bool fixedPb=argc>2&&std::string(argv[2])=="pb-fixed";
 if(fixedPb){const std::set<std::string> names={"playermode_Bella_fb","playermode_Hide_Recon_fb","playermode_Hide_Soccer_fb","playermode_Hide_Kopassus_fb","playermode_REBEL_Soccer_fb"};std::erase_if(a.assetCatalog.entries,[&](const auto& entry){return entry.game=="pointblank"&&entry.name.starts_with("playermode_")&&!names.contains(entry.name);});}
 for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name=="t6_wpn_ar_an94_view_LOD0")a.selectedWeaponAsset=i;
 a.enemyBotCount=5;a.botTeamSide=3;a.botAnimationGame="bo2+bo2_sp";a.playing=true;a.actorPosition={0,0,0};a.botSystemMode=1;a.showBotAnimationClips=true;
 if(nativeImported)a.botAnimationGame.clear();
 size_t pairBody=SIZE_MAX;
 if(pair){
  std::ifstream fixtures(argv[5]);if(!fixtures)throw std::runtime_error("Cannot open frozen fixtures TSV");
  std::string line;size_t pairWeapon=SIZE_MAX;
  while(std::getline(fixtures,line)){
   const auto first=line.find('\t'),second=line.find('\t',first==std::string::npos?0:first+1);if(first==std::string::npos||second==std::string::npos)continue;
   const auto game=line.substr(0,first),role=line.substr(first+1,second-first-1);auto stored=line.substr(second+1);if(!stored.empty()&&stored.back()=='\r')stored.pop_back();
   const bool body=game==argv[3]&&role=="body",weapon=game==argv[4]&&role=="weapon";if(!body&&!weapon)continue;
   for(size_t i=0;i<a.assetCatalog.entries.size();++i){const auto& asset=a.assetCatalog.entries[i];if(asset.game==game&&asset.path.lexically_normal()==std::filesystem::path(stored).lexically_normal()&&asset.role==(body?assets::Role::PlayerModel:assets::Role::ViewWeapon)){if(body)pairBody=i;else pairWeapon=i;break;}}
  }
  if(pairBody==SIZE_MAX||pairWeapon==SIZE_MAX)throw std::runtime_error("Exact frozen body/driver weapon unavailable; no substitution allowed");
  a.enemyBotCount=1;a.botTeamSide=0;a.botFactionModelVariety=false;a.botUseSalukiWeaponPool=false;a.selectedWeaponAsset=pairWeapon;a.botAnimationGame=argv[4];
  a.botCombatDamage=false;a.botAutoRespawn=false;a.botTacticalReposition=0;a.botSpCombatMobilityChance=0;
  a.botSpAlertChance=a.botSpPainChance=a.botSpCrouchChance=a.botSpIdleChance=a.botSpPatrolChance=0;
  a.botSpSlipChance=a.botSpMovingPainChance=a.botSpAmbientStumbleChance=0;a.botAllowEquipment=false;
  a.botSpawnPoints={{-350,0,0}};a.actorPosition={0,0,0};
 }
 std::filesystem::create_directories(output);std::ofstream log(output/"results.txt");
 for(const auto& g:games){
  if(fixedPb&&std::string(g)!="pointblank")continue;
  a.botGame=g;a.botModelAsset=pair?pairBody:static_cast<std::size_t>(-1);rebuildBotActors(a);log<<"BUILD "<<g<<" "<<a.status<<std::endl;if(!a.botActorScene||a.bots.empty())return 4;
  if(pair){a.bots[0].yaw=0;log<<"PAIR body="<<a.assetCatalog.entries[pairBody].path<<" driver="<<argv[4]<<" weapon="<<a.assetCatalog.entries[a.selectedWeaponAsset].path<<" fixtures="<<argv[5]<<"\n";}
  auto&s=*a.botActorScene;log<<"BONES "<<s.skeleton.bones.size()<<" CLIPS "<<s.animations.size()<<std::endl;
  if(pair&&std::getenv("CADENCE_PAIR_DEATH_POOL")){
   log<<"DEATH_POOL "<<a.botDeathChoices.size()<<'\n';
   for(bool moving:{false,true})for(int mode:{0,1}){
    a.activeBotSystemMode=mode;a.botDeathHistory={};
    auto bot=a.bots.front();bot.grounded=true;bot.spPolicyInitialized=false;
    bot.stance=scene::Stance::Stand;bot.input={};bot.input.forward=moving?1.f:0.f;
    bot.velocity=moving?scene::Vec3{200,0,0}:scene::Vec3{};
    std::map<std::size_t,int> counts;
    for(unsigned i=0;i<300;++i){
     const auto clip=botDeathAnimation(s,bot,scene::WeaponClass::Rifle,i,&a);
     if(!clip)throw std::runtime_error("Death rotation returned no clip");
     ++counts[*clip];
    }
    int highest=0;
    for(const auto& [clip,count]:counts){
     highest=std::max(highest,count);
     log<<count<<" "<<s.animations[clip].sourceName<<'\n';
     const auto pose=s.samplePose(clip,s.animations[clip].durationFrames*.5f);
     for(const auto& bone:pose)for(float value:bone.v)if(!std::isfinite(value))
      throw std::runtime_error("Selected death returned an invalid pose");
    }
    log<<"DEATH_DISTRIBUTION moving="<<moving<<" mode="<<mode<<" distinct="<<counts.size()<<" most_used="<<highest<<"/300"<<std::endl;
    if(counts.size()<5||highest>=90)throw std::runtime_error("Death rotation still favors too few clips");
   }
   return 0;
  }
  if(reloadAudit){a.bots[0].ammo=movingReloadAudit?10000:1;log<<"RELOAD_GATE "<<(movingReloadAudit?"deplete only after observed movement":"initial magazine=1")<<"; ordinary AI owns reload selection and timing\n";}
  for(const auto& warning:s.warnings)log<<"ASSEMBLY WARNING "<<warning<<'\n';
  log<<"ATTACHMENTS "<<s.attachments.size()<<'\n';
  for(const auto& attachment:s.attachments){log<<"ATTACHMENT "<<attachment.name<<" meshes="<<attachment.meshCount<<" bone="<<attachment.boneIndex<<'\n';
   scene::Vec3 lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};size_t vertices=0;
   for(size_t m=attachment.firstMesh;m<attachment.firstMesh+attachment.meshCount&&m<s.meshes.size();++m)for(const auto& v:s.meshes[m].vertices){const auto p=scene::transformPoint(s.meshes[m].modelTransform,v.position);lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};++vertices;}
   log<<"ATTACHMENT LOCAL BOUNDS vertices="<<vertices<<" min="<<lo.x<<','<<lo.y<<','<<lo.z<<" max="<<hi.x<<','<<hi.y<<','<<hi.z<<'\n';}
  size_t deathIndex=SIZE_MAX;
  if(deathAudit){
   for(size_t i=0;i<s.animations.size();++i){const auto& c=s.animations[i];if(c.sourceGame==argv[4]&&cadence::AnimationSet::file(c.sourceName)==cadence::AnimationSet::file(requestedDeath)&&c.action==scene::ActionRole::Death&&!c.tracks.empty()){if(deathIndex!=SIZE_MAX)throw std::runtime_error("Ambiguous exact death clip");deathIndex=i;}}
   if(deathIndex==SIZE_MAX)throw std::runtime_error("Requested exact donor death is unavailable");
   a.animationSetForBots=true;a.animationSet.curatedDeaths=true;a.animationSet.set(argv[4],scene::ActionRole::Death,s.animations[deathIndex].sourceName,true);
   applyBotAnimationSet(a,s,argv[3]);
   a.botSetRuntime.refresh(s,a.animationSet,true);
   a.botDeathChoices=a.botSetRuntime.deaths(s,a.animationSet,true);
   if(a.botDeathChoices.size()!=1||a.botDeathChoices.front().animation!=deathIndex)throw std::runtime_error("Production curated death pool did not retain only exact requested clip");
   a.weaponTiming.damage=10000;a.actorMode=true;a.actorThirdPerson=false;a.freeCameraActive=false;
   log<<"DEATH_GATE flat built-in course only; exact="<<s.animations[deathIndex].sourceName<<" lethal player shot scheduled frame90; no loaded-map/slope/airborne certification\n";
  }
  if(!pair)for(int weapon=0;weapon<=int(scene::WeaponClass::RC);++weapon)for(auto m:{scene::MotionRole::Idle,scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint}){
   scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=m;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass(weapon);q.direction=scene::Direction::Forward;
   auto c=botLocomotionAnimation(s,q,&a);if(!c){log<<"MISSING "<<int(m)<<std::endl;continue;}const auto&clip=s.animations[*c];auto p=s.samplePose(*c,0),r=s.samplePose(*c,clip.durationFrames*.4f);float difference=0;
   for(size_t b=0;b<p.size();++b)difference=std::max(difference,scene::length(scene::transformPoint(p[b],{})-scene::transformPoint(r[b],{})));
   log<<"SELECT "<<weapon<<" "<<int(m)<<" "<<clip.sourceName<<" frames="<<clip.durationFrames<<" looping="<<clip.looping<<" motion="<<difference<<std::endl;
   if(!isGroundWorldMotion(clip.motion)||clip.contextual||clip.action!=scene::ActionRole::None)return 5;
  }
  const scene::WeaponClass classes[]={scene::WeaponClass::Rifle,scene::WeaponClass::Knife,scene::WeaponClass::DualWield,scene::WeaponClass::Equipment,scene::WeaponClass::Any};
  if(!pair)for(size_t i=0;i<a.bots.size();++i)a.bots[i].weaponClass=classes[i%5];
  // Preserve the production evaluated matrices, then verify disk round-trip.
  take::Take recorded;recorded.sampleRate=60;recorded.boneCount=1;recorded.botBoneCount=static_cast<unsigned>(s.skeleton.bones.size());recorded.botCount=static_cast<unsigned>(a.bots.size());
  captureTakeBotManifest(a);recorded.botActor=a.recordedTake.botActor;
  std::ofstream frames;if(pair){frames.open(output/"continuous.csv");frames<<"frame,time,requested_phase,current_clip,current_frame,previous_clip,previous_frame,blend_elapsed,action_clip,action_frame,action_weight,grounded,x,y,z,presentation_x,presentation_y,presentation_z,ground_z,speed,finite,fire,reload_started,yaw\n";}
  bool initialIdle=false,moved=false,finalIdle=false,reloadDepleted=false;unsigned transitions=0,fires=0,reloads=0,movingReloadFrames=0;size_t previousClip=SIZE_MAX;
  bool observedDeath=false;std::ofstream deathFrames;
  if(deathAudit){deathFrames.open(output/"death-grounding.csv");deathFrames<<"frame,alive,health,clip,clip_frame,grounded,controller_z,course_z,foot_bones,minimum_foot_ground_gap,body_meshes,body_vertices,body_min_z,body_max_z,body_min_course_gap\n";}
  for(int f=0;f<360;++f){
   // Only change the ordinary AI's mobility opportunity. Its policy chooses
   // supported lateral goals and performs all steering, physics and blending.
   if(pair)a.botSpCombatMobilityChance=!deathAudit&&f>=90&&f<210?1.f:0.f;
   if(movingReloadAudit&&!reloadDepleted&&f>=90&&gameplay::bot::horizontalSpeed(a.bots[0])>30){
    a.bots[0].ammo=0;reloadDepleted=true;log<<"MOVING_RELOAD depleted magazine at frame "<<f<<'\n';
   }
   if(deathAudit&&f==90){
    if(!a.bots[0].alive)throw std::runtime_error("Bot died before scheduled lethal shot");
    const auto origin=a.actorPosition+scene::Vec3{0,0,a.actorViewHeight};
    const auto aim=a.bots[0].position+scene::Vec3{0,0,a.botHeadHeight*a.botModelScale*.58f}-origin;
    a.actorYaw=std::atan2(aim.y,aim.x);a.cameraPitch=std::atan2(aim.z,std::sqrt(aim.x*aim.x+aim.y*aim.y));++a.recoilSequence;
   }
   updateBotActors(a,1.f/60);if(f%60==0){for(size_t i=0;i<a.bots.size();++i){const auto&b=a.bots[i];log<<"LIVE "<<f<<" "<<i<<" clip="<<b.animation<<" frame="<<b.animationFrame<<" speed="<<gameplay::bot::horizontalSpeed(b)<<" ground="<<b.grounded<<"\n"<<a.botAnimationLabels[i]<<std::endl;}}
   if(pair){auto& b=a.bots[0];const auto p=gameplay::bot::presentationPosition(b);bool finite=true;for(const auto& matrix:a.botActorPoses[0])for(float value:matrix.v)finite=finite&&std::isfinite(value);
    const auto clipName=[&](size_t index){return index<s.animations.size()?s.animations[index].sourceName:std::string("<none>");};const float speed=gameplay::bot::horizontalSpeed(b);
    initialIdle=initialIdle||(f<90&&speed<3);moved=moved||(f>=90&&speed>30);finalIdle=finalIdle||(f>=210&&moved&&speed<3);
    if(previousClip!=SIZE_MAX&&previousClip!=b.animation)++transitions;previousClip=b.animation;fires+=b.input.fire;reloads+=b.reloadStarted;
    if(speed>30&&b.actionBlendWeight>.1f&&b.actionAnimation<s.animations.size()&&s.animations[b.actionAnimation].action==scene::ActionRole::Reload)++movingReloadFrames;
    frames<<f<<','<<f/60.f<<','<<(f<90?"settle":f<210?"combat_move":"settle_after_move")<<','<<std::quoted(clipName(b.animation))<<','<<b.animationFrame<<','<<std::quoted(clipName(b.previousAnimation))<<','<<b.previousAnimationFrame<<','<<b.animationBlendElapsed<<','<<std::quoted(clipName(b.actionAnimation))<<','<<b.actionFrame<<','<<b.actionBlendWeight<<','<<b.grounded<<','<<b.position.x<<','<<b.position.y<<','<<b.position.z<<','<<p.x<<','<<p.y<<','<<p.z<<','<<scene::course::groundHeight(b.position.x,b.position.y)<<','<<speed<<','<<finite<<','<<b.input.fire<<','<<b.reloadStarted<<','<<b.yaw<<'\n';
    if(!finite){log<<"NONFINITE live pose at frame "<<f<<std::endl;return 9;}
   }
   if(deathAudit){
    const auto& b=a.bots[0];
    if(f>=90){if(b.alive||b.animation!=deathIndex)throw std::runtime_error("Lethal production shot did not select exact requested death");observedDeath=true;}
    float gap=std::numeric_limits<float>::infinity();size_t footBones=0;
    for(size_t i=0;i<s.skeleton.bones.size();++i){const auto name=lowerText(s.skeleton.bones[i].name);if(name.find("foot")==std::string::npos&&name.find("ankle")==std::string::npos&&name.find("toe")==std::string::npos)continue;
     const auto p=scene::transformPoint(a.botActorPoses[0][i],{});gap=std::min(gap,p.z-scene::course::groundHeight(p.x,p.y));++footBones;}
    // Evaluate actual indexed body vertices, not unused rig helpers or attached weapons.
    std::vector<scene::Mat4> skin;skin.reserve(s.skeleton.bones.size());
    for(size_t i=0;i<s.skeleton.bones.size();++i)skin.push_back(a.botActorPoses[0][i]*s.skeleton.bones[i].inverseBind);
    float minZ=std::numeric_limits<float>::infinity(),maxZ=-minZ,meshGap=minZ;size_t bodyMeshes=0,bodyVertices=0;
    for(const auto& mesh:s.meshes){
     if(!mesh.skinned||mesh.attachmentIndex>=0||mesh.viewmodelWeapon||(mesh.actorVariant>=0&&mesh.actorVariant!=b.modelVariant))continue;
     ++bodyMeshes;std::vector<bool> used(mesh.vertices.size());
     for(const auto index:mesh.indices){if(index>=mesh.vertices.size())throw std::runtime_error("Invalid body mesh index");if(used[index])continue;used[index]=true;
      const auto& vertex=mesh.vertices[index];scene::Vec3 p{};float sum=0;
      for(size_t k=0;k<vertex.weights.size();++k)if(vertex.weights[k]>0){if(vertex.bones[k]>=skin.size())throw std::runtime_error("Invalid body skin palette index");p+=scene::transformPoint(skin[vertex.bones[k]],vertex.position)*vertex.weights[k];sum+=vertex.weights[k];}
      if(sum<=0)throw std::runtime_error("Unweighted indexed skinned body vertex");
      // Affine uModel * weighted skin: divide homogeneous weight sum for world-space distances.
      p=scene::transformPoint(mesh.modelTransform,p/sum);
      if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Nonfinite skinned body vertex");
      minZ=std::min(minZ,p.z);maxZ=std::max(maxZ,p.z);meshGap=std::min(meshGap,p.z-scene::course::groundHeight(p.x,p.y));++bodyVertices;
     }
    }
    if(!bodyVertices)throw std::runtime_error("No indexed skinned body vertices for active variant");
    deathFrames<<f<<','<<b.alive<<','<<b.health<<','<<std::quoted(b.animation<s.animations.size()?s.animations[b.animation].sourceName:std::string("<none>"))<<','<<b.animationFrame<<','<<b.grounded<<','<<b.position.z<<','<<scene::course::groundHeight(b.position.x,b.position.y)<<','<<footBones<<','<<gap<<','<<bodyMeshes<<','<<bodyVertices<<','<<minZ<<','<<maxZ<<','<<meshGap<<'\n';
   }
   take::Sample sample;sample.time=f/60.f;sample.pose={scene::Mat4::identity()};
   for(size_t i=0;i<a.bots.size();++i){const auto&b=a.bots[i];take::RecordedActorState state;state.id=b.id;state.modelVariant=b.modelVariant;state.pose=a.botActorPoses[i];if(deathAudit){state.alive=b.alive;state.health=b.health;}if(b.animation<s.animations.size())state.primaryClip=s.animations[b.animation].sourceName;state.primaryFrame=b.animationFrame;sample.bots.push_back(std::move(state));}
   recorded.samples.push_back(std::move(sample));
   if(pair?(f%4==0||f==359):(f==65||f==80)){
    auto poses=a.botActorPoses;std::vector<int> variants;
    for(size_t i=0;i<poses.size();++i){variants.push_back(a.bots[i].modelVariant);if(!pair){auto p=gameplay::bot::presentationPosition(a.bots[i]);for(auto&m:poses[i]){m.v[12]+=float(i)*120-p.x;m.v[13]-=p.y;m.v[14]-=p.z;}}}
    scene::CastScene empty;scene::Vec3 eye=sidePair?scene::Vec3{-350,-700,80}:closePair?scene::Vec3{-900,-900,350}:pair?scene::Vec3{-1100,-1250,550}:scene::Vec3{240,-650,230};
    const auto followPosition=gameplay::bot::presentationPosition(a.bots.front());
    if(followPair)eye={followPosition.x-250,followPosition.y-380,followPosition.z+140};
    a.renderer.setCameraPosition(eye);a.renderer.setDebugView(1);
    auto vp=scene::perspective((sidePair?45.f:closePair?40.f:60.f)*scene::kPi/180,4.f/3,1,4000)*scene::lookAt(eye,sidePair?scene::Vec3{-350,0,65}:closePair?scene::Vec3{-350,-250,80}:pair?scene::Vec3{-350,0,80}:scene::Vec3{240,0,80},{0,0,1});
    if(followPair)vp=scene::perspective(40.f*scene::kPi/180,4.f/3,1,4000)*scene::lookAt(eye,{followPosition.x,followPosition.y,followPosition.z+65},{0,0,1});
    // Hide only the diagnostic course drawing; physics and recorded poses remain unchanged.
    a.renderer.render(empty,{},vp,1000,750,pair&&!followPair,false,false,&s,&poses,&variants,nullptr,nullptr,false);
    if(!a.renderer.saveColorPng(output/(std::string(g)+"_"+std::to_string(f)+".png"),error))return 10;
   }
  }
  if(pair)log<<"CONTINUOUS OBSERVED initial_idle="<<initialIdle<<" movement="<<moved<<" final_idle="<<finalIdle<<" clip_transitions="<<transitions<<" fire_events="<<fires<<" reload_events="<<reloads<<" (unobserved categories are NOT certified; grounding requires capture review)\n";
  if(reloadAudit&&reloads==0)throw std::runtime_error("No production reload observed from low magazine");
  if(movingReloadAudit){log<<"MOVING_RELOAD overlapping frames="<<movingReloadFrames<<'\n';if(!reloadDepleted||movingReloadFrames<5)throw std::runtime_error("Insufficient production movement/reload overlap");}
  if(deathAudit){if(!observedDeath)throw std::runtime_error("No live death transition observed");log<<"DEATH observed exact lethal-shot transition; foot gaps are bone proxies, not mesh-contact certification\n";}
  const auto path=output/(std::string(g)+".c_dm");take::Take loaded;
  if(!take::save(recorded,path,error)||!take::load(path,loaded,error)){log<<error;return 6;}
  for(size_t f=0;f<recorded.samples.size();++f)for(size_t i=0;i<recorded.botCount;++i){const auto&before=recorded.samples[f].bots[i];const auto&after=loaded.samples[f].bots[i];if(before.primaryClip!=after.primaryClip||before.modelVariant!=after.modelVariant)return 7;if(deathAudit&&(before.alive!=after.alive||before.health!=after.health||before.primaryFrame!=after.primaryFrame))throw std::runtime_error("Death replay state/identity mismatch");for(size_t b=0;b<before.pose.size();++b)for(int k=0;k<16;++k)if(std::abs(before.pose[b].v[k]-after.pose[b].v[k])>.001f)return 8;}
  const auto fractional=loaded.interpolatedSample(1.075f,{nullptr,nullptr,&s.skeleton});
  for(const auto&bot:fractional.bots)for(const auto&m:bot.pose)for(float v:m.v)if(!std::isfinite(v))return 9;
  log<<"REPLAY exact recorded poses/clip identities and finite fractional playback PASS"<<std::endl;
  // Recreate geometry/skeleton from the saved manifest, not just matrices.
  // Identical pose arrays can still render stretched on a mismatched rig.
  if(argc>2&&std::string(argv[2])=="skip-restore")continue;
  log<<"RESTORE copying source geometry"<<std::endl;
  const auto sourceSkeleton=s.skeleton;const auto sourceMeshes=s.meshes;
  log<<"RESTORE invoking production reconstruction"<<std::endl;
  if(!restoreTakeBotActor(a,loaded.botActor,loaded.botBoneCount,error)||!a.botActorScene){log<<"RESTORE FAILED "<<error<<std::endl;return 11;}
  const auto& rebuilt=*a.botActorScene;
  if(rebuilt.skeleton.bones.size()!=sourceSkeleton.bones.size()||rebuilt.meshes.size()!=sourceMeshes.size()){log<<"RESTORE layout mismatch"<<std::endl;return 12;}
  for(size_t b=0;b<sourceSkeleton.bones.size();++b){
   if(rebuilt.skeleton.bones[b].name!=sourceSkeleton.bones[b].name)return 13;
   for(int k=0;k<16;++k)if(std::abs(rebuilt.skeleton.bones[b].inverseBind.v[k]-sourceSkeleton.bones[b].inverseBind.v[k])>.001f)return 14;
  }
  // Live assembly interleaves weapons with bodies; restoration appends weapons last.
  // Compare a one-to-one geometry multiset rather than incidental draw order.
  std::vector<bool> matched(rebuilt.meshes.size());
  for(const auto& before:sourceMeshes){bool found=false;
   for(size_t m=0;m<rebuilt.meshes.size()&&!found;++m){const auto& after=rebuilt.meshes[m];
    if(matched[m]||before.actorVariant!=after.actorVariant||before.vertices.size()!=after.vertices.size()||before.indices!=after.indices)continue;
    bool equal=true;for(size_t v=0;v<before.vertices.size()&&equal;++v)equal=scene::length(before.vertices[v].position-after.vertices[v].position)<=.001f&&before.vertices[v].bones==after.vertices[v].bones&&before.vertices[v].weights==after.vertices[v].weights;
    if(equal){matched[m]=true;found=true;}
   }
   if(!found){log<<"RESTORE no matching geometry: "<<before.name<<" variant "<<before.actorVariant<<std::endl;return 15;}
  }
  log<<"REPLAY restored manifest geometry, variant assignment, bone order, weights and inverse binds PASS"<<std::endl;
  if(deathAudit){
   // Render disk-restored death states with the same fixed camera/course context.
   // Also exercise a fractional seek around the live->death blend, twice.
   std::vector<std::uint8_t> firstSeek;
   for(float time:{88.f/60,92.f/60,100.f/60,140.f/60,220.f/60,359.f/60,1.575f,1.575f}){
    const auto sample=loaded.interpolatedSample(time,{nullptr,nullptr,&rebuilt.skeleton});
    std::vector<std::vector<scene::Mat4>> poses;std::vector<int> variants;
    for(const auto& b:sample.bots){poses.push_back(b.pose);variants.push_back(b.modelVariant);for(const auto& m:b.pose)for(float value:m.v)if(!std::isfinite(value))throw std::runtime_error("Nonfinite death replay seek");}
    scene::CastScene empty;const scene::Vec3 eye=sidePair?scene::Vec3{-350,-700,80}:closePair?scene::Vec3{-900,-900,350}:scene::Vec3{-1100,-1250,550};a.renderer.setCameraPosition(eye);
    const auto vp=scene::perspective((sidePair?45.f:closePair?40.f:60.f)*scene::kPi/180,4.f/3,1,4000)*scene::lookAt(eye,sidePair?scene::Vec3{-350,0,65}:closePair?scene::Vec3{-350,-250,80}:scene::Vec3{-350,0,80},{0,0,1});
    a.renderer.render(empty,{},vp,1000,750,true,false,false,&rebuilt,&poses,&variants,nullptr,nullptr,false);
    if(time==1.575f){std::vector<std::uint8_t> pixels;if(!a.renderer.readColorRgba(pixels,error))throw std::runtime_error(error);if(firstSeek.empty())firstSeek=std::move(pixels);else if(firstSeek!=pixels)throw std::runtime_error("Repeated death replay seek changed pixels");}
    if(!a.renderer.saveColorPng(output/("death_replay_"+std::to_string(time)+".png"),error))throw std::runtime_error(error);
   }
   log<<"DEATH disk replay state/clip/pose roundtrip and restored-render seeks completed; visual terrain-contact review remains required\n";
  }
 }
 return 0;
}catch(const std::exception& e){std::cerr<<"AUDIT exception: "<<e.what()<<std::endl;return 20;}
