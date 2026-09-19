#include "gameplay/CastNavigationSidecar.h"
#include <iostream>
int main(){
    using namespace gameplay::bot;int failures=0;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    CastNavigationSidecar result;std::string error;
    const auto parse=[&](const char* text){std::istringstream input(text);return readCastNavigation(input,result,error);};
    check(parse("CASTNAV 5\nboundary_poly -2000 6000 0 0\nspawn 873.112 5037.93 58.3246\n"),"actual Shipment record format rejected");
    check(result.spawns.size()==1&&std::abs(result.spawns[0].x-873.112f)<.001f,"saved scene coordinates were rescaled");
    const char* malformed[]={
        "CASTNAV 6\n", "CASTNAV 5 extra\n", "CASTNAV 5\nspawn 1 2\n",
        "CASTNAV 5\nspawn 1 2 3 extra\n", "CASTNAV 5\nspawn nan 2 3\n",
        "CASTNAV 5\nspawn 1e20 2 3\n", "CASTNAV 5\nlink 0 9 1 1\n",
        "CASTNAV 5\nboundary_poly -1 1 0 2\nbpoint 0 0\n",
        "CASTNAV 5\nboundary_poly -1 1 0 9999999999\n",
        "CASTNAV 5\nboundary_poly 1 -1 0 0\n",
        "CASTNAV 5\nblock 10 0 0 1 1 1\n",
        "CASTNAV 5\ngoal_area 0 0 0 -1 1 1\n",
        "CASTNAV 5\nbpoint 1 1\n", "CASTNAV 5\nunknown 1 2\n",
        "CASTNAV 5\nboundary_poly -1 1 1 0\n"
    };
    for(const auto* text:malformed){check(!parse(text),"malformed input accepted");check(result.spawns.size()==1&&std::abs(result.spawns[0].x-873.112f)<.001f,"failed parse modified live metadata");}
    check(parse("CASTNAV 1\nnode 1 2 3 1 1 16 0 0\nspawn 4 5 6\n"),"legacy v1 node rejected");
    check(parse("CASTNAV 5\nnode 1 2 3 1 1 16 0 1 0 2 192 1 0\nnode 2 3 4 1 1 16 0 0 0 0 192 0 0\nlink 0 1 1 1\ngoal_area 0 0 0 20 1 1\nblock -1 -1 -1 1 1 1\n"),"valid behavior records rejected");
    check(result.graph.nodes.size()==2&&result.graph.nodes[0].mantlePriority&&result.graph.links.size()==1&&result.graph.goalAreas.size()==1&&result.graph.blocks.size()==1,"behavior metadata lost");
    if(!failures)std::cout<<"CASTNAV bounds, transactional malformed handling, saved-centimetre units and legacy records PASS\n";
    return failures?1:0;
}
