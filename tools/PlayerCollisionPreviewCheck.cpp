#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include "render/GlApi.h"

static void previewCheck(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    GLFWwindow* window=nullptr;bool gui=false;
    try{
        previewCheck(argc==2,"usage: player_collision_preview_check NEW_OUTPUT_DIRECTORY");
        const std::filesystem::path output=argv[1];previewCheck(!std::filesystem::exists(output),"output must be new");std::filesystem::create_directories(output);
        previewCheck(glfwInit()!=0,"GLFW initialization failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        window=glfwCreateWindow(1280,720,"Player clip preview diagnostic",nullptr,nullptr);previewCheck(window!=nullptr,"window failed");glfwMakeContextCurrent(window);
        {
            auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;std::string error;previewCheck(app.renderer.initialize(error),error.c_str());
            ImGui::CreateContext();gui=true;ImGui::GetIO().IniFilename=nullptr;ImGui::StyleColorsDark();
            ImGui_ImplGlfw_InitForOpenGL(window,false);ImGui_ImplOpenGL3_Init("#version 330");
            app.actorMode=true;app.actorPosition={0,0,30};app.actorYaw=0;app.movementAlgorithm=4;app.loadedMap.emplace();
            // Flat ground and a higher roof: preview must choose ground, not roof/camera elevation.
            for(float z:{30.f,300.f})for(int i=0;i<2;++i){scene::glb::CollisionTriangle t;
                const scene::Vec3 a{-1000,-1000,z},b{1000,-1000,z},c{1000,1000,z},d{-1000,1000,z};
                t.a=a;t.b=i?c:b;t.c=i?d:c;t.minimum={-1000,-1000,z};t.maximum={1000,1000,z};t.normal={0,0,1};t.walkable=true;app.loadedMap->collision.push_back(t);}
            app.loadedMap->buildCollisionIndex();
            const float ground=actorGroundHeight(app,200,0,app.actorPosition.z+scene::course::kStepHeight-45.f);previewCheck(std::abs(ground-30)<.001f,"preview ground selected roof");
            const ImVec2 viewportImageMin{0,0},viewportImageMax{820,720};
            const auto viewProjection=scene::perspective(55*scene::kPi/180,820.f/720,.1f,3000)*scene::lookAt({-130,-330,240},{140,0,95},{0,0,1});
            const auto projectPoint=[&](scene::Vec3 p)->std::optional<ImVec2>{
                const float x=viewProjection.v[0]*p.x+viewProjection.v[4]*p.y+viewProjection.v[8]*p.z+viewProjection.v[12];
                const float y=viewProjection.v[1]*p.x+viewProjection.v[5]*p.y+viewProjection.v[9]*p.z+viewProjection.v[13];
                const float w=viewProjection.v[3]*p.x+viewProjection.v[7]*p.y+viewProjection.v[11]*p.z+viewProjection.v[15];
                if(w<=.0001f||std::abs(x/w)>1||std::abs(y/w)>1)return std::nullopt;
                return ImVec2{(x/w*.5f+.5f)*820,(1-(y/w*.5f+.5f))*720};};
            std::ofstream results(output/"results.csv");results<<"case,standing_cm,current_cm,ground_z,center_x,width_override,depth_override,preview_vertices\n";
            const char* names[]={"standing140","standing180","crouch70","preview_off","narrow_deep"};
            for(int mode=0;mode<5;++mode){
                app.playerClipWidthCm=mode==4?35.f:0.f;app.playerClipDepthCm=mode==4?110.f:0.f;
                app.playerClipHeightCm=mode==1?180.f:140.f;app.gameplayStance=mode==2?scene::Stance::Crouch:scene::Stance::Stand;app.playerClipPreview=mode!=3;
                const float expected=mode==1?180.f:mode==2?70.f:140.f;previewCheck(std::abs(playerCurrentCollisionHeight(app,app.gameplayStance)-expected)<.001f,"actual collision height mismatch");
                // Two frames allow Dear ImGui window sizing/item layout to settle.
                int previewVertices=0;
                for(int frame=0;frame<2;++frame){
                    glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
                    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({820,720});
                    ImGui::Begin("Preview",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
                    auto* feedbackDraw=ImGui::GetWindowDrawList();
                    const auto line=[&](scene::Vec3 a,scene::Vec3 b){const auto p=projectPoint(a),q=projectPoint(b);if(p&&q)feedbackDraw->AddLine(*p,*q,IM_COL32(65,70,75,255));};
                    for(int n=-4;n<=6;++n){line({float(n*50),-200,30},{float(n*50),200,30});line({-200,float(n*50),30},{300,float(n*50),30});}
                    if(const auto feet=projectPoint(app.actorPosition)){feedbackDraw->AddCircle(*feet,5,IM_COL32(255,200,60,255));feedbackDraw->AddText(*feet,IM_COL32(255,200,60,255),"Player feet");}
                    const int before=feedbackDraw->VtxBuffer.Size;
#include "app/PlayerCollisionPreview.inc"
                    previewVertices=feedbackDraw->VtxBuffer.Size-before;
                    ImGui::End();
                    ImGui::SetNextWindowPos({820,0});ImGui::SetNextWindowSize({460,720});ImGui::Begin("Player",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
                    drawActorControls(app);ImGui::End();ImGui::Render();
                    glapi::BindFramebuffer(glapi::Framebuffer,0);glViewport(0,0,1280,720);glClearColor(.06f,.07f,.08f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                }
                previewCheck((mode==3)==(previewVertices==0),"preview checkbox failed");previewCheck(app.actorPosition.x==0&&app.actorPosition.y==0&&app.actorPosition.z==30,"preview moved player");
                std::vector<std::uint8_t> pixels(1280*720*4);glReadBuffer(GL_BACK);glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                previewCheck(app.renderer.savePixelsPng(output/(std::string(names[mode])+".png"),1280,720,pixels,error),error.c_str());
                results<<names[mode]<<','<<app.playerClipHeightCm<<','<<expected<<','<<ground<<",200,"<<app.playerClipWidthCm<<','<<app.playerClipDepthCm<<','<<previewVertices<<'\n';
            }
            ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();gui=false;
        }
        glfwDestroyWindow(window);glfwTerminate();std::cout<<"PASS preview geometry/checkbox checks; PNGs require visual review\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(gui){ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();}if(window)glfwDestroyWindow(window);glfwTerminate();return 1;}
}
