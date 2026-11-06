#include "continuous-correction-supervisor.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
using namespace frame_supervision;
int main(){
    Identity key{"test",0x200000023,1234,1,1,1};
    Evidence evidence{true,true,true,true,1000,1000,1,1250};
    Supervisor supervisor;
    assert(!supervisor.start(false,1,key,evidence,1000));
    assert(supervisor.start(true,1,key,evidence,1000));auto old=supervisor.token();
    assert(old->allowed(1000)&&!old->allowed(1250));
    assert(old->gate(key.receiver,key.component,.7f,0,1001)==0);
    assert(old->gate(key.receiver,key.component,.7f,0,1250)==.7f);
    assert(old->gate(key.receiver+1,key.component,.7f,0,1001)==.7f);
    assert(old->gate(key.receiver,key.component+1,.7f,0,1001)==.7f);
    assert(old->gate(key.receiver,key.component,.7f,2,1001)==.7f);
    evidence.pcProofUntilMs=1550;evidence.metadataMs=evidence.heartbeatMs=1300;++evidence.heartbeatSequence;
    assert(!supervisor.refresh(key,evidence,1300));assert(!old->allowed(1300));
    assert(!supervisor.start(true,1,key,evidence,1300));
    assert(supervisor.start(true,2,key,evidence,1300));
    for(uint64_t now=1350;now<3601300;now+=50){
        evidence.pcProofUntilMs=now+250;evidence.metadataMs=now;
        if(now%100==0){evidence.heartbeatMs=now;++evidence.heartbeatSequence;}
        assert(supervisor.refresh(key,evidence,now));assert(supervisor.token()->allowed(now));
    }
    auto live=supervisor.token();supervisor.stop();assert(!live->allowed(3601250));
    assert(live->gate(key.receiver,key.component,.7f,0,3601250)==.7f);
    // Verify each identity field and every evidence flag independently revokes.
    uint64_t session=3;
    for(unsigned field=0;field<10;++field){
        uint64_t now=4000000+field*10;
        evidence={true,true,true,true,now,now,1,now+250};
        assert(supervisor.start(true,session++,key,evidence,now));
        auto changed=key;auto bad=evidence;
        switch(field){case 0:changed.serial="other";break;case 1:++changed.component;break;
        case 2:++changed.receiver;break;case 3:++changed.generation;break;case 4:++changed.runtimeEpoch;break;
        case 5:++changed.profileRevision;break;case 6:bad.compatible=false;break;case 7:bad.connected=false;break;
        case 8:bad.identityVerified=false;break;case 9:bad.pcCorrectionOff=false;break;}
        auto token=supervisor.token();assert(!supervisor.refresh(changed,bad,now+1));assert(!token->allowed(now+1));
    }
    evidence={true,true,true,true,4000200,4000200,1,4000450};
    assert(supervisor.start(true,session++,key,evidence,4000200));
    evidence.heartbeatMs=4000201;assert(!supervisor.refresh(key,evidence,4000201)); // Same sequence, changed timestamp.
    evidence={true,true,true,true,4000210,4000210,2,4000460};
    assert(supervisor.start(true,session++,key,evidence,4000210));
    auto token=supervisor.token();assert(!supervisor.refresh(key,evidence,4000209));assert(!token->allowed(4000210));
    evidence={true,true,true,true,4000220,4000220,3,4000470};
    assert(supervisor.start(true,session++,key,evidence,4000220));token=supervisor.token();
    std::atomic<bool> done{false};
    std::thread reader([&]{while(!done.load())token->allowed(4000221);});
    supervisor.stop();done.store(true);reader.join();assert(!token->allowed(4000221));
    Evidence shortProof{true,true,true,true,5000000,5000000,1,5000010};
    assert(supervisor.start(true,session++,key,shortProof,5000000));
    token=supervisor.token();assert(token->allowed(5000009));
    assert(!token->allowed(5000010));
    shortProof.metadataMs=5000005; // Metadata refresh cannot extend an old PC proof.
    shortProof.heartbeatMs=5000005;++shortProof.heartbeatSequence;
    assert(supervisor.refresh(key,shortProof,5000005));
    assert(!token->allowed(5000010));
    std::puts("PASS simulated 1h renewal, callback expiry without monitor, explicit restart, identity/evidence revocation, replay, clock regression and concurrent stop");
}
