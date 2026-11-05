#pragma once
#include "scalar-component-identity.hpp"
#include "scalar-off-forwarding.hpp"
#include <algorithm>
#include <cmath>

namespace frame_scalar {
struct CorrectionLease {
    bool explicitOn=false,runtimeVerified=false,updateWitnessed=false;
    ExistingScalar binding; // Metadata AND a successful update, never metadata alone.
    uintptr_t receiver=0;
    uint64_t issuedMs=0,deadlineMs=0;
    float deadzone=.8f;
};
struct CorrectionEvidence {
    DeviceSnapshot device;
    bool componentRecordVerified=false;
    uint64_t observedMs=0;
};
enum class CorrectionDecision {Bypass,Deadzone};
struct ScalarResult {float value;CorrectionDecision decision;};
inline ScalarResult boundedScalar(const CorrectionLease& lease,const CorrectionEvidence& evidence,
                                 uintptr_t receiver,uint64_t component,float value,uint64_t nowMs,bool sessionWitness=false){
    ScalarResult bypass{value,CorrectionDecision::Bypass};
    if(!lease.explicitOn||!lease.runtimeVerified||!(lease.updateWitnessed||sessionWitness)||!evidence.componentRecordVerified||
       !lease.receiver||receiver!=lease.receiver||component!=lease.binding.component||
       !lease.issuedMs||lease.deadlineMs<=lease.issuedMs||lease.deadlineMs-lease.issuedMs>5000||
       nowMs<lease.issuedMs||nowMs>=lease.deadlineMs||!evidence.observedMs||
       evidence.observedMs>nowMs||nowMs-evidence.observedMs>250||
       !std::isfinite(value)||value<0||value>1||!std::isfinite(lease.deadzone)||lease.deadzone<0||lease.deadzone>.9f||
       !matchesWithUpdateEvidence(lease.binding,evidence.device,lease.binding.successfulUpdateVerified||sessionWitness)||lease.binding.hand!=Hand::Left||
       lease.binding.path!="/input/capsense/pinky")return bypass;
    // Only the first left-pinky experiment. Unit endpoints are preserved.
    float mapped=std::clamp((value-lease.deadzone)/(1.f-lease.deadzone),0.f,1.f);
    return {mapped,CorrectionDecision::Deadzone};
}
inline int32_t forwardBounded(ScalarUpdate original,void* receiver,uint64_t component,float value,double offset,
        const CorrectionLease& lease,const CorrectionEvidence& evidence,uint64_t nowMs){
    auto result=boundedScalar(lease,evidence,reinterpret_cast<uintptr_t>(receiver),component,value,nowMs);
    return original(receiver,component,result.value,offset);
}
}
