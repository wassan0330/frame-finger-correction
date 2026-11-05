#pragma once
#include <cstdint>
#include <sstream>
#include <string>
namespace frame_supervision {
struct UiHeartbeat {uint64_t session=0,sequence=0,observedMs=0,revision=0;};
inline bool parseHeartbeat(const std::string& text,uint64_t now,UiHeartbeat& output,uint64_t lifetime=500){
    if(!lifetime||lifetime>5000||text.size()>160||text.find('-')!=std::string::npos)return false;
    std::istringstream input(text);std::string tag,extra;UiHeartbeat next;
    if(!(input>>tag>>next.session>>next.sequence>>next.observedMs>>next.revision)||input>>extra||
       tag!="FRAME_UI_HEARTBEAT_1"||!next.session||!next.sequence||!next.observedMs||!next.revision||
       next.observedMs>now||now-next.observedMs>=lifetime)return false;
    output=next;return true;
}
}
