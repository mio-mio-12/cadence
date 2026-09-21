void drawPlayermodelAssembler(AppState& app){
    if(!ImGui::TreeNodeEx("Playermodel",ImGuiTreeNodeFlags_DefaultOpen))return;
    ImGui::PushID("character_assembler");
    using Part=assets::character::Part;
    constexpr auto automatic=std::size_t(-1),none=std::size_t(-2);
    auto current=app.autoPlayerModel?app.classWorldModelAsset:app.manualPlayerModelAsset;
    std::string game=current<app.assetCatalog.entries.size()?app.assetCatalog.entries[current].game:app.playermodelCustomGame;
    if(game.empty()){
        const auto hands=app.classViewhandsOverride<app.assetCatalog.entries.size()?app.classViewhandsOverride:app.classHandsAsset;
        if(hands<app.assetCatalog.entries.size())game=app.assetCatalog.entries[hands].game;
    }
    bool changed=false;
    if(ImGui::BeginCombo("Game",game.empty()?"Select":game.c_str())){
        for(const auto& g:rippedGames(app)){
            if(findGenericPlayermodelForGame(app,g)>=app.assetCatalog.entries.size())continue;
            if(ImGui::Selectable(g.c_str(),g==game)){
                game=g;app.playermodelCustomGame=g;app.autoPlayerModel=false;
                current=app.manualPlayerModelAsset=findGenericPlayermodelForGame(app,g);changed=true;
            }
        }ImGui::EndCombo();
    }
    if(current>=app.assetCatalog.entries.size()&&!game.empty())current=findGenericPlayermodelForGame(app,game);
    if(current>=app.assetCatalog.entries.size()){ImGui::PopID();ImGui::TreePop();return;}
    if(app.playermodelPreviewLastBuiltAsset!=current||app.playermodelPreviewLastBuiltGame!=game)updatePlayermodelPreviewScene(app);
    app.characterPreviewBindPose=true;
    const float width=std::max(100.f,ImGui::GetContentRegionAvail().x),height=std::clamp(width*.85f,240.f,380.f);
    drawCharacterPreviewNavigation(app,width,height);
    // Cache names/categories, not widgets: idle UI frames never scan disk or
    // rebuild a rig, and opening a selector doesn't load any animation clips.
    struct Choices{const void* data{};std::size_t count{},body{none};std::string game;std::array<std::vector<std::size_t>,12> parts;};
    static Choices choices;
    if(choices.data!=app.assetCatalog.entries.data()||choices.count!=app.assetCatalog.entries.size()||choices.body!=current||choices.game!=game){
        choices={};choices.data=app.assetCatalog.entries.data();choices.count=app.assetCatalog.entries.size();choices.body=current;choices.game=game;
        const auto& body=app.assetCatalog.entries[current];
        for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){
            const auto& a=app.assetCatalog.entries[i];if(a.game!=game)continue;
            Part part=Part::None;
            if(a.role==assets::Role::PlayerModel&&(game!="aw"||assets::character::awPart(a.name)==Part::Torso))part=Part::Torso;
            if(game=="aw"){
                part=assets::character::awPart(a.name);
                if(!assets::character::genderCompatible(body.name,a.name))continue;
            }else if(lowerText(body.name).find("_fb")==std::string::npos&&assets::character::headCompatible(game,body.name,a.name))part=Part::Head;
            if(part!=Part::None)choices.parts[static_cast<std::size_t>(part)].push_back(i);
        }
    }
    auto& setup=app.gameReferenceSetups[game];
    const auto& resolved=app.characterPreviewParts;
    const auto drawSlot=[&](const char* label,Part part,std::size_t& slot){
        const auto& options=choices.parts[static_cast<std::size_t>(part)];if(options.empty())return;
        std::size_t visible=slot;
        if(slot==automatic){for(auto i:resolved)if(std::find(options.begin(),options.end(),i)!=options.end()){visible=i;break;}}
        const auto noneLabel=part==Part::Head?"No head":"None";
        const auto preview=visible<app.assetCatalog.entries.size()?assets::character::displayName(app.assetCatalog.entries[visible].name):(slot==none?noneLabel:"Auto");
        ImGui::PushID(label);
        const auto cycle=[&](int direction){auto it=std::find(options.begin(),options.end(),visible);auto n=it==options.end()?(direction<0?0:-1):static_cast<int>(it-options.begin());n=(n+direction+static_cast<int>(options.size()))%static_cast<int>(options.size());slot=options[n];changed=true;};
        if(ImGui::ArrowButton("previous",ImGuiDir_Left))cycle(-1);ImGui::SameLine();
        if(ImGui::ArrowButton("next",ImGuiDir_Right))cycle(1);ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(80.f,ImGui::GetContentRegionAvail().x-74.f));
        if(ImGui::BeginCombo(label,preview.c_str())){
            static ImGuiTextFilter filter;
            if(ImGui::IsWindowAppearing()){filter.Clear();ImGui::SetKeyboardFocusHere();}
            filter.Draw("Search");
            if(part!=Part::Torso){
                if(ImGui::Selectable("Auto",slot==automatic)){slot=automatic;changed=true;}
                if(ImGui::Selectable(noneLabel,slot==none)){slot=none;changed=true;}
            }
            ImGui::BeginChild("choices",{0,240});
            for(auto i:options){const auto& a=app.assetCatalog.entries[i];if(!filter.PassFilter(a.name.c_str()))continue;
                ImGui::PushID(static_cast<int>(i));
                if(ImGui::Selectable(assets::character::displayName(a.name).c_str(),visible==i)){slot=i;changed=true;ImGui::CloseCurrentPopup();}
                ImGui::PopID();
            }ImGui::EndChild();ImGui::EndCombo();
        }ImGui::PopID();
    };
    drawSlot("Head",Part::Head,setup.headAsset);
    drawSlot("Helmet",Part::Headgear,setup.helmetAsset);
    drawSlot("Eyewear",Part::Eyewear,setup.eyewearAsset);
    auto body=current;drawSlot("Body",Part::Torso,body);
    if(body!=current){app.manualPlayerModelAsset=body;setup.playermodelAsset=body;app.autoPlayerModel=false;app.playermodelCustomGame=game;}
    drawSlot("Pants",Part::Pants,setup.pantsAsset);
    drawSlot("Exo",Part::Exo,setup.exoAsset);
    drawSlot("Gloves",Part::Gloves,setup.glovesAsset);
    drawSlot("Boots",Part::Boots,setup.bootsAsset);
    drawSlot("Gear",Part::Loadout,setup.loadoutAsset);
    drawSlot("Kneepads",Part::Kneepads,setup.kneepadAsset);
    drawSlot("Shinguards",Part::Shinguards,setup.shinguardAsset);
    if(changed)updatePlayermodelPreviewScene(app);
    if(ImGui::Button("Apply")){configureClassActor(app);updatePlayermodelPreviewScene(app);}
    ImGui::SameLine();if(ImGui::Button("Match viewhands"))matchViewhandsToCharacter(app);
    ImGui::SameLine();if(ImGui::Button("Reset view")){app.characterPreviewZoom=1;app.characterPreviewPan={};app.playermodelPreviewYaw=scene::kPi;app.playermodelPreviewPitch=.12f;}
    if(ImGui::TreeNode("Extra parts")){
        if(ImGui::BeginCombo("Add","Select")){
            for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game!=game||a.role==assets::Role::ViewWeapon||a.role==assets::Role::ViewHands)continue;
                ImGui::PushID(static_cast<int>(i));if(ImGui::Selectable(a.name.c_str())&&std::find(setup.customModularParts.begin(),setup.customModularParts.end(),i)==setup.customModularParts.end()){setup.customModularParts.push_back(i);updatePlayermodelPreviewScene(app);}ImGui::PopID();
            }ImGui::EndCombo();
        }
        for(std::size_t p=0;p<setup.customModularParts.size();){const auto i=setup.customModularParts[p];ImGui::PushID(static_cast<int>(p));
            if(ImGui::SmallButton("Remove")){setup.customModularParts.erase(setup.customModularParts.begin()+p);updatePlayermodelPreviewScene(app);}else{ImGui::SameLine();ImGui::TextUnformatted(i<app.assetCatalog.entries.size()?app.assetCatalog.entries[i].name.c_str():"Missing");++p;}ImGui::PopID();
        }ImGui::TreePop();
    }
    ImGui::PopID();ImGui::TreePop();
}
