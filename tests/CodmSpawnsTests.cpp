#include "gameplay/CodmSpawns.h"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<#x<<" line "<<__LINE__<<"\n";return 1;}}while(0)
int main(){
 using namespace gameplay::bot;std::string error,out;
 const std::string input=R"({"schema":"codm.spawns/1","coordinateSystem":"RH_Z_UP","units":"inches","unknown":{"keep":17},"sets":[{"mode":"Main_FFA","spawns":[{"id":"ffa","position":[9,8,7]}]},{"mode":"Main_TDM","spawns":[{"id":"original","position":[1,2,3],"customMetadata":"keep"},{"position":[1,2,3]},{"position":[4,5,6],"available":false}]}]})";
 CodmSpawnSet s;CHECK(readCodmSpawns(input,2,s,error));CHECK(s.mode=="Main_TDM"&&s.spawns.size()==1);CHECK(std::abs(s.spawns[0].z-15.24f)<.001f);
 CHECK(mergeCodmSpawns(input,2,{{5.08f,10.16f,15.24f},{50.8f,101.6f,152.4f}},out,error));
 auto j=scene::codm::parseJson(out);CHECK(j["unknown"]["keep"]==17);CHECK(j["sets"][0]["spawns"][0]["id"]=="ffa");CHECK(j["sets"][1]["spawns"][0]["customMetadata"]=="keep");CHECK(j["spawnCount"]==5);
 CHECK(readCodmSpawns(out,2,s,error)&&s.spawns.size()==2);
 std::string again;CHECK(mergeCodmSpawns(out,2,s.spawns,again,error));CHECK(out==again);
 CHECK(mergeCodmSpawns("",1,{{2.54f,5.08f,7.62f}},again,error));CHECK(readCodmSpawns(again,1,s,error)&&s.spawns.size()==1);
 auto bad=input;bad.replace(bad.find("inches"),6,"metres");CHECK(!readCodmSpawns(bad,1,s,error));CHECK(!mergeCodmSpawns(bad,1,{},out,error));
 CHECK(!readCodmSpawns(input,-1,s,error));CHECK(!readCodmSpawns("{",1,s,error));
 std::cout<<"PASS mode selection, scaling, filtering, dedupe, preserving metadata/modes, idempotent merge, new file and malformed rejection\n";
}

