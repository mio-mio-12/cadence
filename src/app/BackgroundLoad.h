#pragma once
#include <future>
#include <optional>
#include <string>
namespace cadence {
template<class Result> Result consumeBackgroundLoad(std::optional<std::future<Result>>& pending){
    Result result;
    try{result=pending->get();}
    catch(const std::exception& e){result.error=std::string("Background load failed: ")+e.what();}
    catch(...){result.error="Unknown background load failure";}
    pending.reset();return result;
}
template<class Result> void joinBackgroundLoad(std::optional<std::future<Result>>& pending) noexcept {
    if(!pending)return;
    try{if(pending->valid())pending->wait();}catch(...){}
    pending.reset();
}
}
