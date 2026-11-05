#pragma once
#include "continuous-correction-supervisor.hpp"
#include "scalar-off-forwarding.hpp"
#include <mutex>

namespace frame_supervision {
// Candidate building block, NOT connected to the native driver or UI.
// A fixed 60-second upper bound is independent of renewable evidence.
class SupervisedScalarPublication {
    struct Session {
        std::shared_ptr<const Permit> permit;
        Identity identity;
        uint64_t issued=0,until=0;
        float threshold=0;
        std::atomic<bool> enabled{true},witnessed{false};
    };
    std::atomic<uint64_t> mappedCalls{0};
    Supervisor supervisor;
    std::mutex control;
    std::shared_ptr<Session> current;
    void stopLocked(){
        auto old=std::atomic_load(&current);
        if(old)old->enabled.store(false);
        supervisor.stop();std::atomic_store(&current,std::shared_ptr<Session>{});
    }
public:
    ~SupervisedScalarPublication(){stop();}
    uint64_t changed()const{return mappedCalls.load();}
    void stop(){std::lock_guard<std::mutex> lock(control);stopLocked();}
    bool start(bool explicitStart,uint64_t session,const Identity& identity,const Evidence& evidence,
               uint64_t now,uint64_t seconds,float threshold){
        std::lock_guard<std::mutex> lock(control);stopLocked();
        if(!explicitStart||seconds<1||seconds>60||!now||now>UINT64_MAX-60000||
           !std::isfinite(threshold)||threshold<0||threshold>.9f)return false;
        if(!supervisor.start(true,session,identity,evidence,now))return false;
        auto next=std::make_shared<Session>();next->permit=supervisor.token();next->identity=identity;
        next->issued=now;next->until=now+seconds*1000;next->threshold=threshold;
        std::atomic_store(&current,next);return true;
    }
    bool refresh(const Identity& identity,const Evidence& evidence,uint64_t now){
        std::lock_guard<std::mutex> lock(control);auto session=std::atomic_load(&current);
        if(!session||!session->enabled.load()||now<session->issued||now>=session->until||
           !supervisor.refresh(identity,evidence,now)){stopLocked();return false;}
        return true;
    }
    int32_t forward(frame_scalar::ScalarUpdate original,void* receiver,uint64_t component,
                    float input,double offset,uint64_t now){
        auto session=std::atomic_load(&current);float output=input;
        const auto receiverId=reinterpret_cast<uintptr_t>(receiver);
        const bool target=session&&receiverId==session->identity.receiver&&component==session->identity.component;
        if(target&&session->enabled.load()&&now>=session->issued&&now<session->until&&session->witnessed.load()){
            const float mapped=std::clamp((input-session->threshold)/(1-session->threshold),0.f,1.f);
            output=session->permit->gate(receiverId,component,input,mapped,now);
            if(!session->enabled.load())output=input;
        }
        if(output!=input&&std::isfinite(input))mappedCalls.fetch_add(1,std::memory_order_relaxed);
        const auto result=original(receiver,component,output,offset);
        if(target&&session->enabled.load()){
            // A failed update revokes this session; fresh evidence cannot re-arm it.
            if(result!=0)session->enabled.store(false);
            else if(std::isfinite(input)&&input>=0&&input<=1)session->witnessed.store(true);
        }
        return result;
    }
};
}
