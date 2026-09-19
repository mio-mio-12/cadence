#include "gameplay/KnifeHit.h"
#include "weapon/WeaponProfile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::exit(1);}}while(false)
int main(){
    using scene::Vec3;const Vec3 origin{0,0,160},forward{1,0,0};
    CHECK(gameplay::inKnifeCone(origin,forward,{100,0,160},150,40));
    CHECK(gameplay::inKnifeCone(origin,forward,{150,0,160},150,40));
    CHECK(!gameplay::inKnifeCone(origin,forward,{151,0,160},150,40));
    CHECK(!gameplay::inKnifeCone(origin,forward,{100,60,160},150,40));
    CHECK(!gameplay::inKnifeCone(origin,forward,{-100,0,160},150,40));
    CHECK(!gameplay::inKnifeCone(origin,{},origin,150,40));
    CHECK(!gameplay::inKnifeCone(origin,forward,{100,0,160},std::numeric_limits<float>::quiet_NaN(),40));
    CHECK(gameplay::knifeTarget(origin,forward,{100,0,0},180,150,40,[](Vec3){return true;}));
    CHECK(!gameplay::knifeTarget(origin,forward,{100,0,0},180,150,40,[](Vec3){return false;}));
    CHECK(!gameplay::knifeTarget(origin,forward,{100,0,0},65,150,40,[](Vec3){return true;}));
    CHECK(gameplay::knifeTarget(origin,scene::normalize(Vec3{100,0,-110}),{100,0,0},65,200,40,[](Vec3){return true;}));
    weapon::Profile p;p.internalName="knife_test";p.meleeWeapon=true;p.knifeRadiusCm=212;p.knifeConeDegrees=32;
    const auto path=std::filesystem::temp_directory_path()/"cadence_knife_v133_test.weap";std::string error;
    CHECK(weapon::save(p,path,error));weapon::Profile loaded;CHECK(weapon::load(path,loaded,error));
    CHECK(loaded.meleeWeapon&&loaded.knifeRadiusCm==212&&loaded.knifeConeDegrees==32);
    std::ifstream in(path);std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
    auto start=text.find("knife_settings ");CHECK(start!=std::string::npos);auto end=text.find('\n',start);text.erase(start,end-start+1);
    {std::ofstream legacy(path);legacy<<text;}CHECK(weapon::load(path,loaded,error));CHECK(loaded.meleeWeapon&&loaded.knifeRadiusCm==150&&loaded.knifeConeDegrees==40);
    {std::ofstream bad(path);bad<<text<<"knife_settings invalid 40\n";}CHECK(!weapon::load(path,loaded,error));CHECK(loaded.knifeRadiusCm==150);
    std::filesystem::remove(path);
    std::cout<<"PASS cone/range/height/occlusion policy, profile roundtrip and legacy defaults\n";
}
