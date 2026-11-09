#include "frame-finger-bank.hpp"
#include <cassert>
#include <cstdio>
using namespace frame_beta;using namespace frame_scalar;
static float received=0;static unsigned calls=0;
static int32_t original(void*,uint64_t,float value,double){received=value;++calls;return 0;}
static std::array<std::optional<Binding>,8> bindings(){
 std::array<std::optional<Binding>,8> out;
 for(unsigned row=0;row<8;++row){unsigned device=row<4?1:2;auto hand=row<4?Hand::Left:Hand::Right;
  std::string serial=row<4?"left":"right";
  out[row]=Binding{ExistingScalar{(uint64_t(device)<<32)|(row%4+10),0x100000000ull|device,fingerPaths[row%4],serial,hand,true,true,0,false},DeviceSnapshot{device,serial,"frame_controller",hand,true,0},0x4000+row*0x100};
 }return out;
}
int main(){
 FingerBank bank;auto input=bindings();bank.configure(input,1000);
 frame_settings::Profile profile;profile.requested=true;for(auto& f:profile.fingers)f={true,false,.2};
 auto receiver=reinterpret_cast<void*>(uintptr_t(0x1234));
 assert(bank.renew(profile,1,true,1000,1000,1000));
 for(unsigned row=0;row<8;++row){
  bank.forward(original,receiver,input[row]->scalar.component,.1f,0,1001);assert(received==.1f); // First real success raw.
  bank.forward(original,receiver,input[row]->scalar.component,.1f,0,1002);assert(received==0);
  bank.forward(original,receiver,input[row]->scalar.component,1.f,0,1003);assert(received==1.f);
 }
 assert(bank.status(1003).witnessed==255);
 profile.fingers[0].selected=false;profile.fingers[7].dynamic=true;
 assert(bank.renew(profile,2,true,1010,1010,1010));
 bank.forward(original,receiver,input[0]->scalar.component,.1f,0,1011);assert(received==.1f);
 bank.forward(original,receiver,input[7]->scalar.component,.6f,0,1011);assert(received==.6f); // Dynamic unlearned bypass.
 assert(bank.renew(profile,2,true,1020,1020,1020,7,1026));
 assert(bank.status(1020).learned==0); // Retained input is not a sample.
 bank.forward(original,receiver,input[7]->scalar.component,.90f,0,1021);assert(received==.90f);
 bank.forward(original,receiver,input[7]->scalar.component,.60f,0,1022);assert(received==.60f);
 bank.forward(original,receiver,input[7]->scalar.component,.61f,0,1023);
 assert(bank.renew(profile,2,true,1026,1026,1026,7,1026));
 auto learned=bank.takeLearningResult();assert(learned&&learned->applied&&learned->samples==3&&learned->row==7);
 assert(bank.status(1026).learned==(1u<<7));
 bank.forward(original,receiver,input[7]->scalar.component,.62f,0,1027);assert(received==0);
 assert(bank.renew(profile,2,true,1027,1027,1027,7,1029));
 bank.forward(original,receiver,input[7]->scalar.component,.9f,0,1028);
 assert(bank.renew(profile,2,true,1029,1029,1029,7,1029));
 auto shortWindow=bank.takeLearningResult();assert(shortWindow&&!shortWindow->applied&&shortWindow->samples==1);
 bank.forward(original,receiver,input[7]->scalar.component,.62f,0,1029);assert(received==0);
 bank.forward(original,receiver,input[6]->scalar.component,.1f,0,1029);assert(received==0);
 bank.forward(original,receiver,0x300000001,.1f,0,1029);assert(received==.1f);
 bank.off();bank.forward(original,receiver,input[7]->scalar.component,.6f,0,1029);assert(received==.6f);
 profile.requested=false;assert(bank.renew(profile,3,true,1030,1030,1030));
 bank.forward(original,receiver,input[6]->scalar.component,.1f,0,1031);assert(received==.1f);
 profile.requested=true;assert(bank.renew(profile,4,true,1040,1040,1040));
 bank.forward(original,receiver,input[6]->scalar.component,.1f,0,1041);assert(received==0); // Explicit ON can be repeated without reconnect.
 bank.forward(original,receiver,input[6]->scalar.component,.1f,0,2000);assert(received==.1f); // Metadata expiry independent of control thread.
 assert(bank.renew(profile,4,true,3000,3000,3000));
 bank.forward(original,receiver,input[6]->scalar.component,.1f,0,3001);assert(received==.1f);
 bank.configure(input,3000);bank.forward(original,receiver,input[6]->scalar.component,.1f,0,3001);assert(received==0);
 input[6].reset();bank.configure(input,3010);bank.forward(original,receiver,0x20000000c,.1f,0,3011);assert(received==.1f);
 assert(!bank.renew(profile,4,true,3000,3000,2999)); // Regressed clock.
 bank.clear();assert(bank.status(3012).present==0);
 FingerBank continuous;auto live=bindings();frame_settings::Profile all;all.requested=true;for(auto& f:all.fingers)f={true,false,.2};
 continuous.configure(live,10000);assert(continuous.renew(all,1,true,10000,10000,10000));
 for(unsigned row=0;row<8;++row)continuous.forward(original,receiver,live[row]->scalar.component,.1f,0,10001);
 for(uint64_t now=10050;now<3610000;now+=50){
  continuous.configure(live,now);assert(continuous.renew(all,1,true,now,now,now));
  for(unsigned row=0;row<8;++row){continuous.forward(original,receiver,live[row]->scalar.component,.1f,0,now);assert(received==0);}
 }
 continuous.off();continuous.forward(original,receiver,live[0]->scalar.component,.1f,0,3610001);assert(received==.1f);
 FingerBank protectedButtons;live[0]->scalar.path="/input/trigger/value";protectedButtons.configure(live,1000);
 assert((protectedButtons.status(1001).present&1u)==0);
 FingerBank standalone;live=bindings();standalone.configure(live,1000);
 assert(!standalone.renew(all,1,true,1000,0,1000));
 assert(standalone.renewStandalone(all,1,true,1000,1000));
 standalone.forward(original,receiver,live[0]->scalar.component,.1f,0,1001);
 standalone.forward(original,receiver,live[0]->scalar.component,.1f,0,1002);assert(received==0);
 assert(!standalone.renewStandalone(all,1,false,1000,1002));
 assert(!standalone.renewStandalone(all,1,true,1000,2600));
 // Route observation may prime receivers, but must never map or learn.
 FingerBank rawOnly;live=bindings();rawOnly.configure(live,4000);
 frame_settings::Profile rawProfile;rawProfile.requested=true;
 for(auto& f:rawProfile.fingers)f={true,false,.2};
 assert(rawOnly.renewStandalone(rawProfile,1,true,4000,4000));
 rawOnly.forward(original,receiver,live[0]->scalar.component,.1f,0,4001,false);assert(received==.1f);
 rawOnly.forward(original,receiver,live[0]->scalar.component,.1f,0,4002,false);assert(received==.1f);
 assert(rawOnly.status(4002).changed==0);
 rawOnly.forward(original,receiver,live[0]->scalar.component,.1f,0,4003);assert(received==0);
 rawProfile.fingers[0].dynamic=true;
 assert(rawOnly.renewStandalone(rawProfile,2,true,4010,4010,0,4020));
 for(uint64_t now=4011;now<4015;++now){
  rawOnly.forward(original,receiver,live[0]->scalar.component,.6f,0,now,false);assert(received==.6f);
 }
 assert(rawOnly.renewStandalone(rawProfile,2,true,4020,4020,0,4020));
 auto noLearning=rawOnly.takeLearningResult();assert(noLearning&&!noLearning->applied&&noLearning->samples==0);
 std::puts("PASS standalone authority: no PC proof, runtime required, UI expiry enforced");
 std::puts("PASS simulated1h continuous all8 renewal; physical trigger/value excluded");
 std::puts("PASS all8 bank: both hands/all fingers, manual selection, explicit dynamic learning, unrelated bypass, repeat ON/OFF, metadata expiry, disconnect and clock regression");
}
