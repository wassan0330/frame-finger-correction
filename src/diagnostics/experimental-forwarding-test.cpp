// test-flags: -DFRAME_COMPAT_OBSERVATION_UI -DFRAME_SCALAR_ALL_FINGERS_BETA -ldl
#include "scalar-off-vtable-probe.hpp"
#include <cassert>
#include <limits>
static unsigned calls;
static float passed;
static double passedOffset;
static uint64_t passedComponent;
static void* passedReceiver;
static int32_t result;
static int32_t mockOriginal(void* receiver,uint64_t component,float value,double offset){
    ++calls;passed=value;passedOffset=offset;passedComponent=component;passedReceiver=receiver;return result;
}
int main(){
    using namespace frame_off_probe;
    using frame_observation::State;
    std::array<std::optional<frame_beta::Binding>,8> bindings{};
    frame_observation::Handles handles{};
    for(unsigned row=0;row<8;++row){
        unsigned device=row<4?1:2;
        auto hand=row<4?frame_scalar::Hand::Left:frame_scalar::Hand::Right;
        std::string serial=row<4?"left":"right";
        handles[row]=(uint64_t(device)<<32)|(row%4+10);
        bindings[row]=frame_beta::Binding{
            {handles[row],0x100000000ull|device,frame_beta::fingerPaths[row%4],serial,hand,true,true,0,false},
            {device,serial,"frame_controller",hand,true,0},0x4000+row*0x100};
    }
    auto* receiver=reinterpret_cast<void*>(uintptr_t{0x1000});
    auto now=observationMs();
    allFingers.configure(bindings,now);
    frame_settings::Profile profile;profile.requested=true;
    for(auto& f:profile.fingers)f={true,false,.2};
    assert(allFingers.renewStandalone(profile,1,true,now,now));
    resident.original.store(mockOriginal);
    experimentalSlot.store(true);
    assert(observation.start(true,true,1,handles,now));
    // Even a lease and enabled bank cannot alter the first real call.
    experimentalLease.store(UINT64_MAX);
    assert(update(receiver,handles[0],.1f,.125)==0);
    assert(calls==1 && passed==.1f && passedOffset==.125);
    assert(passedReceiver==receiver && passedComponent==handles[0]);
    assert(observation.state()==State::Verified && observation.witnessed()==1);
    assert(observation.activate(true,1,handles,observationMs()));
    // Constant input, one component only: all other fingers can remain silent.
    assert(update(receiver,handles[0],.1f,.125)==0 && passed==0.f && calls==2);
    assert(observation.witnessed()==1);
    // Each newly seen finger's first call remains raw; the next can be mapped.
    assert(update(receiver,handles[1],.1f,.25)==0 && passed==.1f && calls==3);
    assert(observation.witnessed()==3);
    assert(update(receiver,handles[1],.1f,.25)==0 && passed==0.f && calls==4);
    assert(update(receiver,9,.75f,.5)==0 && passed==.75f && calls==5);
    // Expired lease cannot map, even when the route and receiver are known.
    experimentalLease.store(0);
    assert(update(receiver,handles[0],.1f,.5)==0 && passed==.1f && calls==6);
    experimentalLease.store(UINT64_MAX);
    // An original-function error is preserved and revokes all authorization.
    result=7;
    assert(update(receiver,handles[1],.1f,.5)==7 && calls==7);
    assert(observation.state()==State::RestorePending && experimentalLease.load()==0);
    result=0;
    assert(update(receiver,handles[0],.125f,0)==0 && passed==.125f && calls==8);
    observation=frame_observation::Trial{};
    assert(observation.start(true,true,1,handles,observationMs()));
    assert(update(receiver,handles[0],std::numeric_limits<float>::quiet_NaN(),0)==0);
    assert(std::isnan(passed) && observation.state()==State::RestorePending);
}
