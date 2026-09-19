#include "render/StaticMaterialOrder.h"
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>

struct Mesh {bool materialPolicyExplicit{};int renderQueue{-1};bool alphaTest{};};
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void oldFrameOrder(std::vector<std::size_t>& pass,const std::vector<Mesh>& meshes){
    if(std::none_of(pass.begin(),pass.end(),[&](auto i){return meshes[i].materialPolicyExplicit;}))return;
    std::stable_sort(pass.begin(),pass.end(),[&](auto a,auto b){
        const auto& x=meshes[a];const auto& y=meshes[b];
        const int xq=x.renderQueue<0?2000:x.renderQueue,yq=y.renderQueue<0?2000:y.renderQueue;
        if(xq!=yq)return xq<yq;return x.alphaTest<y.alphaTest;
    });
}
int main(){try{
    std::mt19937 random(88);
    for(int scenario=0;scenario<40;++scenario){
        std::vector<Mesh> meshes(scenario==0?0:1000);
        for(auto& m:meshes){m.materialPolicyExplicit=scenario>1&&random()%3==0;m.renderQueue=random()%5==0?-1:1900+100*(random()%6);m.alphaTest=random()%2!=0;}
        std::vector<std::size_t> uploaded(meshes.size());std::iota(uploaded.begin(),uploaded.end(),0);std::shuffle(uploaded.begin(),uploaded.end(),random);
        const auto input=uploaded;auto old=uploaded;render::sortStaticOpaqueMaterialPass(uploaded,meshes);
        for(int frame=0;frame<30;++frame){oldFrameOrder(old,meshes);check(old==uploaded,"upload order differs from original frame order");}
        if(scenario<2)check(uploaded==input,"legacy or empty pass reordered");
        for(auto& m:meshes)m.renderQueue=2600-m.renderQueue; // re-upload edited material policy
        oldFrameOrder(old,meshes);render::sortStaticOpaqueMaterialPass(uploaded,meshes);check(old==uploaded,"material re-upload mismatch");
    }
    std::cout<<"Opaque order: empty/legacy/mixed policies, stable ties, 1200 frame comparisons and re-upload PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
