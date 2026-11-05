#pragma once
#include "scalar-monitor-request.hpp"
#include <cmath>
#include <charconv>

namespace frame_scalar {
struct BoundedRequest {
    std::string id;
    bool on=false,pcOffConfirmed=false;
    unsigned seconds=0;
    uint64_t generation=0;
    float deadzone=0;
};
inline bool parseBoundedRequest(const std::string& text,int64_t now,BoundedRequest& result){
    if(text.size()>256)return false;
    std::istringstream input(text);
    std::string format,command,id,confirmation,extra,generationText;
    int64_t timestamp=0,seconds=0;uint64_t generation=0;float deadzone=0;
    if(!(input>>format>>timestamp>>id>>command>>seconds>>deadzone>>generationText>>confirmation)||input>>extra||
       format!="FRAME_SCALAR_BOUNDED_1"||now<0||timestamp<0||timestamp>now||now-timestamp>2||
       id.size()!=16||!std::isfinite(deadzone))return false;
    auto parsed=std::from_chars(generationText.data(),generationText.data()+generationText.size(),generation);
    if(parsed.ec!=std::errc{}||parsed.ptr!=generationText.data()+generationText.size())return false;
    for(char c:id)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    if(command=="OFF"){
        if(seconds!=0||deadzone!=0||generation!=0||confirmation!="STOP")return false;
        result={id,false,false,0,0,0};return true;
    }
    if(command!="ON"||seconds<1||seconds>5||deadzone<0||deadzone>.9f||
       !generation||confirmation!="PC_OFF_CONFIRMED")return false;
    result={id,true,true,static_cast<unsigned>(seconds),generation,deadzone};return true;
}
}
