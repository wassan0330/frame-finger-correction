#pragma once
#include <cstdint>
#include <sstream>
#include <string>

namespace frame_scalar {
struct MonitorRequest {std::string id;unsigned seconds=0;};
inline bool parseMonitorRequest(const std::string& text,int64_t now,MonitorRequest& result){
    if(text.size()>160)return false;
    std::istringstream input(text);std::string format,id,extra;int64_t timestamp=0,seconds=0;
    if(!(input>>format>>timestamp>>id>>seconds)||input>>extra||format!="FRAME_SCALAR_MONITOR_1"||
       now<0||timestamp<0||timestamp>now||now-timestamp>10||seconds<0||seconds>60||id.size()!=16)return false;
    for(char c:id)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    result={id,static_cast<unsigned>(seconds)};return true;
}
}
