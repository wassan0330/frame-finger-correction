#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace frame_observation {
enum class State { Idle, Observing, Verified, Active, Stopped, Rejected, RestorePending, RestartRequired };
enum class Reason { None, InvalidIdentity, Clock, Timeout, SessionLost, IdentityChanged,
                    BadValue, BadResult, ReceiverChanged, RestoreFailed };
inline const char* reasonCode(Reason reason){
    switch(reason){
    case Reason::None:return "none";
    case Reason::InvalidIdentity:return "invalid_identity";
    case Reason::Clock:return "clock";
    case Reason::Timeout:return "timeout";
    case Reason::SessionLost:return "session_lost";
    case Reason::IdentityChanged:return "identity_changed";
    case Reason::BadValue:return "bad_value";
    case Reason::BadResult:return "bad_result";
    case Reason::ReceiverChanged:return "receiver_changed";
    case Reason::RestoreFailed:return "restore_failed";
    }
    return "unknown";
}
using Handles = std::array<uint64_t, 8>;

// Control-plane policy only. Hook code must never call the original with
// a modified value until the native controller explicitly enters Active.
class Trial {
    State state_ = State::Idle;
    Reason reason_ = Reason::None;
    bool used_ = false;
    Handles handles_{};
    std::array<uintptr_t, 8> receivers_{};
    std::array<unsigned, 8> count_{};
    uint64_t session_ = 0, deadline_ = 0, last_ = 0, verifiedAt_ = 0;
    unsigned restoreAttempts_ = 0;
    State terminal_ = State::Stopped;

    void finish(State terminal, Reason reason) {
        terminal_ = terminal;
        reason_ = reason;
        state_ = State::RestorePending;
    }
public:
    static constexpr uint64_t observationMs = 15000;
    static constexpr unsigned requiredSamples = 3, maxRestoreAttempts = 3;
    State state() const { return state_; }
    Reason reason() const { return reason_; }
    uint64_t verifiedAt() const { return verifiedAt_; }
    unsigned witnessed() const {
        unsigned result = 0;
        for (unsigned i = 0; i < count_.size(); ++i)
            if (count_[i] >= requiredSamples) result |= 1u << i;
        return result;
    }
    bool start(bool structuralPass, bool optIn, uint64_t session, const Handles& handles, uint64_t now) {
        if (used_ || !structuralPass || !optIn || !session || !now || now > UINT64_MAX - observationMs)
            return false;
        for (unsigned i = 0; i < handles.size(); ++i) {
            if (!handles[i]) return false;
            for (unsigned j = 0; j < i; ++j) if (handles[j] == handles[i]) return false;
        }
        used_ = true;
        handles_ = handles;
        session_ = session;
        deadline_ = now + observationMs;
        last_ = now;
        state_ = State::Observing;
        return true;
    }
    bool check(uint64_t session, const Handles& handles, uint64_t now) {
        if (state_ != State::Observing && state_ != State::Verified && state_ != State::Active) return false;
        if (now < last_) finish(State::Rejected, Reason::Clock);
        else if (!session || session != session_) finish(State::Rejected, Reason::SessionLost);
        else if (handles != handles_) finish(State::Rejected, Reason::IdentityChanged);
        else if (state_ == State::Observing && now >= deadline_) finish(State::Rejected, Reason::Timeout);
        else { last_ = now; return true; }
        return false;
    }
    void sample(uint64_t component, uintptr_t receiver, float value, int result, uint64_t now) {
        if (!check(session_, handles_, now)) return;
        for (unsigned i = 0; i < handles_.size(); ++i) {
            if (component != handles_[i]) continue;
            if (!std::isfinite(value) || value < 0 || value > 1) finish(State::Rejected, Reason::BadValue);
            else if (result != 0) finish(State::Rejected, Reason::BadResult);
            else if (!receiver || (receivers_[i] && receivers_[i] != receiver))
                finish(State::Rejected, Reason::ReceiverChanged);
            else {
                receivers_[i] = receiver;
                if (count_[i] < requiredSamples) ++count_[i];
                if (state_ == State::Observing && witnessed() == 255) { state_ = State::Verified; verifiedAt_ = now; }
            }
            return;
        }
    }
    bool allows(uint64_t component,uintptr_t receiver,float value) const {
        if(state_!=State::Active||!receiver||!std::isfinite(value)||value<0||value>1)return false;
        for(unsigned i=0;i<handles_.size();++i)
            if(handles_[i]==component)return receivers_[i]==receiver;
        return false;
    }
    bool activate(bool explicitOn, uint64_t session, const Handles& handles, uint64_t now) {
        if (!check(session, handles, now) || !explicitOn || state_ != State::Verified) return false;
        state_ = State::Active;
        return true;
    }
    void stop() {
        if (state_ == State::Observing || state_ == State::Verified || state_ == State::Active)
            finish(State::Stopped, Reason::None);
    }
    void restored(bool confirmed, bool safeToRetry) {
        if (state_ != State::RestorePending) return;
        if (confirmed) state_ = terminal_;
        else if (++restoreAttempts_ >= maxRestoreAttempts || !safeToRetry) {
            state_ = State::RestartRequired;
            reason_ = Reason::RestoreFailed;
        }
    }
};
}
