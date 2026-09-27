#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      return 2;
    const bool produce = std::string(argv[1]) == "produce";
    const auto root = std::filesystem::absolute(argv[2]);
    if (!glfwInit())
      return 2;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto *window =
        glfwCreateWindow(640, 360, "Demo pack replay audit", nullptr, nullptr);
    if (!window)
      return 2;
    glfwMakeContextCurrent(window);
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    auto state = std::make_unique<AppState>();
    auto &app = *state;
    const auto expectPair=[&](const std::filesystem::path& path,const std::string& driver,bool expected){
      if(importedViewPair(app,path,driver)!=expected)throw std::runtime_error("Imported view pair policy: "+path.string()+" -> "+driver);
    };
    for(const auto* path:{"D:/exported_files/bo2/models/hands.cast","D:/EXPORTED_FILES/BO2/models/hands.cast","D:\\exported_files\\bo2\\models\\hands.cast","\\\\?\\D:\\exported_files\\bo2\\models\\hands.cast"})expectPair(path,"csnz",true);
    expectPair("D:/exported_files/bo2/models/hands.cast","bo2",false);
    expectPair("D:/exported_files/bo2/models/hands.cast","cs2",false);
    expectPair("D:/exported_files/cs2/models/hands.cast","csnz",false);
    expectPair("D:/saluki/exported/_files/cs2/models/hands.cast","csnz",false);
    expectPair("D:/saluki/exported/_files/bo2/models/hands.cast","csnz",true);
    expectPair("D:/saluki/exported/unknown/hands.cast","csnz",false);
    expectPair("D:/unknown/hands.cast","csnz",false);
    expectPair("D:/exported_files/css/models/hands.cast","css",true);
    app.assetCatalog.entries.push_back({"D:/unknown/hands.cast","hands","bo2",assets::Role::ViewHands});
    expectPair("D:/unknown/hands.cast","csnz",true);
    app.assetCatalog.entries.back().game="cs2";
    expectPair("D:/unknown/hands.cast","csnz",false);
    app.assetCatalog.entries.clear();
    std::cout<<"Imported hand game classification tolerates relocated path spellings\n";
    const auto geometry=[](const scene::CastScene& rig){
      std::vector<float> values;
      for(const auto& b:rig.skeleton.bones)values.insert(values.end(),b.inverseBind.v.begin(),b.inverseBind.v.end());
      for(const auto& mesh:rig.meshes)for(const auto& v:mesh.vertices){values.push_back(v.position.x);values.push_back(v.position.y);values.push_back(v.position.z);}
      return values;
    };
    app.window = window;
    app.deferSceneUpload = true;
    std::string error;
    if (!app.renderer.initialize(error))
      throw std::runtime_error(error);
    if (produce) {
      app.defaultSalukiDirectory =
          cadence::local_assets::exportPath("");
      for (const auto game : {"bo2", "csnz"})
        if (!assets::appendScan(app.defaultSalukiDirectory / game, game,
                                app.assetCatalog, error))
          throw std::runtime_error(error);
      const auto choose = [&](const std::string &game, assets::Role role,
                              const std::string &part) {
        for (size_t i = 0; i < app.assetCatalog.entries.size(); ++i) {
          const auto &a = app.assetCatalog.entries[i];
          if (a.game == game && a.role == role &&
              a.name.find(part) != std::string::npos)
            return i;
        }
        throw std::runtime_error("Missing audit asset " + game + " " + part);
      };
      app.selectedBaseAsset = choose("bo2", assets::Role::ViewHands, "seal6");
      app.selectedWeaponAsset =
          choose("csnz", assets::Role::ViewWeapon, "as50");
      equipViewWeapon(app, app.selectedWeaponAsset);
      if (app.scene.animations.empty())
        throw std::runtime_error("No audit animations: " + app.status);
      app.recordedTake.boneCount = app.scene.skeleton.bones.size();
      captureTakeActorManifest(app);
      take::Sample sample;
      sample.pose = app.scene.samplePose(app.animationIndex, 0);
      app.recordedTake.samples.push_back(sample);
      cadence::content::Metadata metadata;
      for (const auto &p : cadence::content::constructionDocuments())
        metadata.construction.push_back(cadence::content::utf8(p));
      // Tiny real map with a companion geometry buffer; no user map is
      // modified.
      const auto map = root / "map" / "floor.gltf";
      std::filesystem::create_directories(map.parent_path());
      {
        std::ofstream out(map);
        out << R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"buffers":[{"uri":"floor%20geometry.dat","byteLength":42}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-10,0,-10],"max":[10,0,10]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}]})";
      }
      {
        std::ofstream out(map.parent_path() / "floor geometry.dat", std::ios::binary);
        const float v[]{-10, 0, -10, 10, 0, -10, 0, 0, 10};
        const uint16_t indices[]{0, 2, 1};
        out.write(reinterpret_cast<const char *>(v), sizeof(v));
        out.write(reinterpret_cast<const char *>(indices), sizeof(indices));
      }
      metadata.map = cadence::content::utf8(map);
      auto set = cadence::content::createSet(root, "cross rig");
      cadence::content::saveDemo(app.recordedTake, metadata, set, "as50");
      cadence::content::Progress progress;
      auto plan = cadence::content::planSet(set, progress);
      for (const auto &p : plan.missing)
        std::cerr << "MISSING " << p << '\n';
      if (!plan.missing.empty())
        return 1;
      cadence::content::packSet(plan, root / "cross-rig.zip", true, progress);
      std::filesystem::rename(map.parent_path(),root/"map-source-hidden");
      std::cout << "Packed " << plan.files.size() << " files, " << plan.bytes
                << " bytes\n";
    } else {
      cadence::content::Progress progress;
      const auto home = root / "recipient";
      auto id =
          cadence::content::importPack(home, root / "cross-rig.zip", progress);
      app.demoLibrary.roots = cadence::content::activatePacks(home);
      for (const auto &dir : app.demoLibrary.roots) {
        assets::Catalog part;
        if (!assets::scan(dir, part, error))
          throw std::runtime_error(error);
        app.assetCatalog.entries.insert(app.assetCatalog.entries.end(),
                                        part.entries.begin(),
                                        part.entries.end());
      }
      auto demo =
          cadence::content::listDemos(cadence::content::listSets(home).front())
              .front();
      const auto installedPacks=cadence::content::packs(home);
      const auto installed=std::find_if(installedPacks.begin(),installedPacks.end(),[&](const auto& p){return p.id==id;});
      if(installed==installedPacks.end())throw std::runtime_error("Imported pack not listed");
      cadence::content::setEnabled(*installed,false);
      cadence::content::activatePacks(home);
      bool disabledRejected=false;
      try {openSetDemo(app,demo);} catch(const std::exception&){disabledRejected=true;}
      if(!disabledRejected||app.pendingMapFuture||!app.demoLibrary.pendingDemo.empty()||app.takePreview)
        throw std::runtime_error("Disabled pack replay was not rejected before state mutation");
      cadence::content::setEnabled(*installed,true);
      cadence::content::activatePacks(home);
      std::cout<<"Disabled pack rejected without replay state mutation; re-enabled successfully\n";
      openSetDemo(app, demo);
      const auto start = std::chrono::steady_clock::now();
      while (app.pendingMapFuture || !app.demoLibrary.pendingDemo.empty()) {
        processPendingMapLoad(app);
        processDemoLibrary(app);
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(60))
          throw std::runtime_error("Map/replay load timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      if (!app.takePreview || !app.loadedMap ||
          app.loadedMap->scene.meshes.empty())
        throw std::runtime_error(app.status);
      const auto originalSamples = app.recordedTake.samples.size();
      const auto originalPack = app.demoLibrary.recording.pack;
      if (openRecordedTake(app, root / "does-not-exist.c_dm") ||
          app.recordedTake.samples.size() != originalSamples ||
          app.demoLibrary.recording.pack != originalPack ||
          cadence::content::preferredPack != originalPack)
        throw std::runtime_error(
            "Failed replay open changed the active replay");
      // A structurally valid file with an unavailable required actor used to
      // replace the active take before restoration discovered the problem.
      const auto activeSceneBones=app.scene.skeleton.bones.size();
      const auto activeClips=app.scene.animations.size();
      const auto activeTime=app.takeTime;
      const auto activeModel=app.recordedTake.actor.baseModel;
      const auto activeGeometry=geometry(app.scene);
      const auto activeMetadata=app.demoLibrary.recording;
      const auto unchanged=[&]{return app.takePreview&&app.recordedTake.actor.baseModel==activeModel&&app.takeTime==activeTime&&app.recordedTake.samples.size()==originalSamples&&geometry(app.scene)==activeGeometry&&app.scene.animations.size()==activeClips&&app.demoLibrary.recording.map==activeMetadata.map&&app.demoLibrary.recording.pack==activeMetadata.pack&&app.demoLibrary.recording.mapScale==activeMetadata.mapScale&&app.demoLibrary.recording.construction==activeMetadata.construction&&cadence::content::preferredPack==originalPack;};
      {
        const auto invalid=root/"existing-invalid.cast",incompatible=root/"existing-incompatible.cast";
        {std::ofstream out(invalid,std::ios::binary);out<<"invalid CAST fixture";}
        {std::ofstream out(incompatible,std::ios::binary);const std::uint32_t header[]{cast::Document::kMagic,1,0,0};out.write(reinterpret_cast<const char*>(header),sizeof(header));}
        const auto first=app.recordedTake.samples.front().weaponSlot;
        const auto originalActor=app.recordedTake.actorSlots[first].empty()?app.recordedTake.actor:app.recordedTake.actorSlots[first];
        const auto beforeDependencies=cadence::content::constructionDocuments();
        for(int kind=0;kind<3;++kind){
          auto broken=app.recordedTake;
          const auto slot=static_cast<std::uint8_t>(kind==2?(first+1)%3:first);
          auto actor=originalActor;actor.baseModel=cadence::content::utf8(kind==1?incompatible:invalid);actor.rigModels.clear();actor.attachedModels.clear();
          broken.actorSlots[slot]=actor;
          if(kind!=2)broken.actor=actor;
          else{
            broken.actorSlotBoneCounts[slot]=broken.bonesForSlot(first);
            broken.worldActorSlotBoneCounts[slot]=broken.bonesForWorldSlot(first);
            auto sample=broken.samples.front();sample.weaponSlot=slot;sample.time=broken.samples.back().time+1.f;broken.samples.push_back(std::move(sample));
          }
          const auto path=root/("existing-model-failure-"+std::to_string(kind)+".c_dm");
          if(!take::save(broken,path,error))throw std::runtime_error(error);
          cadence::content::writeMetadata(path,activeMetadata);
          const auto yaw=app.cameraYaw,pitch=app.cameraPitch;const bool playing=app.takePlaying;
          if(openRecordedTake(app,path)||!unchanged()||app.cameraYaw!=yaw||app.cameraPitch!=pitch||app.takePlaying!=playing||cadence::content::constructionDocuments()!=beforeDependencies)
            throw std::runtime_error("Existing model failure changed recipient playback, case "+std::to_string(kind));
          // A different requested map scale forces the deferred-map route.
          // Actor preparation must fail before even scheduling that map job.
          auto differentMap=activeMetadata;differentMap.mapScale=3.f+kind;
          cadence::content::writeMetadata(path,differentMap);
          const auto mapSource=app.loadedMap->sourcePath;
          const auto mapScale=app.loadedMap->scaleMultiplier;
          const auto importScale=app.mapImportScale;
          bool rejected=false;try{openSetDemo(app,path);}catch(const std::exception&){rejected=true;}
          if(!rejected||!unchanged()||app.pendingMapFuture||!app.demoLibrary.pendingDemo.empty()||
             !app.loadedMap||app.loadedMap->sourcePath!=mapSource||app.loadedMap->scaleMultiplier!=mapScale||
             app.mapImportScale!=importScale||app.cameraYaw!=yaw||app.cameraPitch!=pitch||app.takePlaying!=playing||
             cadence::content::constructionDocuments()!=beforeDependencies)
            throw std::runtime_error("Bad set actor scheduled or changed the map, case "+std::to_string(kind));
        }
        std::cout<<"Existing invalid/incompatible/later-slot model failures preserve recipient replay and reject before map scheduling\n";
      }
      for(int mode=0;mode<4;++mode){
        const auto previousWorkspace=app.workspaceMode;
        app.takeRecording=mode==0;app.exportActive=mode==1;
        if(mode>=2)app.workspaceMode=mode==2?3:2;
        const bool direct=openRecordedTake(app,demo);
        bool setRejected=false;try{openSetDemo(app,demo);}catch(const std::exception&){setRejected=true;}
        const bool preserved=unchanged()&&!app.pendingMapFuture&&app.demoLibrary.pendingDemo.empty();
        app.takeRecording=false;app.exportActive=false;app.workspaceMode=previousWorkspace;
        if(direct||!setRejected||!preserved)throw std::runtime_error("Busy/non-runtime replay open changed active replay, case "+std::to_string(mode));
      }
      {
        take::Take legacy;legacy.boneCount=activeSceneBones+1;
        take::Sample sample;sample.pose.resize(legacy.boneCount,scene::Mat4::identity());
        legacy.samples.push_back(std::move(sample));
        const auto legacyFile=root/"legacy-incompatible.c_dm";
        if(!take::save(legacy,legacyFile,error))throw std::runtime_error(error);
        take::Take readable;if(!take::load(legacyFile,readable,error))throw std::runtime_error(error);
        const bool direct=openRecordedTake(app,legacyFile);
        bool setRejected=false;try{openSetDemo(app,legacyFile);}catch(const std::exception&){setRejected=true;}
        if(direct||!setRejected||!unchanged()||app.pendingMapFuture||!app.demoLibrary.pendingDemo.empty())
          throw std::runtime_error("Legacy skeleton mismatch discarded the current replay");
        readable.boneCount=activeSceneBones;
        readable.samples.front().pose.resize(activeSceneBones);
        if(legacyTakeLoadBlockedReason(app,readable))throw std::runtime_error("Matching legacy skeleton was rejected");
        std::cout<<"Legacy skeleton mismatch preserves active replay and matching legacy remains eligible\n";
      }
      for(int auxiliary=0;auxiliary<2;++auxiliary){
        take::Take legacy;legacy.boneCount=activeSceneBones;
        take::Sample sample;sample.pose.resize(activeSceneBones,scene::Mat4::identity());
        const auto badActor=cadence::content::utf8(root/"existing-invalid.cast");
        if(auxiliary==0){legacy.worldActor.baseModel=badActor;legacy.worldBoneCount=1;sample.worldActor.pose.resize(1,scene::Mat4::identity());}
        else{legacy.botActor.baseModel=badActor;legacy.botBoneCount=1;legacy.botCount=1;sample.bots.resize(1);sample.bots.front().pose.resize(1,scene::Mat4::identity());}
        legacy.samples.push_back(std::move(sample));
        const auto legacyFile=root/(auxiliary==0?"legacy-bad-world.c_dm":"legacy-bad-bot.c_dm");
        if(!take::save(legacy,legacyFile,error))throw std::runtime_error(error);
        auto metadata=activeMetadata;metadata.mapScale=8.f+auxiliary;
        cadence::content::writeMetadata(legacyFile,metadata);
        const auto mapPath=app.loadedMap->sourcePath;const auto mapScale=app.loadedMap->scaleMultiplier;
        const auto importScale=app.mapImportScale;const auto dependencies=cadence::content::constructionDocuments();
        if(openRecordedTake(app,legacyFile)||!unchanged())throw std::runtime_error("Bad legacy auxiliary actor replaced recipient replay");
        bool rejected=false;try{openSetDemo(app,legacyFile);}catch(const std::exception&){rejected=true;}
        if(!rejected||!unchanged()||app.pendingMapFuture||!app.demoLibrary.pendingDemo.empty()||app.pendingPreparedTakeOpen||
           app.loadedMap->sourcePath!=mapPath||app.loadedMap->scaleMultiplier!=mapScale||app.mapImportScale!=importScale||
           cadence::content::constructionDocuments()!=dependencies)
          throw std::runtime_error("Bad legacy auxiliary actor changed or scheduled recipient map");
      }
      std::cout<<"Legacy auxiliary actor failures preserve recipient replay and map\n";
      app.demoLibrary.pendingDemo=demo;
      app.demoLibrary.pendingMap=app.loadedMap->sourcePath;
      app.demoLibrary.pendingMapScale=app.loadedMap->scaleMultiplier;
      app.exportActive=true;
      processDemoLibrary(app);
      const bool stayedQueued=app.demoLibrary.pendingDemo==demo&&unchanged();
      app.exportActive=false;
      if(!stayedQueued)throw std::runtime_error("Deferred replay was consumed during capture");
      processDemoLibrary(app);
      if(!app.demoLibrary.pendingDemo.empty()||!unchanged())throw std::runtime_error("Deferred replay failed to resume after capture");
      std::cout<<"Recording/capture/workspace replay guards and deferred resume preserve active scene\n";
      auto missingTake=app.recordedTake;
      missingTake.actor.baseModel=cadence::content::utf8(root/"unavailable-model.cast");
      const auto missingFile=root/"missing-required-model.c_dm";
      if(!take::save(missingTake,missingFile,error))throw std::runtime_error(error);
      if(openRecordedTake(app,missingFile)||!app.takePreview||
         app.recordedTake.actor.baseModel!=activeModel||app.takeTime!=activeTime||
         app.recordedTake.samples.size()!=originalSamples||
         app.scene.skeleton.bones.size()!=activeSceneBones||app.scene.animations.size()!=activeClips||
         app.demoLibrary.recording.pack!=originalPack||cadence::content::preferredPack!=originalPack)
        throw std::runtime_error("Missing model preflight mutated the current replay");
      std::promise<AppState::MapLoadResult> pendingMap;
      app.pendingMapFuture=pendingMap.get_future();
      const bool openedWhileMapPending=openRecordedTake(app,demo);
      app.pendingMapFuture.reset();
      if(openedWhileMapPending||!app.takePreview||app.recordedTake.actor.baseModel!=activeModel||
         app.scene.skeleton.bones.size()!=activeSceneBones||app.takeTime!=activeTime)
        throw std::runtime_error("Direct replay open ignored pending map load");
      std::cout<<"Missing required model and pending-map direct-open checks preserve active replay\n";
      auto relocatedTake=app.recordedTake;
      const auto beforeRelocationGeometry=geometry(app.scene);
      const auto beforeRelocationDriver=app.scene.viewHandsDriverGame;
      // This control intentionally has no export-game provenance, even when
      // the recipient fixture itself lives below an exported_files folder.
      relocatedTake.actor.baseModel=cadence::content::utf8(std::filesystem::path("Z:/cadence-audit-former-machine")/std::filesystem::u8path(activeModel).filename());
      const auto relocatedFile=root/"relocated-model.c_dm";
      if(!take::save(relocatedTake,relocatedFile,error))throw std::runtime_error(error);
      cadence::content::writeMetadata(relocatedFile,app.demoLibrary.recording);
      if(!openRecordedTake(app,relocatedFile)||!app.takePreview||app.scene.skeleton.bones.size()!=activeSceneBones)
        throw std::runtime_error("Preflight broke existing filename/catalog relocation fallback: "+app.status);
      if(app.scene.viewHandsDriverGame!="csnz"||app.scene.viewHandsDriverGame!=beforeRelocationDriver||geometry(app.scene)!=beforeRelocationGeometry)
        throw std::runtime_error("Relocated foreign hands lost driver or bind/vertex geometry");
      if(app.selectedWeaponAsset>=app.assetCatalog.entries.size()||app.assetCatalog.entries[app.selectedWeaponAsset].game!="csnz"||app.assetCatalog.entries[app.selectedWeaponAsset].role!=assets::Role::ViewWeapon||app.assetCatalog.entries[app.selectedWeaponAsset].name.find("as50")==std::string::npos)
        throw std::runtime_error("Relocated foreign hands selected wrong native weapon");
      std::cout<<"Direct-open preflight preserves the existing relocated-model fallback\n";
      app.loadedMap->scaleMultiplier = 2;
      app.pendingModelKind = 1;
      bool blocked = false;
      try {
        openSetDemo(app, demo);
      } catch (...) {
        blocked = true;
      }
      app.pendingModelKind = 0;
      if (!blocked || app.pendingMapFuture)
        throw std::runtime_error(
            "Busy asset load did not block set map replacement");
      openSetDemo(app, demo);
      if (!app.pendingMapFuture || app.demoLibrary.pendingDemo.empty() || !app.pendingPreparedTakeOpen)
        throw std::runtime_error("Same map at a different scale was reused");
      const auto preparedHandle=app.pendingPreparedTakeOpen;
      const auto reloadStart = std::chrono::steady_clock::now();
      while (app.pendingMapFuture) {
        processPendingMapLoad(app);
        if (std::chrono::steady_clock::now() - reloadStart >
            std::chrono::seconds(60))
          throw std::runtime_error("Scaled map reload timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      app.exportActive=true;
      processDemoLibrary(app);
      if(app.pendingPreparedTakeOpen!=preparedHandle||app.demoLibrary.pendingDemo.empty())
        throw std::runtime_error("Prepared set replay was consumed while capture blocked publication");
      app.exportActive=false;
      const auto previousProfile=app.weaponProfile;
      app.weaponProfile.gunPosition={11.f,22.f,33.f};
      app.weaponProfile.adsGunPosition={44.f,55.f,66.f};
      app.weaponProfile.separateAdsPosition=true;
      app.weaponProfile.viewmodelFovMultiplier=1.23f;
      processDemoLibrary(app);
      if (!app.takePreview || app.loadedMap->scaleMultiplier != 1 || app.pendingPreparedTakeOpen || !app.demoLibrary.pendingDemo.empty())
        throw std::runtime_error("Scaled map replay did not restore");
      if(app.weaponProfile.gunPosition.x!=11.f||app.weaponProfile.gunPosition.z!=33.f||
         app.weaponProfile.adsGunPosition.z!=66.f||!app.weaponProfile.separateAdsPosition||
         app.weaponProfile.viewmodelFovMultiplier!=1.23f)
        throw std::runtime_error("Prepared replay overwrote current presentation edits made during map loading");
      app.weaponProfile=previousProfile;
      if(app.activeClassSlot>=0&&app.classSlotRigs[app.activeClassSlot])app.classSlotRigs[app.activeClassSlot]->profile=previousProfile;
      std::cout << "Failed-open preservation, busy-load guard and map-scale "
                   "reload with retained preparation and current profile edits passed\n";
      {
        auto retained=prepareRecordedTakeOpen(app,demo);
        if(!retained)throw std::runtime_error("Could not prepare deferred failure control: "+app.status);
        const auto oldGeometry=geometry(app.scene);
        const auto oldTakeModel=app.recordedTake.actor.baseModel;
        const auto oldMap=app.loadedMap->sourcePath;
        const auto oldPack=cadence::content::preferredPack;
        for(int failure=0;failure<2;++failure){
          app.pendingPreparedTakeOpen=retained;
          app.demoLibrary.pendingDemo=demo;
          app.demoLibrary.pendingMap=failure==0?root/"map-that-failed-to-load.glb":oldMap;
          app.demoLibrary.pendingMapScale=1;
          const bool oldGpuReady=app.mapGpuReady;
          if(failure==1)app.mapGpuReady=false;
          processDemoLibrary(app);
          app.mapGpuReady=oldGpuReady;
          if(app.pendingPreparedTakeOpen||!app.demoLibrary.pendingDemo.empty()||!app.demoLibrary.pendingMap.empty()||
             geometry(app.scene)!=oldGeometry||app.recordedTake.actor.baseModel!=oldTakeModel||
             app.loadedMap->sourcePath!=oldMap||cadence::content::preferredPack!=oldPack)
            throw std::runtime_error("Failed deferred map retained stale preparation or published incoming playback");
        }
        std::cout<<"Failed or GPU-unready deferred map releases retained actors without publishing playback\n";
      }
      for (const auto &p : cadence::content::constructionDocuments())
        if (!cadence::content::key(p).starts_with(cadence::content::key(home) +
                                                  "/"))
          throw std::runtime_error("Escaped pack: " + p.string());
      for (const auto &mesh : app.scene.meshes)
        for (const auto &p :
             {mesh.albedoPath, mesh.normalPath, mesh.specularPath,
              mesh.metalnessPath, mesh.roughnessPath, mesh.emissivePath})
          if (!p.empty() && (!std::filesystem::is_regular_file(p) ||
                             !cadence::content::key(p).starts_with(
                                 cadence::content::key(home) + "/")))
            throw std::runtime_error("Unpacked texture: " + p.string());
      std::cout << "Fresh process replay restored exclusively from pack\n";
    }
    std::vector<float> values;
    for (const auto &b : app.scene.skeleton.bones)
      values.insert(values.end(), b.inverseBind.v.begin(),
                    b.inverseBind.v.end());
    for (const auto &mesh : app.scene.meshes)
      for (const auto &v : mesh.vertices) {
        values.push_back(v.position.x);
        values.push_back(v.position.y);
        values.push_back(v.position.z);
      }
    if (produce) {
      std::ofstream out(root / "reference.bin", std::ios::binary);
      out.write(reinterpret_cast<const char *>(values.data()),
                values.size() * sizeof(float));
    } else {
      std::ifstream in(root / "reference.bin",
                       std::ios::binary | std::ios::ate);
      if (in.tellg() != std::streamoff(values.size() * sizeof(float)))
        throw std::runtime_error("Scene layout changed");
      in.seekg(0);
      std::vector<float> expected(values.size());
      in.read(reinterpret_cast<char *>(expected.data()),
              expected.size() * sizeof(float));
      float maximum{};
      for (size_t i = 0; i < values.size(); ++i)
        maximum = std::max(maximum, std::abs(values[i] - expected[i]));
      std::cout << "Bind/vertex maximum error " << maximum << " across "
                << values.size() << " values\n";
      if (maximum > 1e-4f)
        return 1;
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
