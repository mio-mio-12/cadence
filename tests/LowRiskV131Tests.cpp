#include "app/UiFileLists.h"
#include "app/BoundedPipeWriter.h"
#include <fstream>
#include <iostream>
#include <thread>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<'\n';return 1;}}while(false)
int main(){
 const auto folder=std::filesystem::temp_directory_path()/("cadence_v131_"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(folder);
 cadence::UiFileLists lists;const auto now=cadence::UiFileLists::Clock::now();
 auto empty=lists.get(folder,"|.castvisual|",false,false,now);CHECK(empty->empty());
 {std::ofstream f(folder/"b.castvisual");f<<"test";}{std::ofstream f(folder/"a.castvisual");f<<"test";}
 for(int frame=0;frame<165;++frame)CHECK(lists.get(folder,"|.castvisual|",false,false,now+std::chrono::milliseconds(frame*5))->empty());CHECK(lists.scans()==1);
 const auto filled=lists.get(folder,"|.castvisual|",false,true,now);CHECK(filled->size()==2&&filled->front().filename()=="a.castvisual");CHECK(empty->empty());
 CHECK(lists.get(folder,"|.castvisual|",false,false,now+std::chrono::seconds(2))->size()==2);CHECK(lists.scans()==3);
 CHECK(lists.get(folder/"missing","|.castvisual|",false,false,now)->empty());CHECK(lists.get(folder/"missing","|.castvisual|",false,false,now)->empty());CHECK(lists.scans()==4);
 cadence::capture::BoundedPipeWriter pipe;HANDLE reader{};CHECK(pipe.open(reader));
 std::vector<unsigned char> payload(1024*1024,77);DWORD written{};const auto start=std::chrono::steady_clock::now();
 CHECK(!pipe.write(payload.data(),static_cast<DWORD>(payload.size()),written,100));CHECK(pipe.error()==ERROR_TIMEOUT);CHECK(std::chrono::steady_clock::now()-start<std::chrono::seconds(2));CloseHandle(reader);pipe.close();
 CHECK(pipe.open(reader));std::vector<unsigned char> received(payload.size());bool readOk=true;
 std::thread consumer([&]{std::size_t offset=0;while(offset<received.size()){DWORD n{};if(!ReadFile(reader,received.data()+offset,static_cast<DWORD>(received.size()-offset),&n,nullptr)||!n){readOk=false;break;}offset+=n;}});
 const bool writeOk=pipe.write(payload.data(),static_cast<DWORD>(payload.size()),written);consumer.join();CHECK(writeOk&&readOk&&written==payload.size()&&received==payload);CloseHandle(reader);pipe.close();
 CHECK(pipe.open(reader));CloseHandle(reader);CHECK(!pipe.write(payload.data(),32,written));pipe.close();
 std::filesystem::remove(folder/"a.castvisual");std::filesystem::remove(folder/"b.castvisual");std::filesystem::remove(folder);
 std::cout<<"PASS UI cache reuse/empty/root/refresh/snapshot tests and bounded blocked/broken/normal pipe writes\n";return 0;
}
