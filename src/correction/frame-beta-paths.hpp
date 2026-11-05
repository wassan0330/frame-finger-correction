#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace frame_beta {
inline constexpr std::array<const char*,4> fingerPaths{
 "/input/capsense/trigger","/input/capsense/grip","/input/capsense/ring","/input/capsense/pinky"};
inline int fingerForPath(const std::string& path){
 for(unsigned i=0;i<fingerPaths.size();++i)if(path==fingerPaths[i])return static_cast<int>(i);
 return -1;
}
inline constexpr uint64_t metadataLifetimeMs=1000,uiLifetimeMs=1500,pcLifetimeMs=1500;
}
