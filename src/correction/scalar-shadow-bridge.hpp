#pragma once
#include "correction-publication.hpp"
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
#include "supervised-trial-controller.hpp"
#endif

namespace frame_scalar {
// Native integration rehearsal: there is deliberately no ON entry point.
// configure/clear/status are control-thread-only; forward runs on input threads.
class ScalarShadowBridge {
    struct Observation {
        ExistingScalar binding;
        DeviceSnapshot device;
        uint64_t node=0;
        std::atomic<bool> enabled{true},ambiguous{false};
        std::atomic<uintptr_t> receiver{0};
        std::atomic<uint64_t> attempts{0},successes{0},failures{0};
        std::atomic<int32_t> lastResult{0};
    };
    std::shared_ptr<Observation> observation;
    std::atomic<uint64_t> target{0};
    CorrectionPublication publication;
    uint64_t epoch=0;
    uintptr_t stagedReceiver=0;
    uint64_t lastEvidenceMs=0;
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
    bool onUsed=false;
    uint64_t onDeadline=0;
    std::atomic_flag traceLock=ATOMIC_FLAG_INIT;
    ScalarTrace lastTrace;
    std::atomic<uint64_t> changedCalls{0},traceDropped{0};
#endif
public:
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
    frame_supervision::SupervisedTrialController supervised;
#endif
    struct Status {
        uint64_t generation=0;
        bool receiverObserved=false,ambiguous=false;
        uint64_t attempts=0,successes=0,failures=0;
        int32_t lastResult=0;
        uint64_t component=0;
    };
    struct MonitorFacts {
        std::string serial;
        uint64_t component=0,receiver=0,generation=0,observedMs=0;
        bool verified=false;
    };
    MonitorFacts monitoringFacts()const{
        auto current=std::atomic_load(&observation);
        if(!current)return {};
        auto receiver=current->receiver.load();
        return {current->device.serial,current->binding.component,receiver,current->device.generation,lastEvidenceMs,
            current->enabled.load()&&!current->ambiguous.load()&&receiver!=0};
    }
    void clear(){
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
        supervised.stop();
#endif
        target.store(0);
        auto old=std::atomic_load(&observation);
        if(old)old->enabled.store(false);
        publication.revoke();
        std::atomic_store(&observation,std::shared_ptr<Observation>{});
        stagedReceiver=0;
        lastEvidenceMs=0;
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
        onUsed=false;
        onDeadline=0;
#endif
    }
    bool configure(ExistingScalar binding,DeviceSnapshot device,uint64_t node,uint64_t now){
        auto containerOwner=deviceFromContainer(binding.container);
        auto componentOwner=deviceFromComponent(binding.component);
        if(!node||!now||!binding.metadataVerified||!binding.normalizedOneSided||
           !containerOwner||!componentOwner||*containerOwner!=device.index||*componentOwner!=device.index||
           !device.connected||device.serial.empty()||binding.serial!=device.serial||
           device.type!="frame_controller"||binding.hand!=device.hand||device.hand!=Hand::Left||
           binding.path!="/input/capsense/pinky"){clear();return false;}
        auto old=std::atomic_load(&observation);
        if(!old||old->binding.component!=binding.component||old->node!=node||
           old->binding.container!=binding.container||old->device.index!=device.index||
           old->device.serial!=device.serial||old->device.type!=device.type||old->device.hand!=device.hand){
            clear();
            if(epoch==UINT64_MAX)return false;
            auto next=std::make_shared<Observation>();
            binding.successfulUpdateVerified=false;
            binding.generation=device.generation=++epoch;
            next->binding=std::move(binding);next->device=std::move(device);next->node=node;
            std::atomic_store(&observation,next);target.store(next->binding.component);
            lastEvidenceMs=now;return true;
        }
        if(now<lastEvidenceMs){clear();return false;}
        lastEvidenceMs=now;
        if(old->ambiguous.load()){publication.revoke();return false;}
        auto receiver=old->receiver.load();
        if(!receiver)return true;
        CorrectionEvidence evidence{old->device,true,now};
        if(!stagedReceiver){
            CorrectionLease lease;
            lease.explicitOn=false; // Cannot be changed by any request file.
            lease.runtimeVerified=true;lease.binding=old->binding;lease.receiver=receiver;
            lease.issuedMs=now;lease.deadlineMs=now+5000;
            publication.begin(std::move(lease),std::move(evidence));stagedReceiver=receiver;
        }else publication.refresh(std::move(evidence));
        return true;
    }
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
    // Control-thread only. No file/debug/IPC route is supplied by this class.
    // The native caller must independently verify runtime and explicit consent.
    bool armOnce(bool explicitOn,bool runtimeVerified,bool pcCorrectionOff,
                 uint64_t now,uint64_t seconds,float deadzone){
        auto current=std::atomic_load(&observation);
        if(!explicitOn||!runtimeVerified||!pcCorrectionOff||!current||!stagedReceiver||onUsed||
           !current->enabled.load()||current->ambiguous.load()||!current->receiver.load()||
           !now||!lastEvidenceMs||now<lastEvidenceMs||now-lastEvidenceMs>250||
           seconds<1||seconds>5||now>UINT64_MAX-5000||
           !std::isfinite(deadzone)||deadzone<0||deadzone>.9f)return false;
        CorrectionLease lease;
        lease.explicitOn=lease.runtimeVerified=true;lease.binding=current->binding;
        lease.receiver=stagedReceiver;lease.issuedMs=now;lease.deadlineMs=now+seconds*1000;lease.deadzone=deadzone;
        // begin discards any previous witness. The first call in this ON
        // session is unchanged; only its successful return enables later calls.
        publication.begin(std::move(lease),CorrectionEvidence{current->device,true,lastEvidenceMs});
        onUsed=true;onDeadline=now+seconds*1000;return true;
    }
    void stopCorrection(){publication.revoke();onDeadline=0;}
    uint64_t correctionDeadline()const{return onDeadline;}
    bool trace(ScalarTrace& output,uint64_t& changed,uint64_t& dropped){
        changed=changedCalls.load();dropped=traceDropped.load();
        if(traceLock.test_and_set(std::memory_order_acquire))return false;
        output=lastTrace;traceLock.clear(std::memory_order_release);return output.nowMs!=0;
    }
#endif
    Status status()const{
        auto current=std::atomic_load(&observation);
        if(!current)return {};
        return {current->device.generation,current->receiver.load()!=0,current->ambiguous.load(),
            current->attempts.load(),current->successes.load(),current->failures.load(),current->lastResult.load(),current->binding.component};
    }
    int32_t forward(ScalarUpdate original,void* receiver,uint64_t component,float value,double offset,uint64_t now){
        if(!component||target.load()!=component)return original(receiver,component,value,offset);
        auto current=std::atomic_load(&observation);
        // Retain the precise observation across the original call. A late
        // success from a disconnected generation cannot witness its replacement.
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
        ScalarTrace trace;
        const auto result=publication.forward(original,receiver,component,value,offset,now,&trace);
        if(trace.mapped)changedCalls.fetch_add(1,std::memory_order_relaxed);
        if(!traceLock.test_and_set(std::memory_order_acquire)){
            lastTrace=trace;traceLock.clear(std::memory_order_release);
        }else traceDropped.fetch_add(1,std::memory_order_relaxed);
#elif defined(FRAME_SCALAR_SUPERVISED_ON_CANDIDATE)
        const auto result=supervised.forward(original,receiver,component,value,offset,now);
#else
        const auto result=publication.forward(original,receiver,component,value,offset,now);
#endif
        if(current&&current->binding.component==component&&current->enabled.load()){
            current->attempts.fetch_add(1,std::memory_order_relaxed);
            current->lastResult.store(result,std::memory_order_relaxed);
            (result==0?current->successes:current->failures).fetch_add(1,std::memory_order_relaxed);
        }
        if(current&&current->binding.component==component&&current->enabled.load()&&result==0&&receiver){
            uintptr_t expected=0,actual=reinterpret_cast<uintptr_t>(receiver);
            if(!current->receiver.compare_exchange_strong(expected,actual)&&expected!=actual)
                current->ambiguous.store(true);
        }
        return result;
    }
};
}
