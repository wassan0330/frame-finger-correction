#include "scalar-off-forwarding.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace frame_scalar;
static unsigned calls=0,observations=0;
static uint64_t observedHandle=0;
static uint32_t observedFloat=0;
static uint64_t observedDouble=0;
static int marker;
static int32_t update(void* receiver,uint64_t handle,float value,double offset){
    assert(receiver==&marker);++calls;observedHandle=handle;
    std::memcpy(&observedFloat,&value,4);std::memcpy(&observedDouble,&offset,8);
    return -19;
}
static bool failCreate=false;
static int32_t create(void* receiver,uint64_t container,const char* path,uint64_t* handle,int32_t type,int32_t units){
    assert(receiver==&marker&&container==0x100000002&&std::strcmp(path,"/input/capsense/pinky")==0);
    assert(type==0&&units==0);++calls;
    if(failCreate)return 3;
    *handle=0x200000007;return 0;
}
int main(){
    const uint32_t floatBits[]{0,0x80000000,0x3f400000,0x7fc12345,0x7f800000};
    const uint64_t offsetBits=0xbfbf9add3746f65f;
    for(auto bits:floatBits){
        float value;double offset;
        std::memcpy(&value,&bits,4);std::memcpy(&offset,&offsetBits,8);
        auto old=calls;
        auto result=forwardUpdate(update,&marker,0x200000007,value,offset,
            [&](void* receiver,uint64_t handle,float got,double time,int32_t error){
                assert(calls==old+1&&receiver==&marker&&handle==0x200000007&&error==-19);
                uint32_t b;uint64_t t;std::memcpy(&b,&got,4);std::memcpy(&t,&time,8);
                assert(b==bits&&t==offsetBits);++observations;
                throw std::runtime_error("simulated audit failure");
            });
        assert(result==-19&&calls==old+1&&observedHandle==0x200000007);
        assert(observedFloat==bits&&observedDouble==offsetBits);
    }
    assert(observations==5);
    uint64_t handle=0;
    auto observe=[&](void*,uint64_t,const char*,uint64_t got,int32_t,int32_t){assert(got==handle);++observations;};
    assert(forwardCreate(create,&marker,0x100000002,"/input/capsense/pinky",&handle,0,0,observe)==0);
    assert(handle==0x200000007&&observations==6);
    failCreate=true;
    assert(forwardCreate(create,&marker,0x100000002,"/input/capsense/pinky",&handle,0,0,observe)==3);
    assert(observations==6);
    std::cout<<"PASS ARM64 forwarding ABI, receiver/handle/float/time/return preservation and observer failure\n";
}
