#include "render/MeshUniformCache.h"
#include <cstdlib>
#include <limits>
#define CHECK(v) do {if(!(v))std::abort();} while(false)
int main(){
    render::MeshUniformCache cache;
    CHECK(cache.integer(0,0));CHECK(!cache.integer(0,0));
    CHECK(cache.integer(0,1));CHECK(!cache.integer(0,1));
    CHECK(cache.scalar(0,1));CHECK(!cache.scalar(0,1));
    CHECK(cache.scalar(0,0));CHECK(cache.scalar(0,-0.0f));
    CHECK(cache.vector3(1,1,2,3));CHECK(!cache.vector3(1,1,2,3));
    CHECK(cache.vector3(1,1,2,4));CHECK(cache.vector4(1,1,2,4,0));
    CHECK(!cache.vector4(1,1,2,4,0));CHECK(cache.vector4(1,1,2,4,1));
    CHECK(!cache.scalar(-1,1));CHECK(cache.scalar(9000,1));CHECK(cache.scalar(9000,1));
    auto nan=std::numeric_limits<float>::quiet_NaN();
    CHECK(cache.scalar(2,nan));CHECK(!cache.scalar(2,nan));
    render::MeshUniformCache nextFrame;CHECK(nextFrame.integer(0,1));
}
