#include "app/TimedSnapshotCache.h"
#include <iostream>
#include <stdexcept>
#include <vector>

static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    using Cache=cadence::TimedSnapshotCache<std::string>;using namespace std::chrono_literals;
    Cache cache;const auto t=Cache::Clock::time_point{};int scans=0;std::string disk="original";
    auto load=[&]{++scans;return disk;};
    auto first=cache.get("root",t,false,load);
    for(int frame=1;frame<165;++frame)check(cache.get("root",t+std::chrono::milliseconds(frame*6),false,load)==first,"stable panel draws should reuse snapshot");
    check(scans==1,"unchanged frames performed extra scans");disk="externally edited";
    check(cache.get("root",t+999ms,false,load)==first,"unexpected early refresh");
    auto edited=cache.get("root",t+1s,false,load);check(*edited==disk&&scans==2,"external edit not visible at refresh boundary");
    check(*first=="original","old reader snapshot changed");
    disk="saved package";auto saved=cache.get("root",t+1001ms,true,load);check(*saved==disk&&scans==3,"Save/Refresh must bypass timer");
    check(*edited=="externally edited","forced refresh invalidated active row reference");
    disk="other root";check(*cache.get("other",t+1002ms,false,load)==disk&&scans==4,"root change must refresh");
    disk.clear();auto empty=cache.get("other",t+1003ms,true,load);check(empty->empty(),"removed packages not cleared");
    check(cache.get("other",t+1004ms,false,load)==empty&&scans==5,"empty result not cached");
    disk="new package";check(*cache.get("other",t+2100ms,false,load)==disk,"new package after empty scan not found");
    bool threw=false;try{cache.get("other",t+4s,true,[]{throw std::runtime_error("scan error");return std::string{};});}catch(...){threw=true;}
    check(threw,"scan exception swallowed");disk="retry";check(*cache.get("other",t+4s,false,load)==disk,"failed scan postponed retry");
    cadence::TimedSnapshotCache<std::vector<int>> rows;auto oldRows=rows.get("root",t,false,[]{return std::vector<int>{1,2,3};});
    int sum=0;for(int value:*oldRows){sum+=value;rows.get("root",t,true,[]{return std::vector<int>{8,9};});}check(sum==6,"refresh during iteration invalidated rows");
    std::cout<<"Snapshot cache: 165 Hz reuse, timed/forced refresh, root switch, removal/addition, exceptions and nested refresh PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
