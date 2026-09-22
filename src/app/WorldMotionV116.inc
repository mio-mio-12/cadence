// World-body selection only: viewmodel weapon mappings and recorded poses are untouched.
float smoothWorldFacingOffset(float current,float target,float delta){
    const float change=std::remainder(target-current,2.f*scene::kPi);
    return std::remainder(current+change*(1.f-std::exp(-std::max(0.f,delta)/.075f)),2.f*scene::kPi);
}
float worldMovementFacing(float viewYaw,scene::Vec3 travel,scene::Direction authored){
    if(travel.x*travel.x+travel.y*travel.y<=1.f)return viewYaw;
    const float clipAngle=authored==scene::Direction::Left?scene::kPi*.5f:
        authored==scene::Direction::Right?-scene::kPi*.5f:
        authored==scene::Direction::Backward?scene::kPi:
        authored==scene::Direction::ForwardLeft?scene::kPi*.25f:
        authored==scene::Direction::ForwardRight?-scene::kPi*.25f:
        authored==scene::Direction::BackwardLeft?scene::kPi*.75f:
        authored==scene::Direction::BackwardRight?-scene::kPi*.75f:0.f;
    // Cardinal clip selection loses diagonal travel. Preserve the continuous
    // residual angle, even when the selected clip matches the cardinal query.
    const float offset=std::remainder(std::atan2(travel.y,travel.x)-clipAngle-viewYaw,2.f*scene::kPi);
    // A missing backpedal clip must not turn the actor away from its aim.
    // Retain diagonal adjustment, but never correct a wrong clip by 180 degrees.
    return viewYaw+(std::abs(offset)<=scene::kPi*.5f+.001f?offset:0.f);
}
scene::Direction worldClipDirection(const scene::Animation& clip){
    if(clip.direction==scene::Direction::ForwardLeft||clip.direction==scene::Direction::ForwardRight||
       clip.direction==scene::Direction::BackwardLeft||clip.direction==scene::Direction::BackwardRight)return clip.direction;
    auto name=lowerText(clip.sourceName);
    if(name.ends_with(".cast"))name.resize(name.size()-5);
    for(const auto& [token,direction]:std::initializer_list<std::pair<const char*,scene::Direction>>{
        {"left",scene::Direction::Left},{"right",scene::Direction::Right},
        {"back",scene::Direction::Backward},{"front",scene::Direction::Forward},
        {"forward",scene::Direction::Forward},{"backward",scene::Direction::Backward},
        {"l",scene::Direction::Left},{"r",scene::Direction::Right},
        {"b",scene::Direction::Backward},{"f",scene::Direction::Forward}}){
        const std::string key="_"+std::string(token);
        if(name.ends_with(key)||name.find(key+"_")!=std::string::npos)return direction;
    }
    return clip.direction;
}

bool isGroundWorldMotion(scene::MotionRole motion){
    using M=scene::MotionRole;
    return motion==M::Idle||motion==M::Walk||motion==M::Run||motion==M::Sprint||motion==M::Crawl;
}

// Stance and locomotion are requirements, not small score bonuses that an
// exact weapon name can outweigh. Upper-body actions are layered separately.
std::optional<std::size_t> selectGroundWorldLocomotion(const scene::CastScene& actor,const scene::AnimationQuery& q){
    using M=scene::MotionRole;using W=scene::WeaponClass;
    const auto longGun=[](W w){return w==W::Rifle||w==W::Sniper||w==W::Automatic||w==W::LMG||w==W::Shotgun||w==W::M1216||w==W::Judge||w==W::G11||w==W::Crossbow;};
    std::optional<std::size_t> best;int bestScore=-100000;
    for(std::size_t i=0;i<actor.animations.size();++i){
        const auto& c=actor.animations[i];
        if(c.tracks.empty()||c.domain!=scene::AnimationDomain::PlayerBody||c.action!=scene::ActionRole::None||c.contextual||!isGroundWorldMotion(c.motion))continue;
        if(c.stance!=q.stance&&c.stance!=scene::Stance::Any)continue;
        if((q.motion==M::Idle)!=(c.motion==M::Idle))continue;
        if(q.stance!=scene::Stance::Prone&&c.motion==M::Crawl)continue;
        const auto name=lowerText(c.sourceName);
        // Short stance-adjustment shuffles are not sustained walking cycles.
        // Their exact rifle/direction match otherwise beats genuine locomotion
        // when ADS asks for Walk (notably BO2 clips on CODM bodies).
        if(name.find("_shuffle_")!=std::string::npos)continue;
        if(c.weapon!=q.weapon&&(c.weapon==W::Launcher||c.weapon==W::Grenade||c.weapon==W::RiotShield||c.weapon==W::Equipment||c.weapon==W::Tablet||c.weapon==W::Radio||c.weapon==W::Briefcase))continue;
        if(name.find("_hold")!=std::string::npos||name.find("unarmed")!=std::string::npos||name.find("_in.")!=std::string::npos||name.find("_out.")!=std::string::npos)continue;
        const auto d=worldClipDirection(c);
        int score=c.stance==q.stance?20000:0;
        if(q.direction!=scene::Direction::Any)score+=d==q.direction?10000:d==scene::Direction::Any?0:-10000;
        score+=c.motion==q.motion?2000:0;
        score+=c.ads==q.ads?200:0;
        score+=c.weapon==q.weapon?5500:c.weapon==W::Any?3500:0;
        if(longGun(q.weapon)&&longGun(c.weapon))score+=4000;
        // With no known family, prefer an ordinary single long-gun carry to
        // whichever exotic/dual-handgun clip happens to sort first.
        if(q.weapon==W::Any&&c.weapon==W::Rifle)score+=4000;
        const auto game=lowerText(c.sourceGame);
        score+=game==lowerText(q.preferredGame)?100:0;
        if(q.forceT6Locomotion&&(game=="bo2"||game=="bo2_sp"||game=="bo2+bo2_sp"||game=="t6"))score+=4000;
        score+=c.looping?20:0;
        if(score>bestScore){best=i;bestScore=score;}
    }
    return best;
}

std::optional<std::size_t> selectWorldMantle(const scene::CastScene& actor,float heightIw,bool over,scene::Direction direction,const std::string& game,scene::WeaponClass weapon){
    std::optional<std::size_t> best;float bestScore=-100000.f;
    for(std::size_t i=0;i<actor.animations.size();++i){
        const auto& clip=actor.animations[i];const auto name=lowerText(clip.sourceName);
        if(clip.tracks.empty()||clip.domain==scene::AnimationDomain::ViewModel||name.find("ladder")!=std::string::npos||
           (name.find("mantle")==std::string::npos&&name.find("vault")==std::string::npos))continue;
        const bool clipOver=name.find("over")!=std::string::npos||name.find("vault")!=std::string::npos;
        float height=0;
        // Heights are underscore-delimited values; do not confuse 32 with 132.
        for(std::size_t p=0;p<name.size();++p)if((p==0||name[p-1]=='_')&&std::isdigit(static_cast<unsigned char>(name[p]))){
            std::size_t end=p;int value=0;while(end<name.size()&&std::isdigit(static_cast<unsigned char>(name[end])))value=value*10+name[end++]-'0';
            if(value>=16&&value<=160&&(end==name.size()||name[end]=='_'||name[end]=='.'))height=static_cast<float>(value);
            p=end;
        }
        if(!height){if(name.find("low")!=std::string::npos)height=24;else if(name.find("mid")!=std::string::npos)height=40;else if(name.find("high")!=std::string::npos)height=56;}
        const auto clipDirection=worldClipDirection(clip);
        float score=(clipOver==over?200.f:-200.f)+(height?100.f-std::abs(height-heightIw)*5.f:0.f);
        score+=clipDirection==direction?80.f:clipDirection==scene::Direction::Any?10.f:-80.f;
        score+=lowerText(clip.sourceGame)==lowerText(game)?25.f:0.f;
        score+=clip.weapon==weapon?30.f:clip.weapon==scene::WeaponClass::Any?0.f:-60.f;
        if(score>bestScore){best=i;bestScore=score;}
    }
    return best;
}

std::optional<std::size_t> selectDirectionalWorldClip(const scene::CastScene& actor,scene::MotionRole motion,scene::Direction direction,const std::string& game,scene::WeaponClass weapon=scene::WeaponClass::Any){
    std::optional<std::size_t> best;int bestScore=-10000;
    for(std::size_t i=0;i<actor.animations.size();++i){
        const auto& clip=actor.animations[i];const auto name=lowerText(clip.sourceName);
        if(clip.tracks.empty()||clip.domain!=scene::AnimationDomain::PlayerBody||clip.action!=scene::ActionRole::None||clip.motion!=motion)continue;
        using W=scene::WeaponClass;
        const bool longGun=weapon==W::Rifle||weapon==W::Sniper||weapon==W::Automatic||weapon==W::LMG||weapon==W::Shotgun||weapon==W::M1216||weapon==W::Judge;
        if(longGun&&(clip.weapon==W::Pistol||clip.weapon==W::DualWield||clip.weapon==W::Knife||name.find("pistol")!=std::string::npos||name.find("akimbo")!=std::string::npos))continue;
        if(weapon==W::Knife&&clip.weapon!=W::Knife)continue;
        if(motion==scene::MotionRole::Slide&&(name.find("death")!=std::string::npos||name.find("ladder")!=std::string::npos||name.find("2_")!=std::string::npos||name.find("slide2")!=std::string::npos||name.find("jugmaniac")!=std::string::npos||name.find("2crouch")!=std::string::npos||name.find("2prone")!=std::string::npos))continue;
        const auto d=worldClipDirection(clip);
        // Unlabelled normal slides are forward fallbacks, not lateral clips.
        if(d!=direction&&!(motion==scene::MotionRole::Slide&&direction==scene::Direction::Forward&&d==scene::Direction::Any))continue;
        if(motion==scene::MotionRole::Slide&&weapon!=scene::WeaponClass::Any&&name.find("akimbo")!=std::string::npos)continue;
        int score=lowerText(clip.sourceGame)==lowerText(game)?20:0;
        if(weapon!=scene::WeaponClass::Any&&clip.weapon!=scene::WeaponClass::Any)score+=clip.weapon==weapon?40:-100;
        if(score>bestScore){best=i;bestScore=score;}
    }
    return best;
}

std::optional<std::size_t> selectCanonicalLongGunLocomotion(const scene::CastScene& actor,const scene::AnimationQuery& query){
    using W=scene::WeaponClass;
    const bool longGun=query.weapon==W::Rifle||query.weapon==W::Sniper||query.weapon==W::Automatic||query.weapon==W::LMG||query.weapon==W::Shotgun||query.weapon==W::M1216||query.weapon==W::Judge||query.weapon==W::G11;
    if(!longGun||query.action!=scene::ActionRole::None||(query.motion!=scene::MotionRole::Run&&query.motion!=scene::MotionRole::Sprint)||
       (query.stance!=scene::Stance::Stand&&query.stance!=scene::Stance::Any))return {};
    std::optional<std::size_t> best;int bestScore=-10000;
    for(std::size_t i=0;i<actor.animations.size();++i){
        const auto& clip=actor.animations[i];const auto name=lowerText(clip.sourceName);
        if(clip.tracks.empty()||clip.motion!=query.motion||clip.domain!=scene::AnimationDomain::PlayerBody||clip.action!=scene::ActionRole::None||
           (clip.stance!=scene::Stance::Any&&clip.stance!=scene::Stance::Stand)||clip.contextual)continue;
        if(!(name.starts_with("pb_combatrun_")||name.starts_with("pb_sprint_")))continue;
        if(name.find("akimbo")!=std::string::npos||name.find("pistol")!=std::string::npos||name.find("shield")!=std::string::npos||name.find("rpg")!=std::string::npos||name.find("grenade")!=std::string::npos||name.find("knife")!=std::string::npos||name.find("unarmed")!=std::string::npos||name.find("_hold")!=std::string::npos)continue;
        // AW/Ghosts have no forward assault export. An actual forward SMG
        // long-gun pose is preferable to playing the rifle backpedal forward.
        const auto d=worldClipDirection(clip);
        int score=d==query.direction?300:d==scene::Direction::Any?0:-300;
        score+=clip.weapon==query.weapon?70:clip.weapon==W::Rifle?40:0;
        if(query.weapon==W::Rifle&&name.find("_assault")!=std::string::npos)score+=50;
        if(query.weapon==W::Rifle&&name.find("_smg")!=std::string::npos)score+=20;
        score+=lowerText(clip.sourceGame)==lowerText(query.preferredGame)?150:0;
        const auto game=lowerText(clip.sourceGame);
        if(query.forceT6Locomotion&&(game=="bo2"||game=="bo2_sp"||game=="bo2+bo2_sp"||game=="t6"))score+=1000;
        if(name.find("_loop")!=std::string::npos)score+=20;
        if(score>bestScore){best=i;bestScore=score;}
    }
    return best;
}
// Explicit boost selection is separate from ordinary jump matching.
bool isWorldBoostClip(const scene::Animation& clip){
    const auto name=lowerText(clip.sourceName);
    return name=="pb_standjump_boost_takeoff.cast"||name=="pb_standjump_boost_takeoff_knife.cast";
}
std::optional<std::size_t> selectWorldBoost(const scene::CastScene& actor,scene::WeaponClass weapon){
    const char* name=weapon==scene::WeaponClass::Knife?"pb_standjump_boost_takeoff_knife.cast":"pb_standjump_boost_takeoff.cast";
    for(std::size_t i=0;i<actor.animations.size();++i){const auto& clip=actor.animations[i];
        if(clip.domain==scene::AnimationDomain::PlayerBody&&!clip.tracks.empty()&&lowerText(clip.sourceName)==name)return i;
    }
    return {};
}
