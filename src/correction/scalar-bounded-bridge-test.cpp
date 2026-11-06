// test-flags: -DFRAME_SCALAR_BOUNDED_ON_CANDIDATE
#include "scalar-shadow-bridge.hpp"
#include <cassert>
#include <cstdio>
using namespace frame_scalar;
static float received=0;
static unsigned calls=0;
static int32_t original(void*,uint64_t,float value,double offset){
    assert(offset==.125);received=value;++calls;return 0;
}
int main(){
    ScalarShadowBridge bridge;
    ExistingScalar binding{0x200000023,0x100000002,"/input/capsense/pinky","test",Hand::Left,true,true,0,false};
    DeviceSnapshot device{2,"test","frame_controller",Hand::Left,true,0};
    auto configure=[&](uint64_t now){return bridge.configure(binding,device,4096,now);};
    auto send=[&](uint64_t now,uint64_t handle=0x200000023){
        auto before=calls;
        assert(bridge.forward(original,reinterpret_cast<void*>(1234),handle,.7f,.125,now)==0);
        assert(calls==before+1);return received;
    };
    assert(configure(1000));
    assert(!bridge.armOnce(true,true,true,1000,5,.8f)); // No real successful receiver yet.
    assert(send(1001)==.7f);assert(configure(1010));
    assert(!bridge.armOnce(false,true,true,1010,5,.8f));
    assert(!bridge.armOnce(true,false,true,1010,5,.8f));
    assert(!bridge.armOnce(true,true,false,1010,5,.8f));
    assert(!bridge.armOnce(true,true,true,1400,5,.8f)); // Stale metadata cannot be refreshed by arm.
    assert(!bridge.armOnce(true,true,true,1010,6,.8f));
    assert(bridge.armOnce(true,true,true,1010,5,.8f));
    assert(send(1011)==.7f); // A fresh successful call must precede correction.
    assert(send(1012)==0);
    ScalarTrace sample;uint64_t changed=0,dropped=0;
    assert(bridge.trace(sample,changed,dropped));
    assert(sample.input==.7f&&sample.output==0&&sample.mapped&&sample.deadlineMs==6010&&changed==1);
    assert(send(1012,0x100000023)==.7f); // Right hand unchanged.
    assert(send(1300)==.7f); // Heartbeat stopped, bypass.
    assert(configure(1310));assert(send(1311)==0);
    assert(configure(6000));assert(send(6009)==0);assert(send(6010)==.7f);
    assert(!bridge.armOnce(true,true,true,6011,5,.8f)); // No same-session rearm.
    bridge.clear();assert(send(6012)==.7f);
    assert(configure(7000));assert(send(7001)==.7f);assert(configure(7010));
    assert(bridge.armOnce(true,true,true,7010,5,.8f));
    assert(send(7011)==.7f);assert(send(7012)==0);
    device.connected=false;assert(!configure(7020));assert(send(7021)==.7f);
    device.connected=true;assert(configure(8000));assert(send(8001)==.7f);assert(configure(8010));
    assert(bridge.armOnce(true,true,true,8010,5,.8f));assert(send(8011)==.7f);assert(send(8012)==0);
    bridge.stopCorrection();assert(send(8013)==.7f);assert(!bridge.correctionDeadline());
    std::puts("PASS bounded native bridge candidate: explicit gates, fresh success, left-only, 5s expiry, stale/disconnect bypass, no automatic rearm");
}
