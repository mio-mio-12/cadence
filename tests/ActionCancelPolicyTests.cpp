#include "gameplay/ActionCancelPolicy.h"
#include <iostream>
#include <stdexcept>

using gameplay::action_cancel::Kind;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

int main() try {
    using namespace gameplay::action_cancel;
    require(!mayMint(1.0,0.9,4,0,Kind::Inspect),"expired cooldown cannot mint");
    require(!mayMint(0.5,1.0,0,0,Kind::Reload),"no accepted shot cannot mint");
    require(mayMint(0.5,1.0,4,0,Kind::Inspect),"successful inspect may mint");
    require(mayMint(0.5,1.0,4,0,Kind::Reload),"successful reload may mint");
    require(!mayFire(false,4,4,0,Kind::Inspect),"held fire is not a fresh cancel");
    require(mayFire(true,4,4,0,Kind::Reload),"matching one-shot token fires");
    require(!mayFire(true,4,4,4,Kind::Reload),"consumed token cannot repeat");
    require(!mayFire(true,5,4,0,Kind::Inspect),"stale token cannot affect next shot");
    require(!mayFire(true,4,4,0,Kind::Other),"unrelated action cannot bypass cooldown");
    std::cout<<"Action-cancel firing policy checks passed.\n";return 0;
} catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
