#include "vr-trial-status.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace frame_settings;
    assert(trialStatus("FRAME_VR_STATUS_1 1000 abc STOPPED","abc",2000)=="STOPPED");
    assert(trialStatus("FRAME_VR_STATUS_1 1000 abc ERROR","abc",2000)=="ERROR");
    assert(trialStatus("FRAME_VR_STATUS_1 1000 abc RUNNING","abc",1001)=="RUNNING");
    assert(trialStatus("FRAME_VR_STATUS_1 1000 abc RUNNING","abc",1003)=="Status unconfirmed");
    assert(trialStatus("FRAME_VR_STATUS_1 1000 old STOPPED","new",1001)=="Status unconfirmed");
    assert(trialStatus("FRAME_VR_STATUS_1 1002 abc STOPPED","abc",1001)=="Status unconfirmed");
    assert(brokerReady("FRAME_VR_BROKER_1 1000 READY",1001));
    assert(!brokerReady("FRAME_VR_BROKER_1 1000 READY",1003));
    assert(!brokerReady("FRAME_VR_BROKER_1 1000 OFFLINE",1001));
    std::puts("PASS terminal result retention, live-status expiry, nonce binding and broker readiness expiry");
}
