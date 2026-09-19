#pragma once
#include <array>
#include <bit>
#include <cstdint>

namespace render {
// Scoped to one program's mesh pass. Never survives a frame or program change.
// Compare the exact payload, not approximate float equality. Unknown locations
// are uploaded unconditionally, so driver-assigned locations cannot break it.
class MeshUniformCache {
    struct Entry { std::array<std::uint32_t,4> value{}; unsigned kind{}; };
    std::array<Entry,512> entries_{};
public:
    bool update(int location,unsigned kind,std::array<std::uint32_t,4> value) noexcept {
        if(location<0)return false;
        if(static_cast<unsigned>(location)>=entries_.size())return true;
        auto& old=entries_[location];
        if(old.kind==kind&&old.value==value)return false;
        old={value,kind};return true;
    }
    bool integer(int location,int value) noexcept {return update(location,1,{static_cast<std::uint32_t>(value),0,0,0});}
    bool scalar(int location,float value) noexcept {return update(location,2,{std::bit_cast<std::uint32_t>(value),0,0,0});}
    bool vector3(int location,float x,float y,float z) noexcept {return update(location,3,{std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),std::bit_cast<std::uint32_t>(z),0});}
    bool vector4(int location,float x,float y,float z,float w) noexcept {return update(location,4,{std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),std::bit_cast<std::uint32_t>(z),std::bit_cast<std::uint32_t>(w)});}
};
}
