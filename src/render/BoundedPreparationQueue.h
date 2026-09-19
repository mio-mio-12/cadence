#pragma once
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace render::texture {
// Two fixed workers and a two-result sliding window, not one thread per image.
// A slot is released only AFTER upload/consumption, bounding resident results.
template<class Request,class Result>
class BoundedPreparationQueue {
public:
    using Loader=std::function<Result(const Request&)>;
    BoundedPreparationQueue(const std::vector<Request>& requests,Loader loader):requests_(requests),loader_(std::move(loader)){
        try{for(std::size_t i=0;i<std::min<std::size_t>(2,requests.size());++i)workers_.emplace_back([this]{work();});}
        catch(...){stop();throw;}
    }
    ~BoundedPreparationQueue(){stop();}
    BoundedPreparationQueue(const BoundedPreparationQueue&)=delete;
    BoundedPreparationQueue& operator=(const BoundedPreparationQueue&)=delete;
    template<class Consumer,class Wait>
    void consume(Consumer&& consumer,Wait&& wait){
        std::unique_lock lock(mutex_);
        if(consumed_>=requests_.size())throw std::out_of_range("Texture preparation queue exhausted");
        auto& slot=slots_[consumed_%2];
        while(!slot.done){
            if(ready_.wait_for(lock,std::chrono::milliseconds(8))==std::cv_status::timeout){lock.unlock();wait();lock.lock();}
        }
        if(slot.error)std::rethrow_exception(slot.error);
        lock.unlock();consumer(*slot.result);lock.lock();
        slot={};++consumed_;ready_.notify_all();
    }
private:
    struct Slot {std::optional<Result> result;std::exception_ptr error;bool done{};};
    void stop(){
        {std::lock_guard lock(mutex_);stopping_=true;}
        ready_.notify_all();workers_.clear(); // joins before state is destroyed
    }
    void work(){for(;;){
        std::size_t index;
        {std::unique_lock lock(mutex_);ready_.wait(lock,[&]{return stopping_||(next_<requests_.size()&&next_<consumed_+2);});
         if(stopping_)return;index=next_++;}
        std::optional<Result> result;std::exception_ptr error;
        try{result.emplace(loader_(requests_[index]));}catch(...){error=std::current_exception();}
        {std::lock_guard lock(mutex_);auto& slot=slots_[index%2];slot.result=std::move(result);slot.error=error;slot.done=true;}
        ready_.notify_all();
    }}
    const std::vector<Request>& requests_;
    Loader loader_;
    std::mutex mutex_;
    std::condition_variable ready_;
    Slot slots_[2];
    std::size_t next_{},consumed_{};
    bool stopping_{};
    std::vector<std::jthread> workers_;
};
}
