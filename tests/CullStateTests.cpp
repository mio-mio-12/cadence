#include "render/CullState.h"
#include <cstdlib>
#include <initializer_list>
#define CHECK(x) do{if(!(x))std::abort();}while(false)
int main(){
  for(bool cache:{false,true}){
    bool actual=false;int face=1;render::CullCounters counts;
    render::CullState state{cache,true,false,-1,counts};
    const auto get=[&]{return state.get([&]{return actual;});};
    const auto set=[&](bool x){state.set(x,[&](bool v){actual=v;});};
    const auto mode=[&](int x){state.setFace(x,[&](int v){face=v;});};
    CHECK(!get());CHECK(face==1); // inherited mode remains untouched
    set(true);mode(2); // opaque batch
    for(int i=0;i<20;++i){bool before=get();set(true);mode(2);CHECK(actual&&face==2);set(before);}
    bool before=get();set(false);CHECK(!actual);set(before);CHECK(actual); // double sided
    set(false);before=get();set(true);mode(1);CHECK(actual&&face==1);mode(2);set(before);CHECK(!actual); // expanded overlay
    actual=true;face=1;state.invalidate();CHECK(get());mode(2);CHECK(face==2); // external helper
    if(cache)CHECK(counts.queries==1);else CHECK(counts.queries==24);
    CHECK(actual&&face==2);
  }
}
