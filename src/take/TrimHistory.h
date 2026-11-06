#pragma once
#include "take/Take.h"
#include <iterator>

namespace take {
// Each cut owns only the samples it removed; pose payloads are moved, never
// copied. Exact timestamps are retained to avoid float drift across undo.
class TrimHistory {
    struct Cut {
        std::vector<Sample> removed;
        std::size_t prefix{};
        std::vector<float> times;
        std::vector<ShotEvent> shots;
        std::vector<DollyCameraKeyframe> dolly;
        std::vector<DollyCameraPath> paths;
    };
    std::vector<Cut> cuts_;
public:
    void clear(){cuts_.clear();}
    bool empty() const{return cuts_.empty();}
    template<class Visitor> void visitRemoved(Visitor&& visit){
        for(auto& cut:cuts_)for(auto& sample:cut.removed)visit(sample);
    }
    std::size_t removedTicks() const{
        std::size_t count=0;for(const auto& cut:cuts_)count+=cut.removed.size();return count;
    }
    bool apply(Take& take,std::size_t first,std::size_t last){
        auto& samples=take.samples;
        if(first>last||last>=samples.size()||(!first&&last+1==samples.size()))return false;
        Cut cut;
        cut.prefix=first;
        cut.times.reserve(last-first+1);
        for(auto i=first;i<=last;++i)cut.times.push_back(samples[i].time);
        cut.shots=take.shots;cut.dolly=take.dollyCamera;cut.paths=take.cameraPaths;
        cut.removed.reserve(samples.size()-cut.times.size());
        cuts_.reserve(cuts_.size()+1);
        const auto moveInto=[&](auto begin,auto end){
            cut.removed.insert(cut.removed.end(),std::make_move_iterator(begin),std::make_move_iterator(end));
        };
        moveInto(samples.begin(),samples.begin()+first);
        moveInto(samples.begin()+last+1,samples.end());
        // Recording timestamps normally correspond to tick indices, but the
        // actual endpoints also work for older, irregularly spaced recordings.
        take.trimTicks(first,last);
        cuts_.push_back(std::move(cut));
        return true;
    }
    bool undo(Take& take){
        if(cuts_.empty()||take.samples.size()!=cuts_.back().times.size())return false;
        auto& cut=cuts_.back();
        auto& samples=take.samples;
        samples.reserve(samples.size()+cut.removed.size());
        for(std::size_t i=0;i<samples.size();++i)samples[i].time=cut.times[i];
        const auto middle=cut.removed.begin()+cut.prefix;
        samples.insert(samples.end(),std::make_move_iterator(middle),std::make_move_iterator(cut.removed.end()));
        samples.insert(samples.begin(),std::make_move_iterator(cut.removed.begin()),std::make_move_iterator(middle));
        take.shots=std::move(cut.shots);take.dollyCamera=std::move(cut.dolly);take.cameraPaths=std::move(cut.paths);
        cuts_.pop_back();
        return true;
    }
};
}
