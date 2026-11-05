#pragma once
#include <cstdint>

namespace frame_scalar {
// ARM64 signatures matched to this runtime's 005 adapter. These helpers do not
// install a hook or obtain an interface. The caller must supply the original.
using ScalarCreate=int32_t(*)(void*,uint64_t,const char*,uint64_t*,int32_t,int32_t);
using ScalarUpdate=int32_t(*)(void*,uint64_t,float,double);
template<class Observer>
int32_t forwardCreate(ScalarCreate original,void* receiver,uint64_t container,
        const char* path,uint64_t* component,int32_t type,int32_t units,Observer&& observe){
    auto result=original(receiver,container,path,component,type,units);
    if(result==0&&component&&*component&&path){
        try{observe(receiver,container,path,*component,type,units);}catch(...){/* audit cannot change result */}
    }
    return result;
}
template<class Observer>
int32_t forwardUpdate(ScalarUpdate original,void* receiver,uint64_t component,
        float value,double offset,Observer&& observe){
    auto result=original(receiver,component,value,offset);
    try{observe(receiver,component,value,offset,result);}catch(...){/* preserve original result */}
    return result;
}
}
