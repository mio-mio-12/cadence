#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include "render/GlApi.h"

int main(int argc,char**argv){
 if(argc!=3||!glfwInit())return 1;
 const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);
 glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(760,940,"Character assembler audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& app=*state;app.window=w;app.defaultSalukiDirectory=argv[1];std::string error;
 if(!app.renderer.initialize(error))return 3;
 for(const auto game:{"aw","ghosts"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))return 4;
 std::ofstream report(out/"results.txt");
 if(app.cameraControls.zoomIntensity!=0)return 5;
 for(const auto game:{"aw","ghosts"}){
  app.autoPlayerModel=false;app.playermodelCustomGame=game;std::vector<std::size_t>bodies;
  for(std::size_t i=0;i<app.assetCatalog.entries.size();++i){const auto&a=app.assetCatalog.entries[i];if(a.game==game&&a.role==assets::Role::PlayerModel&&(a.game!="aw"||assets::character::awPart(a.name)==assets::character::Part::Torso))bodies.push_back(i);}
  if(bodies.empty())return 6;
  for(int sample=0;sample<3;++sample){
   app.manualPlayerModelAsset=bodies[(bodies.size()-1)*sample/2];
   const auto start=std::chrono::steady_clock::now();updatePlayermodelPreviewScene(app);
   const double firstMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   if(app.playermodelPreviewScene.meshes.empty()||!app.playermodelPreviewScene.animations.empty()||!app.botAnimationCache.empty())return 7;
   const auto docCount=app.characterPreviewDocuments.size();const auto warmStart=std::chrono::steady_clock::now();updatePlayermodelPreviewScene(app);const auto warmMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-warmStart).count();if(app.characterPreviewDocuments.size()!=docCount)return 8;
   for(const auto& bone:app.playermodelPreviewScene.skeleton.bones)for(float f:bone.restGlobal.v)if(!std::isfinite(f))return 9;
   for(int frame=0;frame<2;++frame){ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({760,940});ImGui::Begin("Character",nullptr,ImGuiWindowFlags_NoDecoration);ImGui::SetNextItemOpen(true,ImGuiCond_Always);drawPlayermodelAssembler(app);ImGui::End();ImGui::Render();glapi::BindFramebuffer(glapi::Framebuffer,0);glViewport(0,0,760,940);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();}
   std::vector<std::uint8_t>pixels(760*940*4);glReadPixels(0,0,760,940,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());if(!app.renderer.savePixelsPng(out/(std::string(game)+"_"+std::to_string(sample)+".png"),760,940,pixels,error))return 10;
   const auto&body=app.assetCatalog.entries[app.manualPlayerModelAsset];report<<"PASS "<<game<<" "<<body.name<<" parts="<<app.characterPreviewParts.size()<<" animationLoads=0 firstMs="<<firstMs<<" warmMs="<<warmMs<<" radius="<<app.characterPreviewRadius<<'\n';
   if(body.game=="aw"){
    auto grounded=app.playermodelPreviewScene;const auto torsoMin=grounded.bounds.minimum.z;scene::refreshCharacterBounds(grounded);
    report<<"GROUND torsoMin="<<torsoMin<<" assembledMin="<<grounded.bounds.minimum.z<<" correction="<<torsoMin-grounded.bounds.minimum.z<<'\n';
    if(grounded.bounds.minimum.z>5.f||grounded.bounds.minimum.z>=torsoMin)return 24;
   }
   const auto match=assets::character::matchHands(app.assetCatalog.entries,body,app.characterPreviewParts);
   if(match.base<app.assetCatalog.entries.size()){
    auto hands=scene::buildScene(cast::Document::load(app.assetCatalog.entries[match.base].path));
    for(auto index:match.parts){const auto doc=cast::Document::load(app.assetCatalog.entries[index].path);if(!doc.valid())return 20;const auto before=hands.meshes.size();scene::appendRigModel(doc,hands,app.assetCatalog.entries[index].name);if(hands.meshes.size()<=before)return 21;}
    for(const auto& mesh:hands.meshes)for(const auto& v:mesh.vertices)if(!std::isfinite(v.position.x)||!std::isfinite(v.position.y)||!std::isfinite(v.position.z))return 22;
    report<<"HANDS "<<app.assetCatalog.entries[match.base].name<<" parts="<<match.parts.size()<<" meshes="<<hands.meshes.size()<<'\n';
   }else report<<"HANDS unchanged: "<<match.error<<'\n';
   const auto signature=app.characterPreviewRendered;renderPlayermodelPreview(app,760,380);renderPlayermodelPreview(app,760,380);if(app.characterPreviewDirty)return 11;
  }
 }
 // Wheel events over the preview may not scroll its containing settings panel.
 const auto navFrame=[&](){ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({760,940});ImGui::Begin("Navigation test",nullptr,ImGuiWindowFlags_NoDecoration);drawCharacterPreviewNavigation(app,700,380);ImGui::Dummy({100,2000});const float scroll=ImGui::GetScrollY();ImGui::End();ImGui::Render();return scroll;};
 ImGui::GetIO().AddMousePosEvent(150,150);navFrame();navFrame();
 const auto zoom=app.characterPreviewZoom;ImGui::GetIO().AddMouseWheelEvent(0,-1);navFrame();const float scroll=navFrame();
 if(scroll!=0||app.characterPreviewZoom<=zoom)return 23;
 report<<"NAV wheel zoom passed; parentScroll="<<scroll<<'\n';
 // GPU regression: mask alpha must survive both texture upload paths while
 // the material itself remains opaque. Compare black/white mask halves.
 const auto tga=[&](const std::filesystem::path& path,bool camo){std::ofstream f(path,std::ios::binary);std::array<unsigned char,18>h{};h[2]=2;h[12]=8;h[14]=8;h[16]=32;h[17]=8;f.write(reinterpret_cast<const char*>(h.data()),h.size());for(int y=0;y<8;++y)for(int x=0;x<8;++x){unsigned char p[4]={static_cast<unsigned char>(camo?0:128),static_cast<unsigned char>(camo?0:128),static_cast<unsigned char>(camo?255:128),static_cast<unsigned char>(camo||x>=4?255:0)};f.write(reinterpret_cast<const char*>(p),4);}};
 tga(out/"mask.tga",false);tga(out/"camo.tga",true);
 scene::CastScene materialScene;scene::Mesh mesh;mesh.camoBlend=mesh.camoUseAlpha=mesh.viewmodelWeapon=true;mesh.albedoPath=out/"mask.tga";mesh.vertices.resize(4);mesh.vertices[0].position={-.9f,-.9f,0};mesh.vertices[1].position={.9f,-.9f,0};mesh.vertices[2].position={.9f,.9f,0};mesh.vertices[3].position={-.9f,.9f,0};mesh.vertices[0].uv={0,0};mesh.vertices[1].uv={1,0};mesh.vertices[2].uv={1,1};mesh.vertices[3].uv={0,1};mesh.indices={0,1,2,0,2,3};materialScene.meshes.push_back(mesh);
 // A second image request engages the bounded prepared-texture path.
 for(bool prepared:{false,true}){
  materialScene.meshes[0].normalPath=prepared?out/"camo.tga":std::filesystem::path{};
  if(!app.renderer.loadScene(materialScene,error))return 12;
  app.renderer.setDebugView(1);app.renderer.setIgnoreTextureAlpha(true,true);app.renderer.setCamoParameters(1,1,0,1,false);app.renderer.clearCamoTexture();
  app.renderer.render(materialScene,{},scene::Mat4::identity(),128,128,false,false,false);std::vector<std::uint8_t>base,painted;if(!app.renderer.readColorRgba(base,error))return 13;
  if(!app.renderer.setCamoTexture(out/"camo.tga",error))return 14;
  app.renderer.render(materialScene,{},scene::Mat4::identity(),128,128,false,false,false);if(!app.renderer.readColorRgba(painted,error))return 15;
  int left=0,right=0;for(int c=0;c<3;++c){left+=std::abs(int(base[(64*128+32)*4+c])-int(painted[(64*128+32)*4+c]));right+=std::abs(int(base[(64*128+96)*4+c])-int(painted[(64*128+96)*4+c]));}
  report<<"CAMO prepared="<<prepared<<" untouched="<<left<<" painted="<<right<<'\n';if(left>3||right<40)return 16;
  app.renderer.saveColorPng(out/(prepared?"camo_prepared.png":"camo_direct.png"),error);
 }
 std::cout<<"Character T-pose preview, cached documents, no animation loading, and zoom default passed\n";
 return 0;
}
