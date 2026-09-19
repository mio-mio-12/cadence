#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace render {
// Apply after the existing texture/VAO order at map upload. These GPU material
// keys do not change until the map is uploaded again. Equal keys preserve the
// old texture order; legacy-only passes must remain completely untouched.
template<class Meshes>
void sortStaticOpaqueMaterialPass(std::vector<std::size_t>& pass,const Meshes& meshes){
    if(std::none_of(pass.begin(),pass.end(),[&](std::size_t i){return meshes[i].materialPolicyExplicit;}))return;
    std::stable_sort(pass.begin(),pass.end(),[&](std::size_t a,std::size_t b){
        const auto& x=meshes[a];const auto& y=meshes[b];
        const int xq=x.renderQueue<0?2000:x.renderQueue,yq=y.renderQueue<0?2000:y.renderQueue;
        if(xq!=yq)return xq<yq;
        return x.alphaTest<y.alphaTest;
    });
}
}
