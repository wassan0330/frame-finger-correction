#include "supervised-scalar-publication.hpp"
#include <cassert>
#include <cstdio>
using namespace frame_supervision;
static float last=0;static unsigned calls=0;static int32_t result=0;
static int32_t original(void*,uint64_t,float value,double){last=value;++calls;return result;}
int main(){
 SupervisedScalarPublication mapping;
 Identity key{"serial",0x100000023,1234,1,1,1};
 Evidence evidence{true,true,true,true,1000,1000,1,1250};
 auto receiver=reinterpret_cast<void*>(uintptr_t(1234));
 assert(!mapping.start(true,1,key,evidence,1000,61,.8f));
 assert(mapping.start(true,1,key,evidence,1000,60,.8f));
 mapping.forward(original,receiver,key.component,.6f,0,1001);assert(last==.6f); // First call raw.
 mapping.forward(original,receiver,key.component,.6f,0,1002);assert(last==0);
 mapping.forward(original,receiver,key.component,.9f,0,1250);assert(last==.9f); // No monitor needed for expiry.
 evidence={true,true,true,true,1300,1300,2,1550};
 assert(!mapping.refresh(key,evidence,1300)); // Cannot resume expired permit.
 assert(mapping.start(true,2,key,evidence,1300,1,.8f));
 mapping.forward(original,receiver,key.component,.6f,0,1301);assert(last==.6f);
 mapping.forward(original,receiver,key.component,.6f,0,1302);assert(last==0);
 for(uint64_t now=1350;now<2300;now+=50){
  evidence={true,true,true,true,now,now,now,now+250};assert(mapping.refresh(key,evidence,now));
 }
 mapping.forward(original,receiver,key.component,.6f,0,2300);assert(last==.6f); // Fixed cap, despite renewal.
 assert(!mapping.refresh(key,evidence,2300));
 evidence={true,true,true,true,3000,3000,3,3250};
 assert(mapping.start(true,3,key,evidence,3000,60,.8f));
 mapping.forward(original,reinterpret_cast<void*>(uintptr_t(4321)),key.component,.6f,0,3001);assert(last==.6f);
 mapping.forward(original,receiver,key.component,.6f,0,3002);assert(last==.6f); // Wrong receiver cannot witness.
 result=7;mapping.forward(original,receiver,key.component,.6f,0,3003);assert(last==0);
 result=0;mapping.forward(original,receiver,key.component,.6f,0,3004);assert(last==.6f);
 assert(!mapping.refresh(key,evidence,3005));
 assert(!mapping.start(true,3,key,evidence,3005,60,.8f));
 assert(mapping.start(true,4,key,evidence,3005,60,.8f));
 evidence.pcCorrectionOff=false;assert(!mapping.refresh(key,evidence,3006));
 mapping.forward(original,receiver,key.component,.6f,0,3007);assert(last==.6f);
 assert(calls==11);
 std::puts("PASS candidate mapper: raw witness, PC expiry, fixed cap, wrong receiver, update failure, explicit restart and PC OFF loss");
}

