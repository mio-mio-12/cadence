#include "assets/AnimationFileIndex.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using Index = assets::AnimationFileIndex;

static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void touch(const fs::path& path) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path); out << "fixture";
    check(static_cast<bool>(out), "fixture write failed");
}
static void advance(const fs::path& path) {
    fs::last_write_time(path, fs::last_write_time(path) + std::chrono::seconds(2));
}
// Original equipViewWeapon scan/matcher, deliberately independent of the index.
static std::vector<fs::path> original(const fs::path& folder, const std::string& prefix, const std::vector<std::string>& keys) {
    auto lower=[](std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;};
    std::vector<fs::path> files; std::error_code ec;
    for(fs::recursive_directory_iterator it(folder,fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
        if(ec){ec.clear();continue;}
        if(it->is_regular_file(ec)&&it->path().extension()==".cast"){
            const auto fname=lower(it->path().filename().string());
            bool match=!prefix.empty()&&fname.starts_with(prefix);
            if(!match)for(const auto& k:keys){
                if(fname.find("_"+k+"_")!=std::string::npos||fname.starts_with("viewmodel_"+k+"_")||fname.starts_with("vm_"+k+"_")||fname.ends_with("_"+k+".cast")||fname.ends_with("_"+k+"_model.cast")){match=true;break;}
            }
            if(!match)for(auto parent=it->path().parent_path();parent!=folder&&!parent.empty();parent=parent.parent_path()){
                const auto pName=lower(parent.filename().string());
                for(const auto& k:keys)if(pName==k||pName==prefix||pName=="knife_"+k||pName=="weapon_"+k||pName=="viewmodel_"+k||pName=="vm_"+k){match=true;break;}
                if(match)break;
            }
            if(match)files.push_back(it->path());
        }
    }
    std::sort(files.begin(),files.end()); return files;
}
static std::vector<fs::path> indexed(Index& index,const fs::path& folder,const std::string& prefix,const std::vector<std::string>& keys){
    std::vector<fs::path> files;
    for(const auto& entry:index.files(folder))if(Index::matches(entry,prefix,keys))files.push_back(entry.path);
    return files;
}
int main(){
    const auto root=fs::temp_directory_path()/("cadence-index-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root);
        // Only the test-created directory is ever removed, including on failure.
        struct Cleanup {fs::path root;~Cleanup(){std::error_code ec;fs::remove_all(root,ec);}} cleanup{root};
        for(const auto* path:{"viewmodel_an94_idle.cast","foo_fn57_reload.cast","VM_EMC_FIRE.cast","foo_emc.cast","foo_emc_model.cast","ignore.CAST","ignore.txt","nested/AN94/arbitrary.cast","nested/knife_karambit/idle.cast","weapon_arx160/draw.cast","viewmodel_fn57/reload.cast","vm_emc/sub/inspect.cast","prefix_/unrelated.cast","unrelated/idle.cast"})touch(root/path);
        fs::create_directories(root/"not_a_file.cast");
        Index index;
        for(const std::string prefix:{"viewmodel_an94_","vm_","prefix_",""})
            for(const std::vector<std::string> keys:std::vector<std::vector<std::string>>{{"an94"},{"emc","arx160"},{"fn57"},{"karambit"},{},{"no_match"}})
                check(indexed(index,root,prefix,keys)==original(root,prefix,keys),"discovery differs from original scan");
        check(index.scans()==1,"unchanged tree should be scanned once");
        touch(root/"vm_emc/sub/new.cast");advance(root/"vm_emc/sub");
        check(indexed(index,root,"",{"emc"})==original(root,"",{"emc"}),"nested addition not detected");
        check(index.scans()==2,"nested addition did not invalidate");
        fs::rename(root/"vm_emc/sub/new.cast",root/"vm_emc/sub/renamed.cast");advance(root/"vm_emc/sub");
        check(indexed(index,root,"",{"emc"})==original(root,"",{"emc"}),"rename not detected");
        fs::remove(root/"vm_emc/sub/renamed.cast");advance(root/"vm_emc/sub");
        check(indexed(index,root,"",{"emc"})==original(root,"",{"emc"}),"deletion not detected");
        touch(root/"new_tree/vm_emc/other.cast");advance(root);
        check(indexed(index,root,"",{"emc"})==original(root,"",{"emc"}),"new directory not detected");
        fs::remove_all(root/"vm_emc");advance(root);
        check(indexed(index,root,"",{"emc"})==original(root,"",{"emc"}),"removed directory not detected");
        check(index.files(root/"missing").empty(),"missing root should be empty");
        touch(root/"missing/first.cast");
        check(index.files(root/"missing").size()==1,"missing folder must not be negatively cached");
        Index nextRequest;
        check(indexed(nextRequest,root,"vm_",{})==original(root,"vm_",{}),"fresh request mismatch");
        std::cout<<"Animation index: matcher parity, stable reuse, nested add/rename/delete, missing root and fresh request PASS\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
