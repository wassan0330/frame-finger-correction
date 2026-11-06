#include "supervised-trial-controller.hpp"
#include <cassert>
#include <cstdio>
using namespace frame_supervision;
static float last=0;static unsigned calls=0;
static int32_t original(void*,uint64_t,float value,double){last=value;++calls;return 0;}
int main(){
 Identity key{"left",0x100000023,1234,1,1,1};
 auto receiver=reinterpret_cast<void*>(uintptr_t(1234));
 UiHeartbeat ui{1,1,1000,1};Evidence facts{true,true,true,true,1000,0,0,1250};
 const std::string start="FRAME_VR_SUPERVISED_1 100 aabbccddeeff0011 START 0.8";
 const std::string stop="FRAME_VR_SUPERVISED_1 100 aabbccddeeff0022 STOP 0.8";
 TrialIntent parsed;
 assert(!parseSupervisedIntent("FRAME_VR_TRIAL_1 100 aabbccddeeff0011 START 0.8",100,parsed));
 assert(!parseSupervisedIntent(start,103,parsed));
 assert(!parseSupervisedIntent("FRAME_VR_SUPERVISED_1 100 aabbccddeeff0011 START nan",100,parsed));
 SupervisedTrialController trial;
 trial.tick(key,facts,ui,true,"",100,1000);assert(trial.ready&&!trial.running());
 trial.forward(original,receiver,key.component,.6f,0,1000);assert(last==.6f);
 trial.tick(key,facts,ui,true,start,100,1000);assert(trial.running());
 trial.forward(original,receiver,key.component,.6f,0,1001);assert(last==.6f);
 trial.forward(original,receiver,key.component,.6f,0,1002);assert(last==0);
 trial.tick(key,facts,ui,true,stop,100,1003);assert(!trial.running()&&trial.state()=="STOPPED");
 trial.forward(original,receiver,key.component,.6f,0,1004);assert(last==.6f);
 trial.tick(key,facts,ui,true,"FRAME_VR_SUPERVISED_1 100 aabbccddeeff0033 START 0.8",100,1005);
 assert(!trial.running()&&trial.state()=="ERROR");
 SupervisedTrialController timeout;
 timeout.tick(key,facts,ui,true,start,100,1000);
 for(uint64_t now=1050;now<21000;now+=50){
  facts={true,true,true,true,now,0,0,now+250};ui.sequence++;ui.observedMs=now;
  timeout.tick(key,facts,ui,true,"",100,now);assert(timeout.running());
 }
 facts={true,true,true,true,21000,0,0,21250};ui.observedMs=21000;ui.sequence++;
 timeout.tick(key,facts,ui,true,"",100,21000);assert(!timeout.running());
 SupervisedTrialController loss;
 ui={4,1,30000,1};facts={true,true,true,true,30000,0,0,30250};
 loss.tick(key,facts,ui,true,start,100,30000);assert(loss.running());
 loss.tick(key,facts,ui,false,"",100,30001);assert(!loss.running());
 ui.sequence++;ui.observedMs=30002;
 loss.tick(key,facts,ui,true,start,100,30002);assert(!loss.running()&&!loss.ready);
 assert(calls==4);
 std::puts("PASS native trial controller: no implicit ON, distinct request tag, explicit START/STOP, one-shot, fixed20s deadline and UI loss latch");
}

