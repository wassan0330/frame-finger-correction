#include "frame-runtime-profile.hpp"
#pragma once
#include <cstdint>

namespace frame_cv_audit {
struct InputSnapshot {
    bool valid=false;
    uint64_t object=0,table=0,update=0;
};
// The cache RVA belongs ONLY to cv SHA256 44e19eeb... . The caller must verify
// that file and locate its actual loaded module; no interface method is called.
template<class Reader>
InputSnapshot snapshot(Reader& reader,uint64_t moduleBase){
    constexpr uint64_t cacheRva=frame_compat::cvInputCacheRva;
    if(!moduleBase||moduleBase>UINT64_MAX-cacheRva)return {};
    InputSnapshot result;
    auto pointer=[](uint64_t value){return value>=4096&&value%8==0&&value<=UINT64_MAX-32;};
    if(!reader.read(moduleBase+cacheRva,&result.object,8)||!pointer(result.object)||
       !reader.read(result.object,&result.table,8)||!pointer(result.table)||
       !reader.read(result.table+24,&result.update,8)||!result.update)return {};
    uint64_t object=0,table=0,update=0;
    if(!reader.read(moduleBase+cacheRva,&object,8)||object!=result.object||
       !reader.read(object,&table,8)||table!=result.table||
       !reader.read(table+24,&update,8)||update!=result.update)return {};
    result.valid=true;return result;
}
}
