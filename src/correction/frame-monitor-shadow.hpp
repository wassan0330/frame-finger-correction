#pragma once
#include "continuous-correction-supervisor.hpp"
#include "frame-ui-heartbeat.hpp"
namespace frame_supervision {
// Diagnostic only: the internal permit is never exposed to an input mapper.
class FrameMonitorShadow {
    Supervisor supervisor;
    uint64_t session=0;
public:
    void stop(){supervisor.stop();}
    bool evaluate(const Identity& key,const Evidence& facts,const UiHeartbeat& ui,uint64_t now){
        if(key.profileRevision!=ui.revision){supervisor.stop();return false;}
        auto evidence=facts;
        evidence.heartbeatMs=ui.observedMs;evidence.heartbeatSequence=ui.sequence;
        if(ui.session!=session){
            // Initial discovery is not a failed running session. Wait until
            // every producer is ready before consuming the UI session ID.
            // Once consumed, failures remain latched until a new UI session.
            if(key.serial.empty()||!key.component||!key.receiver||!key.generation||!key.runtimeEpoch||
               !facts.compatible||!facts.connected||!facts.identityVerified||!facts.pcCorrectionOff||
               !facts.metadataMs||facts.metadataMs>now||now-facts.metadataMs>=250||
               facts.pcProofUntilMs<=now||facts.pcProofUntilMs-now>250)return false;
            session=ui.session;
            supervisor.start(true,session,key,evidence,now); // Shadow evaluation, not an ON request.
        }else supervisor.refresh(key,evidence,now);
        auto token=supervisor.token();return token&&token->allowed(now);
    }
    Reason reason()const{return supervisor.reason();}
};
}
