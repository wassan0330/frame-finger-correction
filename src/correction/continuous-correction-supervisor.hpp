#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <cmath>

namespace frame_supervision {
struct Identity {
    std::string serial;
    uint64_t component=0,receiver=0,generation=0,runtimeEpoch=0,profileRevision=0;
    bool operator==(const Identity& other)const{
        return serial==other.serial&&component==other.component&&receiver==other.receiver&&
            generation==other.generation&&runtimeEpoch==other.runtimeEpoch&&profileRevision==other.profileRevision;
    }
};
struct Evidence {
    bool compatible=false,connected=false,identityVerified=false,pcCorrectionOff=false;
    uint64_t metadataMs=0,heartbeatMs=0,heartbeatSequence=0,pcProofUntilMs=0;
};
enum class State {Off,Active,NeedsExplicitStart};
enum class Reason {None,ExplicitStop,InvalidIdentity,InvalidEvidence,Expired,ClockRegression,Replay};
// Input callbacks may retain this token; no UI/process pointer is retained.
// Expiry is enforced when queried even if the control thread stops entirely.
class Permit {
    friend class Supervisor;
    std::atomic<uint64_t> until{0};
    uint64_t issued=0;
    uint64_t receiver=0,component=0;
public:
    bool allowed(uint64_t now)const{
        auto deadline=until.load(std::memory_order_acquire);
        return issued&&now>=issued&&now<deadline;
    }
    float gate(uint64_t targetReceiver,uint64_t targetComponent,float input,float mapped,uint64_t now)const{
        if(targetReceiver!=receiver||targetComponent!=component||!allowed(now)||
           !std::isfinite(input)||input<0||input>1||!std::isfinite(mapped)||mapped<0||mapped>1)return input;
        return mapped;
    }
};
// Single control-thread owner. Only Permit::allowed is called from input threads.
class Supervisor {
    State state_=State::Off;
    Reason reason_=Reason::None;
    Identity identity;
    Evidence previous;
    uint64_t lastNow=0,lastSession=0;
    std::shared_ptr<Permit> permit;
    void invalidate(Reason reason,State state){
        if(permit)permit->until.store(0,std::memory_order_release);
        state_=state;reason_=reason;
    }
    static bool validIdentity(const Identity& key){
        return !key.serial.empty()&&key.component&&key.receiver&&key.generation&&key.runtimeEpoch&&key.profileRevision;
    }
    static bool validEvidence(const Evidence& e,uint64_t now){
        return e.compatible&&e.connected&&e.identityVerified&&e.pcCorrectionOff&&
            e.metadataMs&&e.heartbeatMs&&e.heartbeatSequence&&e.metadataMs<=now&&e.heartbeatMs<=now&&
            e.pcProofUntilMs>now&&e.pcProofUntilMs-now<=250&&now-e.metadataMs<250&&now-e.heartbeatMs<500&&now<=UINT64_MAX-500;
    }
    static uint64_t deadline(const Evidence& e){return std::min({e.metadataMs+250,e.heartbeatMs+500,e.pcProofUntilMs});}
public:
    Supervisor()=default;
    Supervisor(const Supervisor&)=delete;
    Supervisor& operator=(const Supervisor&)=delete;
    ~Supervisor(){stop();}
    State state()const{return state_;}
    Reason reason()const{return reason_;}
    std::shared_ptr<const Permit> token()const{return permit;}
    void stop(){invalidate(Reason::ExplicitStop,State::Off);}
    bool start(bool explicitStart,uint64_t session,const Identity& key,const Evidence& evidence,uint64_t now){
        invalidate(Reason::InvalidEvidence,State::NeedsExplicitStart);
        if(!explicitStart||!session||session<=lastSession)return false;
        lastSession=session; // Consume even a refused request; never replay it later.
        if(!validIdentity(key)){reason_=Reason::InvalidIdentity;return false;}
        if(now<lastNow){reason_=Reason::ClockRegression;return false;}
        if(!validEvidence(evidence,now))return false;
        identity=key;previous=evidence;lastNow=now;
        permit=std::make_shared<Permit>();permit->issued=now;
        permit->receiver=key.receiver;permit->component=key.component;
        permit->until.store(deadline(evidence),std::memory_order_release);
        state_=State::Active;reason_=Reason::None;return true;
    }
    bool refresh(const Identity& key,const Evidence& evidence,uint64_t now){
        if(state_!=State::Active)return false;
        if(now<lastNow){invalidate(Reason::ClockRegression,State::NeedsExplicitStart);return false;}
        lastNow=now;
        // Once expired, fresh evidence alone must not resurrect this session.
        if(!permit->allowed(now)){invalidate(Reason::Expired,State::NeedsExplicitStart);return false;}
        if(!(identity==key)){invalidate(Reason::InvalidIdentity,State::NeedsExplicitStart);return false;}
        if(!validEvidence(evidence,now)){invalidate(Reason::InvalidEvidence,State::NeedsExplicitStart);return false;}
        if(evidence.metadataMs<previous.metadataMs||evidence.heartbeatMs<previous.heartbeatMs||
           evidence.heartbeatSequence<previous.heartbeatSequence||
           (evidence.heartbeatSequence==previous.heartbeatSequence&&evidence.heartbeatMs!=previous.heartbeatMs)){
            invalidate(Reason::Replay,State::NeedsExplicitStart);return false;
        }
        previous=evidence;
        permit->until.store(deadline(evidence),std::memory_order_release);return true;
    }
};
}
