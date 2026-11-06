// test-flags: -DFRAME_COMPAT_OBSERVATION_UI -DFRAME_SCALAR_ALL_FINGERS_BETA -ldl
#include "scalar-off-vtable-probe.hpp"
#include <cassert>
#include <limits>
static unsigned calls;
static float passed;
static int32_t mockOriginal(void*,uint64_t,float value,double){
    ++calls;passed=value;return 0;
}
int main(){
    using namespace frame_off_probe;
    using frame_observation::State;
    const frame_observation::Handles handles{1,2,3,4,5,6,7,8};
    auto* receiver=reinterpret_cast<void*>(uintptr_t{0x1000});
    resident.original.store(mockOriginal);
    experimentalSlot.store(true);
    assert(observation.start(true,true,1,handles,observationMs()));
    // A lease alone cannot authorize a correction during observation.
    experimentalLease.store(UINT64_MAX);
    for(auto handle:handles)for(unsigned n=0;n<3;++n){
        assert(update(receiver,handle,.25f,0)==0);
        assert(passed==.25f);
    }
    assert(observation.state()==State::Verified && calls==24);
    assert(observation.activate(true,1,handles,observationMs()));
    assert(update(receiver,9,.75f,0)==0 && passed==.75f);
    assert(update(receiver,1,std::numeric_limits<float>::quiet_NaN(),0)==0);
    assert(std::isnan(passed) && observation.state()==State::RestorePending);
    assert(experimentalLease.load()==0);
    assert(update(receiver,1,.125f,0)==0 && passed==.125f);
}
