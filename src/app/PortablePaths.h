#pragma once
#include <filesystem>
#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace cadence::portable {
inline std::filesystem::path executableDirectory(){
#ifdef _WIN32
    std::wstring buffer(32768,L'\0');
    const auto length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(length>0&&length<buffer.size()){buffer.resize(length);return std::filesystem::path(buffer).parent_path();}
#endif
    return std::filesystem::current_path();
}
inline bool enabled(const std::filesystem::path& directory=executableDirectory()){
    std::error_code ec;return std::filesystem::is_regular_file(directory/"Cadence Assets"/"settings"/"portable.flag",ec);
}
inline std::filesystem::path t6WeaponArchive(){
    const char* configured=std::getenv("CADENCE_T6_WEAPON_ARCHIVE");
    return configured&&*configured?std::filesystem::u8path(configured):executableDirectory()/"Cadence Assets"/"weapon_data"/"bo2";
}
inline std::filesystem::path ffmpegExecutable(){
    const auto bundled=executableDirectory()/"tools"/"ffmpeg.exe";
    std::error_code ec;if(std::filesystem::is_regular_file(bundled,ec))return bundled;
#ifdef _WIN32
    wchar_t found[32768]{};
    if(SearchPathW(nullptr,L"ffmpeg.exe",nullptr,32768,found,nullptr))return found;
#endif
    return {};
}
}
