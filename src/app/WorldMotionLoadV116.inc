void appendSupplementalWorldMotion(AppState& app,scene::CastScene& actor){
    const auto root=app.defaultSalukiDirectory.empty()
        ?std::filesystem::path{}
        :salukiExportRoot(app.defaultSalukiDirectory);
    const auto append=[&](const std::string& game,const std::filesystem::path& relative,scene::MotionRole motion){
        const auto filename=lowerText(relative.filename().string());
        for(const auto& clip:actor.animations)if(lowerText(clip.sourceName)==filename)return;
        std::error_code ec;auto path=root/game/"animations"/relative;
        if(!std::filesystem::is_regular_file(path,ec))path=root/"exported_files"/game/"animations"/relative;
        if(!std::filesystem::is_regular_file(path,ec))return;
        auto document=cast::Document::load(path);if(!document.valid())return;
        const auto first=actor.animations.size();
        const auto targetGame=app.classWorldModelAsset<app.assetCatalog.entries.size()?lowerText(app.assetCatalog.entries[app.classWorldModelAsset].game):std::string{};
        // Supplemental AW/Ghosts actions need the same reference-frame bridge
        // as the selected animation library on every foreign player rig.
        if(!appendPointBlankWorldAnimation(app,document,actor,game,targetGame))scene::appendAnimations(document,actor);
        for(std::size_t i=first;i<actor.animations.size();++i){
            auto& clip=actor.animations[i];clip.sourceGame=game;clip.domain=scene::AnimationDomain::PlayerBody;
            clip.motion=motion;clip.action=scene::ActionRole::None;clip.direction=worldClipDirection(clip);
        }
    };
    // Exact exported AW body clip, independent of selected game or weapon.
    append("aw","pb/jump/generic/pb_standjump_boost_takeoff.cast",scene::MotionRole::Jump);
    append("aw","pb/melee/knife/pb_standjump_boost_takeoff_knife.cast",scene::MotionRole::Jump);
    // Reserve this clip for the explicit boost branch; ordinary jump queries
    // must not choose it merely because it is the only imported AW jump.
    for(auto& clip:actor.animations)if(isWorldBoostClip(clip)){
        clip.action=scene::ActionRole::Gesture;clip.motion=scene::MotionRole::Jump;clip.looping=false;
    }
    append("ghosts","mp/scripted/generic/mp_slide.cast",scene::MotionRole::Slide);
    for(int height:{16,24,32,40,48,56,64})for(const char* kind:{"up","over"})
        append("ghosts",std::filesystem::path("mp/scripted/generic")/("mp_mantle_"+std::to_string(height)+"_"+kind+".cast"),scene::MotionRole::Climb);
    // Cold War PB clips are evaluated on the untouched native skeleton and
    // converted by the world-pose adapter installed during scene import.
    for(const char* direction:{"l","r","b"})append("bocw_sp",std::filesystem::path("pb/run/assault rifle")/("pb_rifle_run_slide_land_"+std::string(direction)+".cast"),scene::MotionRole::Slide);
    for(const char* direction:{"l","r","b","f"}){
        append("bocw_sp",std::filesystem::path("pb/climb/assault rifle")/("pb_rifle_mantle_over_low_"+std::string(direction)+"_36.cast"),scene::MotionRole::Climb);
        append("bocw_sp",std::filesystem::path("pb/climb/assault rifle")/("pb_rifle_mantle_on_"+std::string(direction)+"_18.cast"),scene::MotionRole::Climb);
    }
}
