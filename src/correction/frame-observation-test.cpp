#include "frame-observation.hpp"
#include <cassert>
#include <limits>
using namespace frame_observation;

int main() {
    const Handles inputs{1,2,3,4,5,6,7,8};
    Trial trial;
    assert(!trial.start(false,true,1,inputs,100));
    assert(!trial.start(true,false,1,inputs,100));
    auto duplicate = inputs; duplicate[1] = 1;
    assert(!trial.start(true,true,1,duplicate,100));
    assert(trial.start(true,true,1,inputs,100));
    assert(!trial.activate(true,1,inputs,100));
    trial.sample(100,0,-1,-1,100);
    assert(trial.witnessed()==0);
    for (auto component : inputs)
        for (unsigned n=0;n<Trial::requiredSamples;++n) trial.sample(component,10,0.5f,0,101);
    assert(trial.state()==State::Verified && trial.witnessed()==255 && trial.verifiedAt()==101);
    assert(!trial.allows(1,10,.5f));
    assert(!trial.activate(false,1,inputs,102));
    assert(trial.activate(true,1,inputs,102));
    assert(trial.allows(1,10,.5f));
    assert(!trial.allows(1,11,.5f) && !trial.allows(9,10,.5f));
    assert(!trial.allows(1,10,1.1f));
    trial.sample(1,11,0.5f,0,103);
    assert(trial.state()==State::RestorePending && trial.reason()==Reason::ReceiverChanged);
    assert(!trial.allows(1,10,.5f));
    trial.restored(false,true); trial.restored(false,true);
    assert(trial.state()==State::RestorePending);
    trial.restored(false,true);
    assert(trial.state()==State::RestartRequired && trial.reason()==Reason::RestoreFailed);
    assert(!trial.start(true,true,1,inputs,104));

    // Saved ON can attach before the user puts on the headset. Silence must
    // remain pass-through, not permanently reject the current vrserver session.
    Trial idle; assert(idle.start(true,true,1,inputs,100));
    constexpr uint64_t hour=3600000;
    assert(idle.check(1,inputs,hour));
    assert(idle.state()==State::Observing && idle.reason()==Reason::None);
    assert(!idle.allows(1,10,.5f) && !idle.activate(true,1,inputs,hour));
    // Sparse updates from different fingers, including long pauses, all count.
    uint64_t at=hour;
    for(auto component:inputs){
        for(unsigned n=0;n<Trial::requiredSamples;++n){
            at+=hour;idle.sample(component,10,.5f,0,at);
        }
    }
    assert(idle.state()==State::Verified && idle.witnessed()==255);
    assert(!idle.allows(1,10,.5f));
    assert(idle.activate(true,1,inputs,at+1));
    assert(idle.allows(1,10,.5f));
    Trial partial; assert(partial.start(true,true,1,inputs,100));
    for(unsigned n=0;n<Trial::requiredSamples;++n)partial.sample(1,10,.5f,0,101);
    assert(partial.check(1,inputs,24*hour) && partial.witnessed()==1);
    assert(partial.state()==State::Verified);
    assert(partial.activate(true,1,inputs,24*hour));
    assert(partial.allows(1,10,.5f));
    for(unsigned i=1;i<inputs.size();++i)assert(!partial.allows(inputs[i],10,.5f));
    // The same constant value is sufficient; no curl change or gesture needed.
    partial.sample(2,20,.5f,0,24*hour+1);
    assert(partial.witnessed()==3 && partial.allows(2,20,.5f));
    assert(!partial.allows(3,20,.5f));
    partial.stop();assert(partial.state()==State::RestorePending);
    partial.restored(true,false);assert(partial.state()==State::Stopped);
    // Waiting longer never weakens invalid-value/identity/heartbeat checks.
    for (auto reason : {Reason::BadResult,Reason::BadValue,Reason::SessionLost,Reason::IdentityChanged,Reason::Clock}) {
        Trial t; assert(t.start(true,true,1,inputs,100));
        assert(t.check(1,inputs,24*hour));
        switch (reason) {
        case Reason::BadResult: t.sample(1,10,.5f,1,24*hour+1); break;
        case Reason::BadValue: t.sample(1,10,std::numeric_limits<float>::quiet_NaN(),0,24*hour+1); break;
        case Reason::SessionLost: t.check(2,inputs,24*hour+1); break;
        case Reason::IdentityChanged: t.check(1,duplicate,24*hour+1); break;
        default: t.check(1,inputs,99); break;
        }
        assert(t.state()==State::RestorePending && t.reason()==reason);
        t.restored(false,false); assert(t.state()==State::RestartRequired);
    }
}
