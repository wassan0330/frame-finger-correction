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

    Trial timeout; assert(timeout.start(true,true,1,inputs,100));
    assert(!timeout.check(1,inputs,100+Trial::observationMs));
    assert(timeout.reason()==Reason::Timeout);
    timeout.restored(true,false); assert(timeout.state()==State::Rejected);
    for (auto reason : {Reason::BadResult,Reason::BadValue,Reason::SessionLost,Reason::IdentityChanged,Reason::Clock}) {
        Trial t; assert(t.start(true,true,1,inputs,100));
        switch (reason) {
        case Reason::BadResult: t.sample(1,10,.5f,1,101); break;
        case Reason::BadValue: t.sample(1,10,std::numeric_limits<float>::quiet_NaN(),0,101); break;
        case Reason::SessionLost: t.check(2,inputs,101); break;
        case Reason::IdentityChanged: t.check(1,duplicate,101); break;
        default: t.check(1,inputs,99); break;
        }
        assert(t.state()==State::RestorePending && t.reason()==reason);
        t.restored(false,false); assert(t.state()==State::RestartRequired);
    }
}
