#include "scalar-shadow-bridge.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
using namespace frame_scalar;
static thread_local unsigned calls;
static thread_local int32_t code;
static thread_local uint32_t expectedBits;
static int32_t original(void* receiver,uint64_t component,float value,double offset){
    assert(receiver&&component&&offset==.125);
    uint32_t bits=0;std::memcpy(&bits,&value,4);assert(bits==expectedBits);
    ++calls;return code;
}
static std::atomic<bool> entered{false},releaseCall{false};
static int32_t delayed(void*,uint64_t,float,double){
    entered.store(true);while(!releaseCall.load())std::this_thread::yield();return 0;
}
int main(){
    ScalarShadowBridge bridge;
    ExistingScalar binding{0x200000023,0x100000002,"/input/capsense/pinky","test",Hand::Left,true,true,0,false};
    DeviceSnapshot device{2,"test","frame_controller",Hand::Left,true,0};
    auto configure=[&]{return bridge.configure(binding,device,4096,1100);};
    auto call=[&](uintptr_t receiver,int32_t result,float value){
        code=result;std::memcpy(&expectedBits,&value,4);unsigned before=calls;
        assert(bridge.forward(original,reinterpret_cast<void*>(receiver),binding.component,value,.125,1200)==result);
        assert(calls==before+1);
    };
    assert(configure());auto generation=bridge.status().generation;
    call(1234,5,.7f);assert(!bridge.status().receiverObserved);
    assert(bridge.status().attempts==1&&bridge.status().failures==1&&bridge.status().lastResult==5);
    call(1234,0,.7f);assert(bridge.status().receiverObserved);
    assert(bridge.status().successes==1&&bridge.status().attempts==2&&bridge.status().lastResult==0);
    assert(configure());call(1234,0,.7f); // Staged publication is still OFF.
    call(5678,0,.7f);assert(bridge.status().ambiguous);assert(!configure());
    bridge.clear();assert(configure());assert(bridge.status().generation>generation);
    std::thread pending([&]{bridge.forward(delayed,reinterpret_cast<void*>(1234),binding.component,.7f,.125,1200);});
    while(!entered.load())std::this_thread::yield();
    bridge.clear();assert(configure());releaseCall.store(true);pending.join();
    assert(!bridge.status().receiverObserved); // Old successful return cannot leak.
    device.connected=false;assert(!configure());assert(!bridge.status().generation);
    device.connected=true;assert(configure());call(1234,0,-0.f);
    uint32_t nanBits=0x7fc01234;float nan;std::memcpy(&nan,&nanBits,4);call(1234,0,nan);
    auto start=std::chrono::steady_clock::now();
    std::thread control([&]{for(int i=0;i<5000;++i){bridge.clear();assert(configure());}});
    for(int i=0;i<100000;++i)call(1234,0,.7f);
    control.join();
    auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::printf("PASS OFF-only forwarding, return/bits/time, receiver ambiguity, disconnect, late return; 100000 calls with concurrent publication: %lld us (synthetic aggregate, not native callback latency)\n",static_cast<long long>(us));
}
