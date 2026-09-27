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
    state->weather=render::weather::preset(1);state->weather.lens.enabled=false;state->weather.lens.dispersion=.32f;state->weather.seed=-391;state->weather.dustAerosolDetail=233;state->weather.dustAerosolAmount=2.5f;
    const auto weatherText=[](const auto& s){std::ostringstream out;out<<std::setprecision(6)<<s.weather;return out.str();};
    CHECK(saveVisualPreset(*state,out/"rain.castvisual"));VisualPresetResources resources;
    std::ifstream saved(out/"rain.castvisual");CHECK(parseVisualPreset(*other,saved,resources));
    CHECK(extendedText(*state)==extendedText(*other));
    CHECK(weatherText(*state)==weatherText(*other));
    CHECK(other->weather.dustAerosolDetail==233&&other->weather.dustAerosolAmount==2.5f);
    CHECK(saveWeatherPreset(*state,out/"test.castweather"));CHECK(loadWeatherPreset(*other,out/"test.castweather"));CHECK(weatherText(*state)==weatherText(*other));
    const auto weatherGroup=[](const AppState& a){std::ostringstream o;o<<std::setprecision(std::numeric_limits<float>::max_digits10)<<a.weather<<' '<<a.rain<<' '<<a.rain.style<<' '<<a.rain.blockerDistance<<' '<<a.wet<<' '<<a.wet.detail;return o.str();};
    CHECK(weatherGroup(*state)==weatherGroup(*other));
    {std::ofstream legacy(out/"legacy.castweather");legacy<<"CASTWEATHER 2\n"<<state->weather<<' '<<state->weather.dustAerosolDetail<<' '<<state->weather.dustAerosolAmount<<' '<<state->weather.dustAerosolContrast;}
    other->rain.density=7.25f;other->wet.amount=2.75f;
    CHECK(loadWeatherPreset(*other,out/"legacy.castweather"));CHECK(other->rain.density==7.25f&&other->wet.amount==2.75f);
    const auto beforeBroken=weatherGroup(*other);
    {std::ifstream src(out/"test.castweather");std::string bytes(std::istreambuf_iterator<char>(src),{});std::ofstream bad(out/"broken.castweather");bad<<bytes.substr(0,bytes.size()/2);}
    CHECK(!loadWeatherPreset(*other,out/"broken.castweather"));CHECK(weatherGroup(*other)==beforeBroken);
    CHECK(loadWeatherPreset(*other,out/"test.castweather"));
    CHECK(other->weather.dustAerosolDetail==233&&other->weather.dustAerosolAmount==2.5f);
    CHECK(resources.visualImages&&other->camoPath==state->camoPath&&other->specularImperfectionsPath==state->specularImperfectionsPath);
    CHECK(other->hitmarkerTexturePath==state->hitmarkerTexturePath&&other->killmarkerTexturePath==state->killmarkerTexturePath&&other->weaponProfile.scopeOverlayImage==state->weaponProfile.scopeOverlayImage);
    auto copied=std::make_unique<AppState>();copyVisualPresetSettings(*copied,*other);CHECK(extendedText(*copied)==extendedText(*state));
    forEachExtendedVisualGroup(*state,[&](const char* key,auto){std::istringstream broken(std::string("CASTVISUAL 5\n")+key+" nope\n");VisualPresetResources r;if(parseVisualPreset(*copied,broken,r)){std::cerr<<"Accepted malformed group "<<key<<'\n';std::exit(1);}});
    std::cout<<"PASS extended visual groups, image paths, transactional copy and malformed groups"<<std::endl;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={640,480};io.DeltaTime=1.f/60;unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);ImGui::NewFrame();
    ImGui::SetNextWindowSize({600,460});ImGui::Begin("Weather layout");
    ImGui::SetNextItemOpen(true);drawWeatherControls(*state);
    ImGui::End();
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
    CHECK(!other->weather.active());
    // Weather presets are presentation state; changing them must not touch take bytes.
    state->recordedTake.boneCount=1;state->recordedTake.sampleRate=30;
    for(int i=0;i<=30;++i){take::Sample sample;sample.time=i/30.f;sample.pose.push_back(scene::Mat4::identity());state->recordedTake.samples.push_back(sample);}
    std::string takeError;CHECK(take::save(state->recordedTake,out/"weather_before.c_dm",takeError));
    state->weather=render::weather::preset(6);state->weather.seed=719;state->weather.offset=4;state->weather.timeScale=.2f;
    CHECK(take::save(state->recordedTake,out/"weather_after.c_dm",takeError));
    const auto bytes=[](const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),{});};
    CHECK(bytes(out/"weather_before.c_dm")==bytes(out/"weather_after.c_dm"));
    std::cout<<"PASS weather changes leave serialized take bytes identical"<<std::endl;
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
