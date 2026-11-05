#pragma once
#include <cstdint>
#include <sstream>
#include <string>
namespace frame_settings {
inline std::string trialStatus(const std::string& text,const std::string& id,int64_t now){
    std::istringstream input(text);std::string tag,nonce,state,extra;int64_t stamp=0;
    if(!(input>>tag>>stamp>>nonce>>state)||input>>extra||tag!="FRAME_VR_STATUS_1"||
       nonce!=id||stamp<0||stamp>now)return "Status unconfirmed";
    if(state=="STOPPED"||state=="ERROR")return state; // Terminal acknowledgement persists.
    if((state=="RUNNING"||state=="PREPARING")&&now-stamp<=2)return state;
    return "Status unconfirmed";
}
inline bool brokerReady(const std::string& text,int64_t now){
    std::istringstream input(text);std::string tag,state,extra;int64_t stamp=0;
    return (input>>tag>>stamp>>state)&&!(input>>extra)&&tag=="FRAME_VR_BROKER_1"&&
        state=="READY"&&stamp>=0&&stamp<=now&&now-stamp<=2;
}
}
