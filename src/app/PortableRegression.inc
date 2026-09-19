// Opt-in audit, run only in a fresh, disposable portable package copy.
int portableRegression(const std::filesystem::path& output){
    if(!cadence::portable::enabled()||std::filesystem::exists(settingsFilePath())){std::cerr<<"Use a fresh portable copy with no settings.txt\n";return 2;}
    std::filesystem::create_directories(output);
    int failures=0;const auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';if(!ok)++failures;};
    check(settingsFilePath().parent_path()==cadenceAssetsDirectory()/"settings","Settings isolated from AppData");
    check(!loadDefaultSalukiDirectory(),"Fresh install has no personal Saluki directory");
    check(loadDefaultMapDirectory().empty(),"Fresh install does not load developer default map");
    const auto root=output/std::filesystem::path(u8"Saluki test \u00e9")/"exported_files";
    std::filesystem::create_directories(root/"bo2");
    saveDefaultSalukiDirectory(root.parent_path());
    check(loadDefaultSalukiDirectory()==root.parent_path(),"Unicode and spaces round-trip");
    auto app=std::make_unique<AppState>();app->defaultSalukiDirectory=*loadDefaultSalukiDirectory();
    prepareCatalogGameChoices(*app,false);
    check(app->catalogGameChoices.size()==1&&app->catalogGameChoices.front().first=="bo2","Parent Saluki directory resolves exported_files");
    saveDefaultSalukiDirectory(root);check(loadDefaultSalukiDirectory()==root,"Direct exported_files directory supported");
    saveDefaultSalukiDirectory(output/"missing");check(!loadDefaultSalukiDirectory(),"Missing saved directory requests setup");
    saveDefaultSalukiDirectory({});check(!loadDefaultSalukiDirectory(),"Empty directory requests setup");
    std::filesystem::remove(settingsFilePath());
    const auto encoder=cadence::portable::ffmpegExecutable();
    check(encoder==programDirectory()/"tools"/"ffmpeg.exe","Bundled encoder selected without PATH dependency");
    app->exportWidth=16;app->exportHeight=16;app->exportFps=30;
    FILE* pipe{};const auto movie=output/"portable prores.mov";
    const bool opened=openProResPipe(*app,pipe,movie,false);check(opened,"ProRes encoder opens through quoted portable path");
    if(opened){std::vector<unsigned char> pixels(16*16*4,128);std::fwrite(pixels.data(),1,pixels.size(),pipe);check(_pclose(pipe)==0,"ProRes encoder completes");check(std::filesystem::file_size(movie)>128,"ProRes file produced");}
    cadence::capture::LiveViewportRecorder recorder;const auto mp4=output/"portable gameplay.mp4";
    check(recorder.start(mp4,16,16,30,18,30,false),"Bundled gameplay recorder starts");
    cadence::capture::CameraFrame camera;camera.width=16;camera.height=16;
    check(recorder.submit(std::vector<std::uint8_t>(16*16*4,128),camera),"Gameplay recording accepts frame");recorder.stop();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(recorder.active()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    check(!recorder.active()&&recorder.frames()==1&&recorder.error().empty(),"Gameplay recording completes with one frame");
    std::cout<<"Portable failures: "<<failures<<'\n';return failures?1:0;
}
