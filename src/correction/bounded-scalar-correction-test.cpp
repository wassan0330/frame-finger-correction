#include "bounded-scalar-correction.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace frame_scalar;
static int object;
static unsigned calls=0;
static float received=0;
static uint64_t receivedHandle=0,receivedTimeBits=0;
static int32_t original(void* receiver,uint64_t handle,float value,double offset){
    assert(receiver==&object);++calls;received=value;receivedHandle=handle;
    std::memcpy(&receivedTimeBits,&offset,8);return -17;
}
int main(){
    CorrectionLease lease;
    lease.explicitOn=lease.runtimeVerified=lease.updateWitnessed=true;
    lease.binding={0x200000023,0x100000002,"/input/capsense/pinky","example-serial",Hand::Left,true,true,8,true};
    lease.receiver=reinterpret_cast<uintptr_t>(&object);lease.issuedMs=1000;lease.deadlineMs=6000;
    CorrectionEvidence evidence{{2,"example-serial","frame_controller",Hand::Left,true,8},true,1100};
    auto apply=[&](const CorrectionLease& l,const CorrectionEvidence& e,float input=0.7f,uint64_t now=1200){
        return boundedScalar(l,e,reinterpret_cast<uintptr_t>(&object),0x200000023,input,now);
    };
    assert(apply(lease,evidence).decision==CorrectionDecision::Deadzone&&apply(lease,evidence).value==0);
    assert(apply(lease,evidence,1.f).value==1.f);
    assert(apply(lease,evidence,.8f).value==0);
    assert(std::abs(apply(lease,evidence,.9f).value-.5f)<1e-6);
    auto refused=[&](const CorrectionLease& l,const CorrectionEvidence& e){
        auto result=apply(l,e);assert(result.decision==CorrectionDecision::Bypass&&result.value==.7f);
    };
    auto bad=lease;bad.explicitOn=false;refused(bad,evidence);
    bad=lease;bad.runtimeVerified=false;refused(bad,evidence);
    bad=lease;bad.updateWitnessed=false;refused(bad,evidence);
    bad=lease;bad.binding.metadataVerified=false;refused(bad,evidence);
    bad=lease;bad.binding.successfulUpdateVerified=false;refused(bad,evidence);
    bad=lease;bad.binding.path="/input/capsense/ring";refused(bad,evidence);
    bad=lease;bad.binding.hand=Hand::Right;refused(bad,evidence);
    bad=lease;bad.receiver++;refused(bad,evidence);
    bad=lease;bad.deadlineMs=6001;refused(bad,evidence);
    bad=lease;bad.deadzone=.95f;refused(bad,evidence);
    auto wrong=evidence;wrong.componentRecordVerified=false;refused(lease,wrong);
    wrong=evidence;wrong.device.connected=false;refused(lease,wrong);
    wrong=evidence;wrong.device.serial="other";refused(lease,wrong);
    wrong=evidence;wrong.device.generation++;refused(lease,wrong);
    wrong=evidence;wrong.observedMs=949;refused(lease,wrong);
    wrong=evidence;wrong.observedMs=1201;refused(lease,wrong);
    assert(apply(lease,evidence,.7f,6000).decision==CorrectionDecision::Bypass);
    assert(apply(lease,evidence,.7f,999).decision==CorrectionDecision::Bypass);
    auto other=boundedScalar(lease,evidence,lease.receiver,0x100000023,.7f,1200);
    assert(other.decision==CorrectionDecision::Bypass&&other.value==.7f);
    uint32_t nanBits=0x7fc12345;float nan;
    std::memcpy(&nan,&nanBits,4);auto unchanged=apply(lease,evidence,nan).value;uint32_t got;
    std::memcpy(&got,&unchanged,4);assert(got==nanBits);
    double offset=-.123456789;uint64_t timeBits;std::memcpy(&timeBits,&offset,8);
    assert(forwardBounded(original,&object,0x200000023,.7f,offset,lease,evidence,1200)==-17);
    assert(calls==1&&received==0&&receivedHandle==0x200000023&&receivedTimeBits==timeBits);
    assert(forwardBounded(original,&object,0x20000001c,.7f,offset,lease,evidence,1200)==-17);
    assert(calls==2&&received==.7f&&receivedHandle==0x20000001c&&receivedTimeBits==timeBits);
    // Every other handle bypasses, with no accidental range-wide selection.
    for(uint64_t component=1;component<=100000;++component){
        auto result=boundedScalar(lease,evidence,lease.receiver,component,.7f,1200);
        assert(result.decision==CorrectionDecision::Bypass&&result.value==.7f);
    }
    std::cout<<"PASS left-pinky-only 5s lease, freshness/identity/witness guards, endpoint mapping and exact forwarding\n";
}
