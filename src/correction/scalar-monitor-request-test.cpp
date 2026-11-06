#include "scalar-monitor-request.hpp"
#include <cassert>
#include <iostream>
int main(){
    frame_scalar::MonitorRequest request;
    assert(frame_scalar::parseMonitorRequest("FRAME_SCALAR_MONITOR_1 100 abcdef0123456789 30",100,request));
    assert(request.seconds==30&&request.id=="abcdef0123456789");
    assert(!frame_scalar::parseMonitorRequest("FRAME_SCALAR_MONITOR_1 100 abcdef0123456789 61",100,request));
    assert(!frame_scalar::parseMonitorRequest("FRAME_SCALAR_MONITOR_1 100 abcdef0123456789 30 extra",100,request));
    assert(!frame_scalar::parseMonitorRequest("FRAME_SCALAR_MONITOR_1 100 abcdef0123456789 30",111,request));
    assert(!frame_scalar::parseMonitorRequest("FRAME_SCALAR_MONITOR_1 101 abcdef0123456789 30",100,request));
    assert(!frame_scalar::parseMonitorRequest("FRAME_SCALAR_MONITOR_1 100 invalid 30",100,request));
    assert(frame_scalar::parseMonitorRequest("FRAME_SCALAR_MONITOR_1 100 abcdef0123456789 0",100,request));
    std::cout<<"PASS monitor request expiry/format/duration/id guards\n";
}
