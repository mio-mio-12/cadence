#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <cmath>
static void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    GLFWwindow* window=nullptr;
    try{
        check(glfwInit()!=0,"GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        window=glfwCreateWindow(320,240,"Albedo validation",nullptr,nullptr);check(window!=nullptr,"window");glfwMakeContextCurrent(window);
        {
        render::StageRenderer r;std::string error;check(r.initialize(error),error);
        scene::CastScene scene;scene::Mesh mesh;mesh.name="albedo_fixture";mesh.color={.3f,.6f,.2f,1};mesh.doubleSided=true;
        for(auto p:{scene::Vec3{-50,0,-50},scene::Vec3{50,0,-50},scene::Vec3{50,0,50},scene::Vec3{-50,0,50}}){scene::Vertex v;v.position=p;v.normal={0,-1,0};mesh.vertices.push_back(v);}
        mesh.indices={0,1,2,0,2,3};scene.meshes.push_back(mesh);scene.bounds.valid=true;scene.bounds.minimum={-50,0,-50};scene.bounds.maximum={50,0,50};
        check(r.loadScene(scene,error),error);const scene::Vec3 eye{0,-180,0};r.setCameraPosition(eye);r.setCameraDepthRange(1,1000);
        const auto vp=scene::perspective(60*scene::kPi/180,4.f/3,1,1000)*scene::lookAtDirection(eye,{0,1,0},{0,0,1});
        auto draw=[&](int mode){r.setDebugView(mode);r.render(scene,{},vp,320,240,false,false,false);glFinish();std::vector<std::uint8_t> p;check(r.readColorRgba(p,error),error);check(glGetError()==GL_NO_ERROR,"GL error");return p;};
        const auto clean=draw(1);
        const auto center=(120*320+160)*4;check(clean[center]>65&&clean[center]<90&&clean[center+1]>140&&clean[center+1]<165,"unlit base color");
        r.setSun(true,true,{.2f,.3f,-1},5,.01f,{1,0,0},{0,0,1},256,1000,800,{},true,256,800,2000,100);
        r.setFog(true,{1,0,1},0,10,1,1);r.setFilmTweaks(true,.3f,2,1,{1,0,0},{0,1,0},{0,0,1},true,true);
        r.setLensTweaks(true,.1f,.5f,0,1,0,2,true,1,.1f,.5f);r.setPostEffects(3,.2f,1,true,1,true,1);r.setTonemapping(2);
        render::HbaoSettings ao;ao.enabled=true;r.setHbao(ao);render::DepthOfFieldSettings dof;dof.enabled=true;r.setDepthOfField(dof);
        render::rain::Settings rain;rain.enabled=true;r.setRain(rain,2);render::water::Settings water;water.enabled=true;r.setWater(water,2);
        render::weather::Settings weather;weather.snow.enabled=true;weather.dust.enabled=true;weather.fog.enabled=true;r.setWeather(weather,2);
        check(draw(1)==clean,"Albedo contaminated by Beauty effects");
        const auto beauty=draw(0);check(beauty!=clean,"Beauty not resumed");check(draw(1)==clean,"Beauty-to-Albedo state leaked");check(draw(0)==beauty,"stored Beauty preset changed");
        r.setCollisionOverlay(nullptr,30,true);r.setDynamicCollisionOverlay({{{0,-10,-40},20,80,{1,0,0}}});
        const auto legacyHull=draw(10);check(legacyHull!=clean,"dynamic collision hull missing without map");
        if(argc>1){std::filesystem::create_directories(argv[1]);check(r.saveColorPng(std::filesystem::path(argv[1])/"collision.png",error),error);}
        r.setDynamicCollisionOverlay({{{0,-10,-40},20,80,{1,0,0},20,20}});
        check(draw(10)==legacyHull,"explicit square extents changed legacy radius overlay");
        const auto redBounds=[](const std::vector<std::uint8_t>& pixels){std::array<int,4> bounds{320,240,-1,-1};
            for(int y=0;y<240;++y)for(int x=0;x<320;++x){const auto i=(y*320+x)*4;if(pixels[i]>150&&pixels[i+1]<90&&pixels[i+2]<90){bounds[0]=std::min(bounds[0],x);bounds[1]=std::min(bounds[1],y);bounds[2]=std::max(bounds[2],x);bounds[3]=std::max(bounds[3],y);}}
            return bounds;};
        const auto rectangular=[&](float halfWidth,float halfDepth,const char* filename){
            r.setDynamicCollisionOverlay({{{0,-10,-40},0,80,{1,0,0},halfWidth,halfDepth}});const auto pixels=draw(10);
            // Verify projected corners are actually rasterized, not merely that
            // a struct contains the requested dimensions. Collision is world-axis.
            for(float x:{-halfWidth,halfWidth})for(float y:{-10-halfDepth,-10+halfDepth})for(float z:{-40.f,40.f}){
                const float cx=vp.v[0]*x+vp.v[4]*y+vp.v[8]*z+vp.v[12],cy=vp.v[1]*x+vp.v[5]*y+vp.v[9]*z+vp.v[13],cw=vp.v[3]*x+vp.v[7]*y+vp.v[11]*z+vp.v[15];
                const int px=static_cast<int>((cx/cw*.5f+.5f)*320),py=static_cast<int>((cy/cw*.5f+.5f)*240);bool found=false;
                for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx){const int sx=px+dx,sy=py+dy;if(sx<0||sx>=320||sy<0||sy>=240)continue;const auto i=(sy*320+sx)*4;found|=pixels[i]>150&&pixels[i+1]<90&&pixels[i+2]<90;}
                check(found,"rectangular collision corner not rendered at independent X/Y extent");
            }
            if(argc>1)check(r.saveColorPng(std::filesystem::path(argv[1])/filename,error),error);
            return redBounds(pixels);
        };
        const auto narrow=rectangular(15,8,"collision_rect_narrow.png"),wide=rectangular(35,8,"collision_rect_wide.png"),deep=rectangular(15,45,"collision_rect_deep.png");
        check(wide[2]-wide[0]>(narrow[2]-narrow[0])*1.8f&&std::abs(wide[1]-narrow[1])<=1&&std::abs(wide[3]-narrow[3])<=1,"width-only overlay change altered depth/height projection");
        check(deep[3]-deep[1]>(narrow[3]-narrow[1])*1.15f,"depth-only overlay change did not alter projected near/far edges");
        r.setDynamicCollisionOverlay({{{0,-10,-40},20,80,{1,0,0},-1,20}});check(draw(10)==clean,"invalid rectangular extent rendered");
        r.setDynamicCollisionOverlay({});check(draw(10)==clean,"cleared collision hull remained");
        for(int mode:{0,1}){draw(mode);auto start=std::chrono::steady_clock::now();for(int i=0;i<12;++i)draw(mode);std::cout<<(mode?"Albedo":"Beauty")<<" synthetic completed-frame ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/12<<'\n';}
        if(argc>1){std::filesystem::create_directories(argv[1]);draw(1);check(r.saveColorPng(std::filesystem::path(argv[1])/"albedo.png",error),error);draw(0);check(r.saveColorPng(std::filesystem::path(argv[1])/"beauty.png",error),error);}
        // Generated textures exercise actual texture upload/sRGB decoding, not just uniform colors.
        const auto fixtureDir=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/("cadence-albedo-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(fixtureDir);
        std::vector<std::uint8_t> texels(8*8*4);
        for(int y=0;y<8;++y)for(int x=0;x<8;++x){const auto i=(y*8+x)*4;texels[i]=128;texels[i+1]=64;texels[i+2]=192;texels[i+3]=x<4?0:255;}
        const auto albedoPath=fixtureDir/"fixture_srgb_cutout.png";
        check(r.savePixelsPng(albedoPath,8,8,texels,error),error);
        auto& textured=scene.meshes.front();textured.albedoPath=albedoPath;textured.color={1,1,1,1};textured.materialPolicyExplicit=true;textured.gltfPbr=true;textured.alphaTest=true;textured.alphaCutoff=.5f;
        textured.vertices[0].uv={0,0};textured.vertices[1].uv={1,0};textured.vertices[2].uv={1,1};textured.vertices[3].uv={0,1};
        check(r.loadScene(scene,error),error);
        auto colorCount=[](const std::vector<std::uint8_t>& pixels,int red,int green,int blue){std::size_t n=0;for(std::size_t i=0;i<pixels.size();i+=4)if(std::abs(int(pixels[i])-red)<=2&&std::abs(int(pixels[i+1])-green)<=2&&std::abs(int(pixels[i+2])-blue)<=2)++n;return n;};
        const auto cutout=draw(1);const auto cutoutCount=colorCount(cutout,128,64,192);
        check(cutoutCount>1000,"explicit sRGB textured Albedo does not round-trip display RGB");
        check(r.saveColorPng(fixtureDir/"albedo_srgb_cutout.png",error),error);
        textured.alphaTest=false;check(r.loadScene(scene,error),error);const auto opaque=draw(1);
        check(colorCount(opaque,128,64,192)>cutoutCount*1.7,"alpha-test holes were not preserved in Albedo");
        std::size_t holes=0;for(std::size_t i=0;i<cutout.size();i+=4)if(cutout[i]==cutout[0]&&cutout[i+1]==cutout[1]&&cutout[i+2]==cutout[2]&&opaque[i]!=cutout[i])++holes;
        check(holes>1000,"cutout did not expose the background");
        // Legacy camo is display-encoded, unlike explicit glTF albedo; test that contract separately.
        for(std::size_t i=0;i<texels.size();i+=4){texels[i]=32;texels[i+1]=200;texels[i+2]=80;texels[i+3]=255;}
        const auto camoPath=fixtureDir/"fixture_camo.png";check(r.savePixelsPng(camoPath,8,8,texels,error),error);
        textured.materialPolicyExplicit=false;textured.gltfPbr=false;textured.alphaTest=true;textured.camoBlend=true;textured.camoUseAlpha=false;
        check(r.loadScene(scene,error),error);check(r.setCamoTexture(camoPath,error),error);r.setCamoParameters(1,1,0,1,false);
        const auto camouflaged=draw(1);check(colorCount(camouflaged,32,200,80)>1000,"Albedo lost camo surface color");
        check(r.saveColorPng(fixtureDir/"albedo_camo_cutout.png",error),error);
        draw(0);check(draw(1)==camouflaged,"textured camo Albedo changed after Beauty round-trip");
        std::cout<<"Textured sRGB, cutout and camo checks passed\n";
        }
        glfwDestroyWindow(window);glfwTerminate();std::cout<<"Albedo effect isolation, Beauty restoration and dynamic hull checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';if(window)glfwDestroyWindow(window);glfwTerminate();return 1;}
}
