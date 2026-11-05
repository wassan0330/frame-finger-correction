#pragma once
#include "bounded-scalar-correction.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <cstring>

namespace frame_scalar {
struct ScalarTrace {
    uint64_t nowMs=0,deadlineMs=0;
    float input=0,output=0;
    bool mapped=false;
};
// Control-thread operations are serialized. Each callback retains one immutable
// identity/evidence pair; refresh never changes its lease deadline.
class CorrectionPublication {
    struct Session {
        std::atomic<bool> enabled{true}, witnessed{false};
    };
    struct Frame {
        CorrectionLease lease;
        CorrectionEvidence evidence;
        std::shared_ptr<Session> session;
    };
    std::mutex control;
    std::shared_ptr<const Frame> current;
    void revokeLocked(){
        auto old=std::atomic_load(&current);
        if(old)old->session->enabled.store(false);
        std::atomic_store(&current,std::shared_ptr<const Frame>{});
    }
public:
    void revoke(){std::lock_guard<std::mutex> lock(control);revokeLocked();}
    void begin(CorrectionLease lease,CorrectionEvidence evidence){
        std::lock_guard<std::mutex> lock(control);
        revokeLocked();
        // Successful updates must be observed in this session, never supplied
        // by a stale saved configuration or aggregate OFF-probe counters.
        lease.updateWitnessed=false;
        lease.binding.successfulUpdateVerified=false;
        std::shared_ptr<const Frame> next=std::make_shared<Frame>(Frame{
            std::move(lease),std::move(evidence),std::make_shared<Session>()});
        std::atomic_store(&current,std::move(next));
    }
    bool refresh(CorrectionEvidence evidence){
        std::lock_guard<std::mutex> lock(control);
        auto old=std::atomic_load(&current);
        if(!old)return false;
        const auto& a=old->evidence.device;
        const auto& b=evidence.device;
        if(!b.connected||a.index!=b.index||a.serial!=b.serial||a.type!=b.type||
           a.hand!=b.hand||!b.generation||a.generation!=b.generation||
           !evidence.componentRecordVerified||evidence.observedMs<old->evidence.observedMs){
            revokeLocked();return false;
        }
        std::shared_ptr<const Frame> next=std::make_shared<Frame>(Frame{
            old->lease,std::move(evidence),old->session});
        std::atomic_store(&current,std::move(next));return true;
    }
    int32_t forward(ScalarUpdate original,void* receiver,uint64_t component,
                    float value,double offset,uint64_t nowMs,ScalarTrace* trace=nullptr){
        auto frame=std::atomic_load(&current);
        float output=value;
        bool target=frame&&frame->lease.receiver==reinterpret_cast<uintptr_t>(receiver)&&
            frame->lease.binding.component==component;
        if(target&&frame->lease.explicitOn&&frame->session->enabled.load()){
            output=boundedScalar(frame->lease,frame->evidence,reinterpret_cast<uintptr_t>(receiver),
                component,value,nowMs,frame->session->witnessed.load()).value;
            if(!frame->session->enabled.load())output=value;
        }
        if(trace)*trace={nowMs,frame?frame->lease.deadlineMs:0,value,output,std::memcmp(&value,&output,sizeof(value))!=0};
        const auto result=original(receiver,component,output,offset);
        // First successful call always uses the original value. A result from
        // an old session cannot authorize a new receiver or connection epoch.
        if(target&&result==0&&frame->session->enabled.load())
            frame->session->witnessed.store(true);
        return result;
    }
};
}
