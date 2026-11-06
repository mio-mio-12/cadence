#include <d3dcompiler.h>
#include <fstream>
#include <vector>
#include <iostream>
int main(int argc,char** argv){if(argc!=3)return 2;std::ifstream i(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(i)),{});ID3DBlob* out=nullptr;const auto hr=D3DDisassemble(bytes.data(),bytes.size(),0,nullptr,&out);if(FAILED(hr)){std::cerr<<"D3DDisassemble "<<std::hex<<hr;return 1;}std::ofstream o(argv[2],std::ios::binary);o.write(static_cast<const char*>(out->GetBufferPointer()),out->GetBufferSize());out->Release();return o?0:1;}
