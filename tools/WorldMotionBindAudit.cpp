#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main

int main(int argc,char**argv){
 if(argc!=3||!glfwInit())return 1;
 glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(640,480,"World motion audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&app=*state;app.window=w;app.defaultSalukiDirectory=argv[1];app.deferSceneUpload=true;
 const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");std::string error;
 if(!app.renderer.initialize(error))return 3;
 const std::vector<std::string>games={"bo","bo2","mw","mw2","mw3","ghosts","aw","iw_sp","mwr","bocw_sp","codm","pointblank"};
 for(const auto&g:games){std::cout<<"scan "<<g<<std::endl;(void)assets::appendScan(app.defaultSalukiDirectory/g,g,app.assetCatalog,error);}
 PointBlankPreparationBatch batch;
 struct Request{std::string game;std::filesystem::path path;};std::vector<Request>requests;
 for(const auto&g:games){
  std::vector<std::filesystem::path>mantles,slides;std::error_code ec;
  for(std::filesystem::recursive_directory_iterator it(app.defaultSalukiDirectory/g/"animations",std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
   if(ec){ec.clear();continue;}if(it->path().extension()!=".cast")continue;
   const auto n=lowerText(it->path().filename().string());if(!n.starts_with("pb_")&&!n.starts_with("mp_"))continue;
   if(n.find("mantle")!=std::string::npos&&n.find("ladder")==std::string::npos)mantles.push_back(it->path());
   if(n.find("slide")!=std::string::npos&&n.find("ladder")==std::string::npos&&n.find("death")==std::string::npos&&n.find("akimbo")==std::string::npos)slides.push_back(it->path());
  }
  for(auto*list:{&mantles,&slides}){std::sort(list->begin(),list->end());if(!list->empty())requests.push_back({g,list->front()});}
 }
 // Exercise the exact default supplemental clips, not just arbitrary matches.
 for(const auto*file:{"mp_mantle_32_over.cast","mp_mantle_56_up.cast","mp_slide.cast"}){
  auto p=app.defaultSalukiDirectory/"ghosts/animations/mp/scripted/generic"/file;if(std::filesystem::is_regular_file(p))requests.push_back({"ghosts",p});
 }
 int failures=0,pairs=0;
 for(const auto&g:games){
  std::vector<size_t>bodies;for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto&a=app.assetCatalog.entries[i];if(a.game!=g||a.role!=assets::Role::PlayerModel)continue;if(g=="aw"&&assets::character::awPart(a.name)!=assets::character::Part::Torso)continue;bodies.push_back(i);}
  if(bodies.empty()){report<<"SKIP "<<g<<" no player models\n";continue;}
  for(size_t variant=0;variant<(g=="aw"?std::min(size_t(3),bodies.size()):size_t(1));++variant){
   const auto index=bodies[(bodies.size()-1)*variant/(g=="aw"?2:1)];const auto&asset=app.assetCatalog.entries[index];
   auto original=scene::buildScene(cast::Document::load(asset.path),false);for(auto part:characterAssemblyParts(app,asset))scene::appendRigModel(pointBlankReferenceDocument(app.assetCatalog.entries[part].path),original,app.assetCatalog.entries[part].name);scene::refreshCharacterBounds(original);
   if(!app.renderer.loadScene(original,error)){report<<"FAIL render "<<g<<" "<<error<<'\n';++failures;continue;}
   app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.12f,.13f,.15f,1});app.classWorldModelAsset=index;
   for(const auto&r:requests){
    auto actor=original;const auto doc=pointBlankReferenceDocument(r.path);
    if(!appendPointBlankWorldAnimation(app,doc,actor,r.game,g))scene::appendAnimations(doc,actor);
    if(actor.animations.empty()||actor.animations[0].tracks.empty()){report<<"FAIL missing "<<g<<" <- "<<r.path<<'\n';++failures;continue;}
    bool finite=true;float maxBoneLengthError=0,maxStep=0;std::vector<scene::Mat4>previous;const auto duration=actor.animations[0].durationFrames;
    for(int sample=0;sample<=60;++sample){const auto pose=actor.samplePose(0,duration*sample/60.f);
     for(size_t b=0;b<pose.size();++b){for(auto v:pose[b].v)finite&=std::isfinite(v);const auto&bone=actor.skeleton.bones[b];const auto name=scene::pointblank::worldSemantic(bone.name);if(bone.parent>=0&&(name.starts_with("j_elbow")||name.starts_with("j_wrist")||name.starts_with("j_knee")||name.starts_with("j_ankle"))){
      const auto p=bone.parent;const float length=scene::length(scene::transformPoint(pose[b],{})-scene::transformPoint(pose[p],{}));const float bind=scene::length(scene::transformPoint(bone.restGlobal,{})-scene::transformPoint(actor.skeleton.bones[p].restGlobal,{}));maxBoneLengthError=std::max(maxBoneLengthError,std::abs(length-bind));
     }if(!previous.empty())maxStep=std::max(maxStep,scene::length(scene::transformPoint(pose[b],{})-scene::transformPoint(previous[b],{})));}previous=pose;
    }
    report<<"PAIR "<<g<<" "<<asset.name<<" <- "<<r.game<<" "<<r.path.filename().string()<<" adapter="<<bool(actor.animations[0].coldWarWorldPose)<<" finite="<<finite<<" limbLengthError="<<maxBoneLengthError<<" maxStep="<<maxStep<<'\n';report.flush();++pairs;
    // Native clips can author segment translation; only converted clips
    // promise to retain target segment lengths.
    if(!finite||(actor.animations[0].coldWarWorldPose&&maxBoneLengthError>2.f))++failures;
    if(r.game=="bocw_sp"||(r.game=="ghosts"&&(r.path.filename()=="mp_mantle_32_over.cast"||r.path.filename()=="mp_slide.cast"))){
     constexpr int width=400,height=400;std::vector<std::uint8_t>strip(width*5*height*4);
     for(int k=0;k<5;++k){auto pose=actor.samplePose(0,duration*k/4.f);if(auto root=actor.skeleton.boneByCanonicalName.find("tag_origin");root!=actor.skeleton.boneByCanonicalName.end()){const auto delta=scene::transformPoint(pose[root->second],{})-scene::transformPoint(actor.skeleton.bones[root->second].restGlobal,{});for(auto&m:pose){m.v[12]-=delta.x;m.v[13]-=delta.y;m.v[14]-=delta.z;}}
      const auto vp=scene::perspective(45*scene::kPi/180,1,1,4000)*scene::lookAt({270,-290,175},{0,0,90},{0,0,1});app.renderer.render(actor,pose,vp,width,height,false,false,false);std::vector<std::uint8_t>pixels;if(!app.renderer.readColorRgba(pixels,error))return 4;
      for(int y=0;y<height;++y)std::copy_n(pixels.data()+y*width*4,width*4,strip.data()+(y*width*5+k*width)*4);
     }(void)app.renderer.savePixelsPng(out/(g+"_"+std::to_string(variant)+"_"+r.path.stem().string()+".png"),width*5,height,strip,error);
    }
   }
  }
 }
 // Exercise presentation through the actual player evaluator. Cardinal
 // forward clips must retain W+A/D diagonal facing for all grounded stances.
 app.hiddenWorldActor.emplace();auto&body=*app.hiddenWorldActor;
 scene::Bone root;root.name="tag_origin";root.parent=-1;root.restGlobal=root.inverseBind=scene::Mat4::identity();body.skeleton.bones.push_back(root);body.skeleton.boneByCanonicalName[root.name]=0;
 app.classWorldModelAsset=SIZE_MAX;app.selectedWeaponAsset=SIZE_MAX;app.actorMode=true;app.actorGrounded=true;app.actorMantling=app.actorSliding=false;app.actorOnLadder=false;app.cameraPitch=0;
 for(const auto stance:{scene::Stance::Stand,scene::Stance::Crouch,scene::Stance::Prone})for(const bool sprint:{false,true})for(const float side:{-100.f,100.f}){
  body.animations.clear();scene::Animation clip;clip.sourceName="pb_run_forward.cast";clip.domain=scene::AnimationDomain::PlayerBody;clip.motion=sprint?scene::MotionRole::Sprint:scene::MotionRole::Run;clip.direction=scene::Direction::Forward;clip.stance=stance;clip.durationFrames=30;clip.framerate=30;scene::Track track;track.boneIndex=0;track.property=scene::TrackProperty::Rotation;track.frames={0};track.rotationValues={{0,0,0,1}};clip.tracks.push_back(track);body.animations.push_back(clip);
  app.worldSelectionKey.reset();app.actorWorldFacingOffset=0;app.actorWorldPresentationTime=-1;app.actorCurrLocoClip=app.actorPrevLocoClip=SIZE_MAX;app.gameplayClock=0;app.actorYaw=0;app.actorSprinting=sprint;app.gameplayStance=stance;app.actorVelocity=app.actorWishVelocity={100,side,0};app.actorMoveInputForward=1;app.actorMoveInputSide=side>0?-1:1;
  for(int i=0;i<90;++i){app.gameplayClock=i/60.f;(void)evaluateHiddenWorldActorPose(app);}
  const float expected=std::atan2(side,100.f);if(std::abs(app.actorWorldFacingOffset-expected)>.001f){++failures;report<<"FAIL movement facing "<<app.actorWorldFacingOffset<<" expected "<<expected<<'\n';}
 }
 report<<"movement facing cases=12\n";
 report<<"pairs="<<pairs<<" failures="<<failures<<'\n';return failures?5:0;
}
