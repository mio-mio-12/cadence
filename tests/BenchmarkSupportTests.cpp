#include "app/BenchmarkSupport.h"
#include <iostream>

namespace {bool expect(bool condition,const char* message){if(!condition)std::cerr<<"FAILED: "<<message<<'\n';return condition;}}
int main(){
    int failures{};
    failures+=!expect(benchmark::isPlaceholder("MW3","viewmodel_cod4_ak47"),"MW3 legacy AK47 rejected");
    failures+=!expect(benchmark::isPlaceholder("iw5","AK-47"),"IW5 AK47 spelling rejected");
    failures+=!expect(benchmark::isPlaceholder("bo2","rifle_PLACEHOLDER"),"explicit placeholder rejected");
    failures+=!expect(!benchmark::isPlaceholder("cs2","ak47"),"native CS2 AK47 allowed");
    failures+=!expect(!benchmark::hasWeaponFamily("viewmodel_m40a3","m4"),"rifle family cannot match sniper name prefix");
    assets::Catalog catalog;
    catalog.entries={
        {{},"viewmodel_ak47","mw3",assets::Role::ViewWeapon,"rifles"},
        {{},"viewmodel_awp","mw3",assets::Role::ViewWeapon,"snipers"},
        {{},"viewmodel_m4a1","mw3",assets::Role::ViewAttachment,"rifles"},
        {{},"viewmodel_acr","mw3",assets::Role::ViewWeapon,"rifles"},
        {{},"viewmodel_p99","mw3",assets::Role::ViewWeapon,"pistols"},
        {{},"viewmodel_m4a1","cs2",assets::Role::ViewWeapon,"rifles"},
        {{},"unknown","test",assets::Role::ViewWeapon,"unknown"}};
    failures+=!expect(benchmark::selectWeapon(catalog,"MW3",false)==3,"rifle selection rejects placeholder and attachments");
    failures+=!expect(benchmark::selectWeapon(catalog,"mw3",true)==4,"pistol selection stays within game and class");
    failures+=!expect(benchmark::selectWeapon(catalog,"test",false)==benchmark::npos,"unknown class not substituted");
    failures+=!expect(benchmark::selectWeapon(catalog,"absent",false)==benchmark::npos,"absent game has no selection");
    failures+=!expect(benchmark::percentile({},95)==0.0,"empty timings safe");
    failures+=!expect(benchmark::percentile({4,1,3,2},50)==2.5,"median interpolation");
    failures+=!expect(benchmark::percentile({4,1,3,2},100)==4,"maximum percentile");
    failures+=!expect(benchmark::percentile({4,1},-20)==1,"percentile clamped");
    failures+=!expect(benchmark::percentile({std::numeric_limits<double>::quiet_NaN(),2},95)==2,"invalid timings ignored");
    if(!failures)std::cout<<"All benchmark support tests passed.\n";
    return failures?1:0;
}
