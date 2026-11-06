#pragma once
#include "app/UiFileLists.h"
#include "render/EnvironmentSource.h"

namespace cadence {
// Resolve exporter metadata only when the underlying directory snapshot changes.
// Retaining that snapshot also prevents pointer reuse from hiding a refresh.
class SkyLibrary {
public:
    std::shared_ptr<const UiFileLists::Files> get(UiFileLists& lists,const std::filesystem::path& root,bool force=false) {
        auto input=lists.get(root,"|.iwi|.dds|.hdr|.png|.jpg|.jpeg|.webp|",true,force);
        if(input!=input_) {
            auto output=std::make_shared<UiFileLists::Files>();
            for(const auto& path:*input) {
                render::environment::Source source;std::string error;
                if(render::environment::resolve(path,source,error)&&source.layout!=render::environment::Layout::UnsupportedNative)output->push_back(path);
            }
            input_=std::move(input);output_=std::move(output);
        }
        return output_;
    }
private:
    std::shared_ptr<const UiFileLists::Files> input_,output_;
};
}
