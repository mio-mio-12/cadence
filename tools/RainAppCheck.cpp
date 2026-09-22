#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(int argc,char** argv){
    // Optional map and muzzle-model arguments are independent fixtures.
    CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    if(argc>2){scene::glb::Map map;std::string error;CHECK(scene::c2m::load(argv[2],map,error));CHECK(!map.scene.meshes.empty());for(const auto& m:map.scene.meshes){CHECK(!m.weatherNonBlocking);CHECK(m.materialName.find("nomipmap_MP_Shipment_Night")==std::string::npos);}CHECK(map.shotGeometry);std::size_t openings=0,solids=0;for(std::size_t i=0;i<map.collision.size();i+=std::max(std::size_t(1),map.collision.size()/500)){const auto& t=map.collision[i];const auto origin=(t.a+t.b+t.c)/3.f+t.normal*50.f;if(map.raycastSurface(origin,t.normal*-1.f,100.f)){if(map.raycastShot(origin,t.normal*-1.f,100.f))++solids;else ++openings;}}std::cout<<"Map imported surfaces="<<map.scene.meshes.size()<<"; authored sky omitted; sampled movement faces with solid shot surface="<<solids<<", open shot path="<<openings<<'\n';}
    auto state=std::make_unique<AppState>(),other=std::make_unique<AppState>();
    state->rain=render::rain::preset(4);state->rain.freezeTime=2.75f;state->rain.animate=false;
    state->rain.radius=73;state->rain.brightness=19;state->rain.seed=-4;
    state->rain.blockerDistance=123.5f;
    state->wet.ground=true;state->wet.enabled=true;state->wet.viewmodels=false;state->wet.dropSize=7;
    state->wet.detail.viewhands=false;state->wet.detail.edgeDetail=91;state->wet.detail.reflection=7;
    state->rain.style.refractive=true;state->rain.style.ior=1.45f;state->rain.style.grainScale=300;state->rain.style.dropCount=12;
    state->muzzleLight.enabled=true;state->muzzleLight.shape=2;state->muzzleLight.radius=345;
    forEachExtendedVisualGroup(*state,[&](const char*,auto refs){std::apply([](auto&... value){
        const auto change=[](auto& v){using T=std::remove_cvref_t<decltype(v)>;if constexpr(std::is_same_v<T,bool>)v=!v;else if constexpr(std::is_integral_v<T>)v+=1;else v+=.125f;};(change(value),...);
    },refs);});
    state->camoPath="images/camo with spaces.png";state->specularImperfectionsPath="images/imperfections.png";
    state->hitmarkerTexturePath="images/hit.png";state->killmarkerTexturePath="images/kill.png";state->weaponProfile.scopeOverlayImage="images/scope.png";
    const auto extendedText=[](auto& app){std::ostringstream stream;stream<<std::setprecision(9);forEachExtendedVisualGroup(app,[&](const char* key,auto values){stream<<key<<' ';writeVisualValues(stream,values);});return stream.str();};
    CHECK(saveVisualPreset(*state,out/"rain.castvisual"));VisualPresetResources resources;
    std::ifstream saved(out/"rain.castvisual");CHECK(parseVisualPreset(*other,saved,resources));
    CHECK(extendedText(*state)==extendedText(*other));
    CHECK(resources.visualImages&&other->camoPath==state->camoPath&&other->specularImperfectionsPath==state->specularImperfectionsPath);
    CHECK(other->hitmarkerTexturePath==state->hitmarkerTexturePath&&other->killmarkerTexturePath==state->killmarkerTexturePath&&other->weaponProfile.scopeOverlayImage==state->weaponProfile.scopeOverlayImage);
    auto copied=std::make_unique<AppState>();copyVisualPresetSettings(*copied,*other);CHECK(extendedText(*copied)==extendedText(*state));
    forEachExtendedVisualGroup(*state,[&](const char* key,auto){std::istringstream broken(std::string("CASTVISUAL 5\n")+key+" nope\n");VisualPresetResources r;if(parseVisualPreset(*copied,broken,r)){std::cerr<<"Accepted malformed group "<<key<<'\n';std::exit(1);}});
    std::cout<<"PASS extended visual groups, image paths, transactional copy and malformed groups"<<std::endl;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={640,480};io.DeltaTime=1.f/60;unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);ImGui::NewFrame();
    auto& drawing=*ImGui::GetForegroundDrawList();drawing.PushTextureID(io.Fonts->TexID);drawing.PushClipRectFullScreen();
    state->hitmarkerEnabled=false;drawHitmarker(*state,&drawing,{100,100},1,.13f);CHECK(drawing.VtxBuffer.empty());
    state->hitmarkerEnabled=true;state->hitmarkerProcedural=true;
    for(int kind=1;kind<=3;++kind){const int before=drawing.VtxBuffer.Size;drawHitmarker(*state,&drawing,{100,100},kind,.06f);CHECK(drawing.VtxBuffer.Size>before);}
    for(const auto& v:drawing.VtxBuffer)CHECK(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y));
    drawing.PopClipRect();drawing.PopTextureID();ImGui::EndFrame();ImGui::DestroyContext();
    std::cout<<"PASS disabled marker, procedural hit/headshot/kill geometry"<<std::endl;
    CHECK(other->rain.enabled&&other->rain.wind==8&&other->rain.direction==90&&other->rain.wallMist);
    CHECK(other->rain.freezeTime==2.75f&&!other->rain.animate&&other->rain.turbulence==1.2f);
    CHECK(other->rain.radius==73&&other->rain.brightness==19&&other->rain.seed==-4);
    CHECK(other->rain.blockerDistance==123.5f);
    CHECK(other->wet.ground&&other->wet.enabled&&!other->wet.viewmodels&&other->wet.dropSize==7);
    CHECK(!other->wet.detail.viewhands&&other->wet.detail.edgeDetail==91&&other->wet.detail.reflection==7);
    CHECK(other->rain.style.refractive&&other->rain.style.ior==1.45f&&other->rain.style.grainScale==300&&other->rain.style.dropCount==12);
    CHECK(other->muzzleLight.enabled&&other->muzzleLight.shape==2&&other->muzzleLight.radius==345);
    other->rain={};copyVisualPresetSettings(*other,*state);CHECK(other->rain.enabled&&other->rain.wind==8);
    std::istringstream legacy("CASTVISUAL 5\n");CHECK(parseVisualPreset(*other,legacy,resources));CHECK(!other->rain.enabled);
    CHECK(other->rain.blockerDistance==50);
    CHECK(!other->wet.ground&&!other->wet.enabled&&!other->muzzleLight.enabled);
    // Invalid load must not partially publish the new settings to the app.
    CHECK(!loadVisualPreset(*state,out/"missing.castvisual"));CHECK(state->rain.wind==8);
    {std::ofstream broken(out/"invalid.castvisual");broken<<"CASTVISUAL 5\nhitmarker_shape nope\n";}
    const auto beforeInvalid=extendedText(*state);
    CHECK(!loadVisualPreset(*state,out/"invalid.castvisual"));CHECK(extendedText(*state)==beforeInvalid);
    CHECK(render::rain::phase(1,state->rain)==render::rain::phase(8,state->rain));
    if(argc>3){
        const auto document=cast::Document::load(argv[3]);CHECK(document.valid());
        scene::CastScene rig;CHECK(scene::appendRigModel(document,rig,"wpn_h1_pst_de50_vm_camo")>0);
        CHECK(!rig.rigParts.empty()&&rig.rigParts.back().muzzleParent);
        std::vector<scene::Mat4> pose;for(const auto& bone:rig.skeleton.bones)pose.push_back(bone.restGlobal);
        const auto original=scene::resolveMuzzlePosition(rig,pose);CHECK(original);
        const auto flash=rig.skeleton.boneByCanonicalName.at("tag_flash");pose[flash]=scene::translation({-2.1749f,.21488f,-.22204f});
        const auto corrected=scene::resolveMuzzlePosition(rig,pose);CHECK(corrected&&scene::length(*corrected-*original)<.0001f);
        std::cout<<"MWR model muzzle: "<<corrected->x<<","<<corrected->y<<","<<corrected->z<<"; conflicting flash clip ignored\n";
    }
    std::cout<<"PASS app visual save/parse/copy, legacy preset rain-off default, failed load preservation, frozen clock\n";
}
