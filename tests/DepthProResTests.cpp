#include "app/DepthProRes.h"
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x))std::abort();}while(false)
int main(){using namespace cadence::depth_export;
for(bool full:{false,true}){int lo=full?0:64,hi=full?1023:940;double error=0;for(unsigned i=0;i<65536;++i){float z=(i+.5f)/65536;auto q=quantize(z,i,true,1,full);CHECK(q>=lo&&q<=hi);CHECK(q==quantize(z,i,true,1,full));CHECK(std::abs(float(q)-(lo+z*(hi-lo)))<=1.001f);error+=q-(lo+z*(hi-lo));}CHECK(std::abs(error/65536)<.01);CHECK(quantize(0,5,true,1,full)==lo);CHECK(quantize(1,5,true,1,full)==hi);CHECK(quantize(std::numeric_limits<float>::quiet_NaN(),5,true,1,full)==hi);}
std::vector<float> z{0,.5f,1};std::vector<std::uint16_t> y;pack(z,y,false,1,false);CHECK(y.size()==9&&y[0]==64&&y[1]==502&&y[2]==940);for(int i=3;i<9;++i)CHECK(y[i]==512);
for(unsigned i=0;i<100;++i)CHECK(quantize(.3f,i,false,1,false)==quantize(.3f,i,true,0,false));
}
