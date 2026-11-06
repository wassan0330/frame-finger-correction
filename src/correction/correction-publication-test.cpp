#include "correction-publication.hpp"
#include <cassert>
#include <thread>
#include <vector>
using namespace frame_scalar;
static thread_local float received;
static int32_t original(void*,uint64_t,float value,double offset){
    assert(offset==.125);received=value;return 0;
}
int main(){
    CorrectionPublication publication;
    CorrectionLease lease;
    lease.explicitOn=lease.runtimeVerified=true;
    lease.receiver=1234;lease.issuedMs=1000;lease.deadlineMs=6000;
    lease.binding={0x200000023,0x100000002,"/input/capsense/pinky","test",Hand::Left,true,true,8,false};
    CorrectionEvidence evidence{{2,"test","frame_controller",Hand::Left,true,8},true,1100};
    auto call=[&](uint64_t now){publication.forward(original,reinterpret_cast<void*>(1234),lease.binding.component,.7f,.125,now);return received;};
    publication.begin(lease,evidence);
    assert(call(1200)==.7f);assert(call(1200)==0);
    assert(call(1400)==.7f); // Heartbeat expired.
    evidence.observedMs=5900;assert(publication.refresh(evidence));
    assert(call(5999)==0);assert(call(6000)==.7f); // No lease extension.
    evidence.device.generation=9;assert(!publication.refresh(evidence));
    assert(call(1200)==.7f);
    evidence.device.generation=8;evidence.observedMs=1100;
    // Exercise publication, revocation and callbacks concurrently. Every call
    // must see either an authorized complete snapshot or the unchanged value.
    std::vector<std::thread> readers;
    for(int n=0;n<4;++n)readers.emplace_back([&]{for(int i=0;i<20000;++i){
        float value=call(1200);assert(value==0||value==.7f);
    }});
    for(int i=0;i<5000;++i){publication.begin(lease,evidence);publication.refresh(evidence);publication.revoke();}
    for(auto& reader:readers)reader.join();
    publication.revoke();assert(call(1200)==.7f);
}
