#pragma once
#include "frame-settings-profile.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <unistd.h>
namespace frame_settings {
inline bool loadFile(const std::string& path,Profile& profile){
    std::error_code error;
    auto size=std::filesystem::file_size(path,error);
    if(error||size>4096)return false;
    std::ifstream input(path);
    std::string content((std::istreambuf_iterator<char>(input)),{});
    return input&&!input.bad()&&parse(content,profile);
}
inline bool saveFile(const std::string& path,const Profile& profile){
    auto content=serialize(profile);if(content.empty())return false;
    std::string pattern=path+".tmp-XXXXXX";std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    int fd=mkstemp(name.data());if(fd<0)return false;
    size_t written=0;bool ok=true;
    while(written<content.size()){
        auto count=write(fd,content.data()+written,content.size()-written);
        if(count<=0){ok=false;break;}written+=static_cast<size_t>(count);
    }
    if(ok&&fsync(fd)!=0)ok=false;
    if(close(fd)!=0)ok=false;
    if(ok&&rename(name.data(),path.c_str())!=0)ok=false;
    if(!ok)unlink(name.data());
    return ok;
}
}
