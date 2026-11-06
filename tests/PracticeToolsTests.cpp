#include "gameplay/PracticeTools.h"
#include <iostream>
int main(){
 int failures=0;auto check=[&](bool ok){failures+=!ok;};
 for(bool third:{false,true})for(int mode:{0,1,2,3,4,6}){
  check(!gameplay::practice::directSwapKey(third,mode));
  check(gameplay::practice::swapPressed(true,false,false,third));
  check(!gameplay::practice::swapPressed(true,true,false,third));
 }
 check(gameplay::practice::directSwapKey(true,5));
 for(bool third:{false,true}){
  check(!gameplay::practice::swapPressed(true,false,false,third,true));
  check(!gameplay::practice::swapPressed(false,false,true,third,true));
  check(gameplay::practice::swapPressed(true,false,false,third,false));
 }
 check(gameplay::practice::ebHit(true,false,100,253));
 check(!gameplay::practice::ebHit(true,false,100,254));
 check(!gameplay::practice::ebHit(false,false,500,1));
 check(!gameplay::practice::ebHit(true,true,500,1));
 check(!gameplay::practice::ebHit(true,false,0,0));
 for(unsigned i=1;i<1000;++i){auto p=gameplay::practice::roamOffset(12,i,500);check(scene::length(p)<=425.001f&&scene::length(p)>=124.99f);}
 check(scene::length(gameplay::practice::roamOffset(1,1,0))==0);
 std::cout<<"Practice tool failures: "<<failures<<'\n';return failures?1:0;
}
