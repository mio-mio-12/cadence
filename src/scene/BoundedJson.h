#pragma once
#include "third_party/nlohmann_json.hpp"
#include <unordered_set>
namespace scene::codm {
using Json=nlohmann::json;
inline Json parseJson(std::string_view text){
    if(text.size()>64u*1024u*1024u)throw std::runtime_error("JSON exceeds 64 MiB bound");
    std::size_t nodes=0;std::vector<std::unordered_set<std::string>> keys;
    return Json::parse(text.begin(),text.end(),[&](int depth,Json::parse_event_t event,Json& value){
        if(depth>64||++nodes>8000000)throw std::runtime_error("JSON nesting/node bound exceeded");
        if(event==Json::parse_event_t::object_start)keys.emplace_back();
        if(event==Json::parse_event_t::key&&!keys.back().insert(value.get<std::string>()).second)throw std::runtime_error("Duplicate JSON key");
        if(event==Json::parse_event_t::object_end)keys.pop_back();
        return true;
    });
}
}
