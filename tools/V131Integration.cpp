#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include "../src/render/GlApi.h"
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<'\n';std::exit(1);}}while(false)
static std::string bytes(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char**argv){
 CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);std::cout<<std::unitbuf;
 CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,700,"v131 validation",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
 auto state=std::make_unique<AppState>();auto& app=*state;std::string error;CHECK(app.renderer.initialize(error));app.window=window;
 app.filmContrast=1.37f;app.fogEnabled=true;app.actorOverlays.chams=true;app.transitionDuration=.27f;
 CHECK(saveVisualPreset(app,out/"before.castvisual"));CHECK(saveGameplayPreset(app,out/"before.cadencegame"));
 for(const auto& [name,text]:std::initializer_list<std::pair<const char*,const char*>>{
  {"truncated","CASTVISUAL 5\nfilm 1 0.5"},{"mixed","CASTVISUAL 5\nfog 0 0 0 0 1 20 .5 0\ncrosshair 1"},
  {"bad_order","CASTVISUAL 5\npost_order 0 1 2 3 4 5 9\n"},{"missing_sky","CASTVISUAL 5\nfilm 1 .2 4 0 0 1 1 1 1 1 1\nsky \"this-file-does-not-exist.hdr\"\n"}}){
   auto path=out/(std::string(name)+".castvisual");{std::ofstream f(path);f<<text;}CHECK(!loadVisualPreset(app,path));CHECK(saveVisualPreset(app,out/"after.castvisual"));CHECK(bytes(out/"before.castvisual")==bytes(out/"after.castvisual"));
 }
 {std::ofstream f(out/"truncated.cadencegame");f<<"CADENCEGAME 9\n.9 .8 1 1 50 1 1\n1 0";}
 CHECK(!loadGameplayPreset(app,out/"truncated.cadencegame"));CHECK(saveGameplayPreset(app,out/"after.cadencegame"));CHECK(bytes(out/"before.cadencegame")==bytes(out/"after.cadencegame"));
 app.filmContrast=.1f;CHECK(loadVisualPreset(app,out/"before.castvisual"));CHECK(app.filmContrast==1.37f);app.transitionDuration=.1f;CHECK(loadGameplayPreset(app,out/"before.cadencegame"));CHECK(app.transitionDuration==.27f);
 {std::ofstream f(out/"legacy.cadencegame");f<<"CADENCEGAME 1\n.3 .2 0 0 40 1 1\n";}CHECK(loadGameplayPreset(app,out/"legacy.cadencegame"));CHECK(app.transitionDuration==.3f&&app.sourceAutoJump);
 std::cout<<"PASS preset full roundtrips, legacy gameplay, truncated visual/gameplay rejection, missing sky atomic failure\n";
 const auto root=std::filesystem::path(cadence::local_assets::exportPath(""));
 int visualCount=0;for(const auto& file:std::filesystem::directory_iterator("Cadence Assets/visual_presets"))if(file.path().extension()==".castvisual"){std::ifstream in(file.path());VisualPresetResources resources;auto staged=std::make_unique<AppState>();copyVisualPresetSettings(*staged,app);CHECK(parseVisualPreset(*staged,in,resources));++visualCount;}
 CHECK(visualCount>0);std::cout<<"PASS parsed "<<visualCount<<" existing user visual presets without modifying originals\n";
 auto document=cast::Document::load(root/"bocw_sp/models/weapons/world/sniper rifles/sniper_standard/wpn_t9_sniper_standard_world/wpn_t9_sniper_standard_world_LOD0.cast");CHECK(document.valid());
 scene::CastScene original,prepared;scene::Bone bone;bone.name="tag_weapon_right";bone.parent=-1;original.skeleton.bones.push_back(bone);prepared=original;
 CHECK(scene::appendAttachment(document,original,0,"weapon")>0);auto imported=scene::buildScene(document);const auto skeletonSize=imported.skeleton.bones.size();CHECK(scene::appendPreparedAttachment(std::move(imported),prepared,0,"weapon")>0);CHECK(imported.skeleton.bones.size()==skeletonSize);
 CHECK(scene::sameRigGeometry(original,prepared));CHECK(original.attachments.size()==prepared.attachments.size());CHECK(original.attachments[0].localMatrix().v==prepared.attachments[0].localMatrix().v);
 for(std::size_t i=0;i<original.meshes.size();++i){CHECK(original.meshes[i].albedoPath==prepared.meshes[i].albedoPath);CHECK(original.meshes[i].materialName==prepared.meshes[i].materialName);CHECK(original.meshes[i].attachmentIndex==prepared.meshes[i].attachmentIndex);}
 std::cout<<"PASS actual CW world attachment: original vs reused geometry, binds, material paths and mount matrices identical\n";
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(window,false);ImGui_ImplOpenGL3_Init("#version 330");
 float firstWidth=0;
 for(int i=0;i<4;++i){glapi::BindFramebuffer(glapi::Framebuffer,0);glClearColor(.04f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({i%2?700.f:420.f,320});ImGui::Begin("Levels stable layout",nullptr,ImGuiWindowFlags_NoSavedSettings);app.histogramUpdated=glfwGetTime();for(std::size_t b=0;b<app.frameHistogram.size();++b)app.frameHistogram[b]=std::exp(-std::pow((float(b)-110.f)/55.f,2.f));drawLevelsHistogram(app);const float width=ImGui::GetItemRectSize().x;if(i==0)firstWidth=width;else CHECK(width==firstWidth);ImGui::End();ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());if(i>=2){std::vector<std::uint8_t> pixels(960*700*4);glReadPixels(0,0,960,700,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());CHECK(app.renderer.savePixelsPng(out/("levels_"+std::to_string(i)+".png"),960,700,pixels,error));}}
 std::cout<<"PASS Levels histogram/control group width remains "<<firstWidth<<" across changing host widths\n";
 ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();app.renderer.shutdown();state.reset();glfwDestroyWindow(window);glfwTerminate();
}
