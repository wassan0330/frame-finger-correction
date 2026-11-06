#include "frame-monitor-shadow.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace frame_supervision;
    UiHeartbeat ui;
    assert(parseHeartbeat("FRAME_UI_HEARTBEAT_1 1 1 1000 1",1000,ui));
    assert(!parseHeartbeat("FRAME_UI_HEARTBEAT_1 1 1 1000 1",1500,ui));
    assert(!parseHeartbeat("FRAME_UI_HEARTBEAT_1 1 -1 1000 1",1000,ui));
    assert(!parseHeartbeat("FRAME_UI_HEARTBEAT_1 0 0 0 0",1000,ui));
    FrameMonitorShadow monitor;
    Identity key{"test",0x200000023,1234,1,1,1};
    Evidence facts{true,true,true,true,1000,0,0,1250};
    auto notReady=facts;notReady.pcCorrectionOff=false;
    assert(!monitor.evaluate(key,notReady,ui,1000));
    auto undiscovered=key;undiscovered.receiver=0;
    assert(!monitor.evaluate(undiscovered,facts,ui,1000));
    assert(monitor.evaluate(key,facts,ui,1000));
    facts.pcProofUntilMs=1350;facts.metadataMs=1100;ui.sequence=2;ui.observedMs=1100;
    assert(monitor.evaluate(key,facts,ui,1100));
    facts.pcProofUntilMs=1750;facts.metadataMs=1500;ui.sequence=3;ui.observedMs=1500;
    assert(!monitor.evaluate(key,facts,ui,1500)); // Expired session cannot auto-resume.
    ui.session=2;assert(monitor.evaluate(key,facts,ui,1500));
    facts.pcCorrectionOff=false;assert(!monitor.evaluate(key,facts,ui,1501));
    facts.pcCorrectionOff=true;ui.sequence=4;ui.observedMs=1502;
    assert(!monitor.evaluate(key,facts,ui,1502)); // Missing proof remains latched.
    ui.session=3;assert(monitor.evaluate(key,facts,ui,1502));
    ++key.profileRevision;assert(!monitor.evaluate(key,facts,ui,1503));
    monitor.stop();
    std::puts("PASS Frame monitoring bridge: UI freshness, evidence, expiry latch and profile revision; no correction API");
}
