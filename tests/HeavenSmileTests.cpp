#include "render/HeavenSmile.h"
#include <iostream>
int main(){using namespace render::heaven_smile;int failed{};const auto check=[&](bool ok){if(!ok)++failed;};
    Burst burst{{10,20,30},.5f,170,7};const auto a=shards(burst),b=shards(burst);check(a.size()==96&&b.size()==96);
    for(size_t i=0;i<a.size();++i){check(scene::length(a[i].center-b[i].center)==0);check(std::isfinite(a[i].radius)&&a[i].radius>0);check(a[i].alpha>=0&&a[i].alpha<=1);}
    burst.age=-1;check(shards(burst).empty());burst.age=2;check(shards(burst).empty());
    burst.age=.5f;burst.seed=8;check(scene::length(shards(burst)[0].center-a[0].center)>1);
    const auto jets=streams(burst),repeat=streams(burst);check(jets.size()==120&&repeat.size()==jets.size());
    for(size_t i=0;i<jets.size();++i){check(scene::length(jets[i].start-repeat[i].start)==0);check(jets[i].radius>0&&std::isfinite(jets[i].end.z));}
    burst.age=2;check(streams(burst).empty());burst.age=-1;check(streams(burst).empty());
    std::cout<<"Death effect failures: "<<failed<<'\n';return failed?1:0;
}
