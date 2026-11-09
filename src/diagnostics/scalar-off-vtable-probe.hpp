#ifdef FRAME_COMPAT_OBSERVATION_UI
#include "../correction/frame-observation.hpp"
#include <mutex>
#endif
#include "frame-runtime-profile.hpp"
#include "../correction/scalar-activity-label.hpp"
#pragma once
// Replaces one shared scalar vtable slot after authorization. The experimental
// path initially forwards unchanged; only a separate verified activation permits correction.
#include "self-memory-reader.hpp"
#include "../correction/readonly-slot-transaction.hpp"
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
#include "../correction/frame-finger-bank.hpp"
#endif
#include "../correction/scalar-off-forwarding.hpp"
#ifdef FRAME_COMPAT_STRUCTURAL_AUDIT
#include "runtime-facts.hpp"
#endif
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
#include "../correction/scalar-shadow-bridge.hpp"
#endif
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <functional>
#include <memory>
#include <string>
#include <sys/mman.h>
#include <vector>

namespace frame_off_probe {
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
inline frame_beta::FingerBank allFingers;
inline constexpr unsigned targetCount=8,maximumObserverSeconds=60;
#else
inline constexpr unsigned targetCount=4,maximumObserverSeconds=30;
#endif
struct ResidentState {
    struct TargetSet {
        std::array<uint64_t,targetCount> handles{};
        struct Count {
            std::atomic<uint64_t> calls{0},successes{0},failures{0};
            std::atomic<int32_t> lastResult{0};
        };
        std::array<Count,targetCount> counts;
    };
    std::atomic<frame_scalar::ScalarUpdate> original{nullptr};
    std::atomic<uint64_t> total{0},selected{0};
    std::shared_ptr<TargetSet> targets;
};
inline ResidentState resident;
inline void countUpdate(uint64_t handle,int32_t result){
    resident.total.fetch_add(1,std::memory_order_relaxed);
    auto targets=std::atomic_load(&resident.targets);
    if(!targets)return;
    for(size_t i=0;i<targets->handles.size();++i)if(handle&&targets->handles[i]==handle){
        resident.selected.fetch_add(1,std::memory_order_relaxed);
        auto& count=targets->counts[i];
        count.calls.fetch_add(1,std::memory_order_relaxed);
        (result==0?count.successes:count.failures).fetch_add(1,std::memory_order_relaxed);
        count.lastResult.store(result,std::memory_order_relaxed);break;
    }
}
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
inline frame_scalar::ScalarShadowBridge shadow;
#endif
#ifdef FRAME_COMPAT_OBSERVATION_UI
inline std::mutex observationMutex;
inline frame_observation::Trial observation;
inline std::atomic<bool> experimentalSlot{false};
inline std::atomic<uint64_t> experimentalLease{0};
inline uint64_t observationMs(){
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
#endif
inline int32_t update(void* receiver,uint64_t component,float value,double offset){
    // Original is published before the table entry, and is never cleared.
    auto original=resident.original.load(std::memory_order_acquire);
#ifdef FRAME_COMPAT_OBSERVATION_UI
    if(experimentalSlot.load(std::memory_order_acquire)){
        // Do not hold the observation lock while calling SteamVR.
        const auto now=observationMs();
        const auto address=reinterpret_cast<uintptr_t>(receiver);
        bool authorized=false;
        {
            std::lock_guard<std::mutex> guard(observationMutex);
            authorized=now<experimentalLease.load(std::memory_order_acquire)&&observation.allows(component,address,value);
        }
        // False records this real call's receiver/result but never changes its
        // value or learns a baseline. No synthetic input and no replay.
        auto result=allFingers.forward(original,receiver,component,value,offset,now,authorized);
        std::lock_guard<std::mutex> guard(observationMutex);
        observation.sample(component,address,value,result,observationMs());
        if(observation.state()==frame_observation::State::RestorePending)
            experimentalLease.store(0,std::memory_order_release);
        return result;
    }
#endif
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
    auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
    auto result=allFingers.forward(original,receiver,component,value,offset,static_cast<uint64_t>(now));
#else
    auto result=shadow.forward(original,receiver,component,value,offset,static_cast<uint64_t>(now));
#endif
    countUpdate(component,result);
    return result;
#else
    return frame_scalar::forwardUpdate(original,receiver,component,value,offset,
        [](void*,uint64_t handle,float,double,int32_t result){countUpdate(handle,result);});
#endif
}
inline bool knownExecutableFiles(){
    // Fixed executable and numeric self pid only. No runtime launch arguments.
    char command[384]{};
    std::snprintf(command,sizeof(command),"/usr/bin/sha256sum /proc/%ld/exe /opt/steamvr/bin/linuxarm64/vrserver /opt/steamvr/drivers/cv/bin/linuxarm64/driver_cv.so",static_cast<long>(getpid()));
    FILE* pipe=popen(command,"r");if(!pipe)return false;
    char line[512]{};unsigned matches=0;
    while(fgets(line,sizeof(line),pipe)){
        const char* digest=matches<2?frame_compat::serverSha256:frame_compat::cvSha256;
        if(std::strncmp(line,digest,64)||line[64]!=' '){matches=99;break;}
        ++matches;
    }
    return pclose(pipe)==0&&matches==3;
}
class Probe {
    std::unique_ptr<frame_scalar::ReadonlySlotTransaction> transaction;
    std::function<void(const char*)> log;
    void* page=nullptr;size_t pageSize=0;
    bool attached=false;
#ifdef FRAME_COMPAT_OBSERVATION_UI
    bool restartRequired_=false;
#endif
    std::chrono::steady_clock::time_point deadline{};
    bool protect(bool writable){return mprotect(page,pageSize,PROT_READ|(writable?PROT_WRITE:0))==0;}
    bool readonlyPage(uintptr_t address){
        FILE* maps=fopen("/proc/self/maps","r");if(!maps)return false;
        char line[1024]{};bool found=false;
        pageSize=static_cast<size_t>(sysconf(_SC_PAGESIZE));
        if(!pageSize||(pageSize&(pageSize-1))){fclose(maps);return false;}
        auto base=address&~(static_cast<uintptr_t>(pageSize)-1);
        while(fgets(line,sizeof(line),maps)){
            unsigned long low=0,high=0;char permissions[5]{};
            if(sscanf(line,"%lx-%lx %4s",&low,&high,permissions)==3&&base>=low&&base+pageSize<=high){
                found=!std::strcmp(permissions,"r--p");break;
            }
        }
        fclose(maps);page=reinterpret_cast<void*>(base);return found;
    }
public:
    bool active()const{return attached;}
#ifdef FRAME_COMPAT_OBSERVATION_UI
    bool restartRequired()const{return restartRequired_;}
#endif
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
    void renewVerifiedObserver(){if(attached)deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);}
#endif
    void targets(const std::vector<uint64_t>& handles){
        std::array<uint64_t,targetCount> next{};
        if(handles.size()<=next.size())std::copy(handles.begin(),handles.end(),next.begin());
        auto old=std::atomic_load(&resident.targets);
        if(old&&old->handles==next)return;
        auto set=std::make_shared<ResidentState::TargetSet>();set->handles=next;
        std::atomic_store(&resident.targets,std::move(set));
    }
#ifdef FRAME_COMPAT_OBSERVATION_UI
    // Caller must re-run the complete structural classifier and finger census
    // immediately before calling. Attaching does not permit correction.
    bool beginObservation(uintptr_t table,uintptr_t entry,uint64_t session,
                          const frame_observation::Handles& handles,std::function<void(const char*)> logger){
        if(attached||restartRequired_||!table||!entry||!readonlyPage(table+24))return false;
        log=std::move(logger);
        Dl_info self{};
        if(!dladdr(reinterpret_cast<void*>(&update),&self)||!self.dli_fname||
           !dlopen(self.dli_fname,RTLD_NOW|RTLD_NOLOAD|RTLD_NODELETE))return false;
        auto candidate=std::make_unique<frame_scalar::ReadonlySlotTransaction>(
            reinterpret_cast<uintptr_t*>(table+24),entry,reinterpret_cast<uintptr_t>(&update));
        {
            std::lock_guard<std::mutex> guard(observationMutex);
            if(!observation.start(true,true,session,handles,observationMs()))return false;
        }
        resident.original.store(reinterpret_cast<frame_scalar::ScalarUpdate>(entry),std::memory_order_release);
        experimentalSlot.store(true,std::memory_order_release);
        transaction=std::move(candidate);
        const auto result=transaction->install([&](bool writable){return protect(writable);});
        attached=result==frame_scalar::SlotResult::Changed;
        if(!attached){stop(result==frame_scalar::SlotResult::ProtectionFailure);return false;}
        log("FrameCompat: observation attached; all scalar calls pass through");
        return true;
    }
#endif
    bool begin(void* input,unsigned seconds,std::function<void(const char*)> logger){
        log=std::move(logger);
#ifdef FRAME_COMPAT_OBSERVATION_UI
        if(restartRequired_||experimentalSlot.load(std::memory_order_acquire))return false;
#endif
        if(attached||!input||!seconds||seconds>maximumObserverSeconds||!knownExecutableFiles()){
            log("FrameScalarOffProbe: refused duplicate/invalid request or unknown runtime");return false;
        }
        SelfMemoryReader reader;uintptr_t table=0,entry=0;
        if(!reader.read(reinterpret_cast<uintptr_t>(input),&table,8)||!reader.read(table+24,&entry,8))return false;
        Dl_info owner{};
        if(!dladdr(reinterpret_cast<void*>(entry),&owner)||!owner.dli_fbase||!owner.dli_fname||
           std::strcmp(owner.dli_fname,"/opt/steamvr/bin/linuxarm64/vrserver"))return false;
        auto base=reinterpret_cast<uintptr_t>(owner.dli_fbase);
        if(table!=base+frame_compat::inputTableRva||entry!=base+frame_compat::inputEntries[3])return false;
        constexpr auto& fingerprint=frame_compat::scalarFingerprint;
        unsigned char bytes[16]{};
        if(!reader.read(entry,bytes,16)||std::memcmp(bytes,fingerprint,16)||!readonlyPage(table+24))return false;
#ifdef FRAME_COMPAT_STRUCTURAL_AUDIT
        {   // Diagnostic only: the v0.1.0 gate above remains the sole decision.
            auto live=frame_runtime_facts::classifyLive(input);
            log(frame_runtime_facts::formatVerdict(live.first.verdict==frame_compat_check::Verdict::Known?
                "known_gate_pass":"known_gate_pass_classifier_disagrees",live.first,live.second,1).c_str());
        }
#endif
        Dl_info self{};
        if(!dladdr(reinterpret_cast<void*>(&update),&self)||!self.dli_fname||
           !dlopen(self.dli_fname,RTLD_NOW|RTLD_NOLOAD|RTLD_NODELETE))return false;
        // Intentionally retain the module until process exit, including after Cleanup.
        auto original=reinterpret_cast<frame_scalar::ScalarUpdate>(entry);
        resident.original.store(original,std::memory_order_release);
        resident.total.store(0);resident.selected.store(0);
        transaction=std::make_unique<frame_scalar::ReadonlySlotTransaction>(reinterpret_cast<uintptr_t*>(table+24),entry,reinterpret_cast<uintptr_t>(&update));
        auto result=transaction->install([&](bool writable){return protect(writable);});
        attached=result==frame_scalar::SlotResult::Changed;
        if(!attached){
#ifdef FRAME_COMPAT_OBSERVATION_UI
            // The install may have changed the slot before protection failed.
            if(result==frame_scalar::SlotResult::ProtectionFailure)stop(true);
#endif
            log("FrameScalarOffProbe: attach refused/failed; inspect memory protection before retry");return false;
        }
        deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
#if defined(FRAME_SCALAR_SUPERVISED_ON_CANDIDATE) || defined(FRAME_SCALAR_ALL_FINGERS_BETA)
        log("FrameScalarOffProbe: attached UpdateScalar slot only; supervised trial initially OFF; explicit VR START required");
#else
        log("FrameScalarOffProbe: attached UpdateScalar slot only; " FRAME_SCALAR_ACTIVITY "; all calls forwarded unchanged");
#endif
        return true;
    }
#ifdef FRAME_COMPAT_OBSERVATION_UI
    void stop(bool protectionUncertain=false){
        experimentalLease.store(0,std::memory_order_release);
#else
    void stop(){
#endif
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
        allFingers.clear();
#endif
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
        shadow.clear();
#endif
        if(!transaction)return;
#ifdef FRAME_COMPAT_OBSERVATION_UI
        auto result=transaction->restoreBounded([&](bool writable){return protect(writable);},protectionUncertain);
        transaction.reset();
        attached=false;
        restartRequired_=result!=frame_scalar::SlotResult::Changed&&result!=frame_scalar::SlotResult::AlreadyOriginal;
#else
        auto result=transaction->restore([&](bool writable){return protect(writable);});
#endif
        if(result==frame_scalar::SlotResult::Changed||result==frame_scalar::SlotResult::AlreadyOriginal){
            attached=false;
            if(log)log("FrameScalarOffProbe: original slot restored; module retained for cached/in-flight calls");
        }else if(log)log("FrameScalarOffProbe: restore refused/failed; no foreign value overwritten; unregister and restart normally");
    }
    void tick(){
        if(!attached)return;
        if(std::chrono::steady_clock::now()>=deadline){stop();return;}
#ifndef FRAME_SCALAR_STANDALONE_BETA
        char text[160]{};
        std::snprintf(text,sizeof(text),"FrameScalarOffProbe: calls=%llu selected_calls=%llu " FRAME_SCALAR_ACTIVITY "",
            static_cast<unsigned long long>(resident.total.load()),static_cast<unsigned long long>(resident.selected.load()));
        if(log)log(text);
        auto targets=std::atomic_load(&resident.targets);
        if(targets)for(size_t i=0;i<targets->handles.size();++i)if(targets->handles[i]){
            auto& count=targets->counts[i];char entry[240]{};
            std::snprintf(entry,sizeof(entry),"FrameScalarOffProbe: component=0x%llx calls=%llu successes=%llu failures=%llu last_result=%d " FRAME_SCALAR_ACTIVITY "",
                static_cast<unsigned long long>(targets->handles[i]),static_cast<unsigned long long>(count.calls.load()),
                static_cast<unsigned long long>(count.successes.load()),static_cast<unsigned long long>(count.failures.load()),count.lastResult.load());
            if(log)log(entry);
        }
#endif
    }
};
}
