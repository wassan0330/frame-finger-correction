// test-flags: -DFRAME_SCALAR_SUPERVISED_ON_CANDIDATE
#include "scalar-shadow-bridge.hpp"
#include <cassert>
#include <cstdio>
static float received=0;static int32_t original(void*,uint64_t,float value,double){received=value;return 0;}
int main(){
 using namespace frame_scalar;using namespace frame_supervision;
 ScalarShadowBridge bridge;
 DeviceSnapshot device{2,"test-left","frame_controller",Hand::Left,true,1};
 ExistingScalar binding{0x200000023,0x100000002,"/input/capsense/pinky","test-left",Hand::Left,true,true,1,false};
 assert(bridge.configure(binding,device,0x4000,1000));
 auto receiver=reinterpret_cast<void*>(uintptr_t(1234));
 bridge.forward(original,receiver,binding.component,.6f,0,1000);assert(received==.6f);
 assert(bridge.configure(binding,device,0x4000,1001));
 auto facts=bridge.monitoringFacts();assert(facts.verified);
 Identity key{facts.serial,facts.component,facts.receiver,facts.generation,1,1};
 Evidence evidence{true,true,true,true,1001,0,0,1251};UiHeartbeat ui{1,1,1001,1};
 bridge.supervised.tick(key,evidence,ui,true,"FRAME_VR_SUPERVISED_1 100 aabbccddeeff0011 START 0.8",100,1001);
 assert(bridge.supervised.running());
 bridge.forward(original,receiver,binding.component,.6f,0,1002);assert(received==.6f);
 bridge.forward(original,receiver,binding.component,.6f,0,1003);assert(received==0);
 bridge.forward(original,receiver,binding.component+1,.6f,0,1004);assert(received==.6f);
 bridge.clear();assert(!bridge.supervised.running());
 bridge.forward(original,receiver,binding.component,.6f,0,1005);assert(received==.6f);
 assert(bridge.configure(binding,device,0x4000,1006));
 bridge.forward(original,receiver,binding.component,.6f,0,1007);assert(received==.6f);
 assert(!bridge.supervised.ready);
 std::puts("PASS supervised native bridge: verified left-pinky receiver, first raw, mapping, other component bypass, clear/disconnect revocation");
}
