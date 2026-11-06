#pragma once
#include <optional>
#include <vector>
#include <algorithm>
namespace cadence::ui {
template<class T> struct History {
    std::optional<T> baseline;
    std::vector<T> previous,next;
    void reset(const T& value){baseline=value;previous.clear();next.clear();}
    bool observe(const T& value,bool editing=false){
        if(!baseline){reset(value);return false;}
        if(editing||value==*baseline)return false;
        if(previous.size()>=32)previous.erase(previous.begin());
        previous.push_back(std::move(*baseline));baseline=value;next.clear();return true;
    }
    bool undo(T& value){observe(value);if(previous.empty())return false;next.push_back(value);value=std::move(previous.back());previous.pop_back();baseline=value;return true;}
    bool redo(T& value){if(next.empty())return false;previous.push_back(value);value=std::move(next.back());next.pop_back();baseline=value;return true;}
};
inline bool commandsAllowed(bool text,bool active,bool popup){return !text&&!active&&!popup;}
}
