#pragma once
#include "frame-beta-paths.hpp"
#include "frame-settings-profile.hpp"
#include "scalar-component-identity.hpp"
#include "scalar-off-forwarding.hpp"
#include "finger-deadzone.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
namespace frame_beta {
struct Binding {frame_scalar::ExistingScalar scalar;frame_scalar::DeviceSnapshot device;uint64_t node=0;};
enum class Authority {VerifiedPcOff,FrameOnly};
class FingerBank {
 struct Record {
  Binding binding;
  uint64_t generation=0;
  std::atomic<bool> enabled{true},ambiguous{false};
  std::atomic<uint64_t> receiver{0},metadataUntil{0},successes{0},failures{0};
  std::mutex samples;
  frame_correction::FingerDeadzone zone;
  uint64_t settingsRevision=0,lastSample=0,lastLearnUntil=0;
  float lastRaw=0;bool hasRaw=false;
 };
 using Records=std::array<std::shared_ptr<Record>,8>;
 struct Control {
  frame_settings::Profile profile;
  bool on=false;uint64_t issued=0,until=0,revision=0;
  int learn=-1;uint64_t learnUntil=0;
 };
 std::shared_ptr<const Records> records;
 std::shared_ptr<const Control> control;
 uint64_t epoch=0,lastConfigure=0,lastRenew=0;
 std::atomic<uint64_t> changedCalls{0};
 static bool valid(const Binding& b,unsigned row){
  auto a=frame_scalar::deviceFromContainer(b.scalar.container),c=frame_scalar::deviceFromComponent(b.scalar.component);
  auto hand=row<4?frame_scalar::Hand::Left:frame_scalar::Hand::Right;
  return a&&c&&*a==*c&&*a==b.device.index&&b.node&&b.device.connected&&b.device.type=="frame_controller"&&
   !b.device.serial.empty()&&b.scalar.serial==b.device.serial&&b.scalar.hand==hand&&b.device.hand==hand&&
   b.scalar.path==fingerPaths[row%4]&&b.scalar.metadataVerified&&b.scalar.normalizedOneSided;
 }
 static bool same(const Binding& a,const Binding& b){return a.node==b.node&&a.scalar.component==b.scalar.component&&
  a.scalar.container==b.scalar.container&&a.device.serial==b.device.serial&&a.device.hand==b.device.hand&&a.scalar.path==b.scalar.path;}
public:
 void clear(){
  auto old=std::atomic_load(&records);if(old)for(auto& r:*old)if(r)r->enabled.store(false);
  std::atomic_store(&records,std::shared_ptr<const Records>{});off();
 }
 void off(){std::atomic_store(&control,std::shared_ptr<const Control>{});}
 void configure(const std::array<std::optional<Binding>,8>& bindings,uint64_t now){
  if(now<lastConfigure){clear();return;}lastConfigure=now;
  auto old=std::atomic_load(&records);auto next=std::make_shared<Records>();
  for(unsigned i=0;i<8;++i){
   auto previous=old?(*old)[i]:nullptr;
   if(bindings[i]&&valid(*bindings[i],i)&&now&&now<=UINT64_MAX-metadataLifetimeMs){
    if(previous&&same(previous->binding,*bindings[i]))(*next)[i]=previous;
    else if(epoch<UINT64_MAX){auto r=std::make_shared<Record>();r->binding=*bindings[i];r->generation=++epoch;(*next)[i]=r;}
    if((*next)[i])(*next)[i]->metadataUntil.store(now+metadataLifetimeMs);
   }
   if(previous&&previous!=(*next)[i])previous->enabled.store(false);
  }
  std::shared_ptr<const Records> immutable=next;std::atomic_store(&records,std::move(immutable));
 }
 // Control-thread only. All timestamps are from the same Frame monotonic clock.
 bool renew(const frame_settings::Profile& profile,uint64_t revision,bool verifiedRuntime,
            uint64_t uiStamp,uint64_t pcStamp,uint64_t now,int learning=-1,uint64_t learnUntil=0,Authority authority=Authority::VerifiedPcOff){
  if(now<lastRenew){off();return false;}lastRenew=now;
  if(!verifiedRuntime||!revision||!now||!uiStamp||uiStamp>now||now-uiStamp>=uiLifetimeMs||
     (authority==Authority::VerifiedPcOff&&(!pcStamp||pcStamp>now||now-pcStamp>=pcLifetimeMs))||now>UINT64_MAX-2000||
     frame_settings::serialize(profile).empty()||learning < -1||learning>=8){off();return false;}
  auto next=std::make_shared<Control>();next->profile=profile;next->on=profile.requested;
  next->issued=now;next->until=authority==Authority::FrameOnly?uiStamp+uiLifetimeMs:std::min(uiStamp+uiLifetimeMs,pcStamp+pcLifetimeMs);next->revision=revision;
  next->learn=learning;next->learnUntil=learnUntil;
  // cv publishes only when a value changes. On an explicit LEARN request,
  // seed from the retained PRE-correction value of the same live record, so
  // holding an extended finger still can learn without manufacturing an update.
  auto set=std::atomic_load(&records);
  if(next->on&&learning>=0&&learnUntil>now&&set){
   auto r=(*set)[static_cast<unsigned>(learning)];
   if(r&&r->enabled.load()&&!r->ambiguous.load()&&r->receiver.load()&&now<r->metadataUntil.load()){
    std::unique_lock<std::mutex> lock(r->samples,std::try_to_lock);
    if(lock.owns_lock()&&r->hasRaw&&r->lastLearnUntil!=learnUntil){
     const auto& finger=profile.fingers[static_cast<unsigned>(learning)];
     if(r->settingsRevision!=revision){r->zone.reset();r->settingsRevision=revision;}
     frame_correction::DeadzoneSettings settings;settings.enabled=finger.selected;settings.manual=finger.threshold;
     settings.mode=finger.dynamic?frame_correction::DeadzoneMode::Dynamic:frame_correction::DeadzoneMode::Manual;
     r->zone.observe(r->lastRaw,.05,true,false,settings);r->lastLearnUntil=learnUntil;
    }
   }
  }
  std::shared_ptr<const Control> immutable=next;std::atomic_store(&control,std::move(immutable));return true;
 }
 bool renewStandalone(const frame_settings::Profile& profile,uint64_t revision,bool verifiedRuntime,
                      uint64_t uiStamp,uint64_t now,int learning=-1,uint64_t learnUntil=0){
  return renew(profile,revision,verifiedRuntime,uiStamp,0,now,learning,learnUntil,Authority::FrameOnly);
 }
 struct Status {unsigned present=0,witnessed=0,learned=0;bool on=false;uint64_t changed=0;};
 Status status(uint64_t now){
  Status out;out.changed=changedCalls.load();auto c=std::atomic_load(&control);
  out.on=c&&c->on&&now>=c->issued&&now<c->until;
  auto set=std::atomic_load(&records);if(!set)return out;
  for(unsigned i=0;i<8;++i){auto r=(*set)[i];if(!r||!r->enabled.load()||now>=r->metadataUntil.load())continue;
   out.present|=1u<<i;if(r->receiver.load()&&!r->ambiguous.load())out.witnessed|=1u<<i;
   std::unique_lock<std::mutex> lock(r->samples,std::try_to_lock);
   if(lock.owns_lock()&&c&&r->settingsRevision==c->revision&&r->zone.ready())out.learned|=1u<<i;
  }return out;
 }
 int32_t forward(frame_scalar::ScalarUpdate original,void* receiver,uint64_t component,float input,double offset,uint64_t now){
  auto set=std::atomic_load(&records);auto c=std::atomic_load(&control);
  std::shared_ptr<Record> record;unsigned row=0;
  if(set)for(unsigned i=0;i<8;++i)if((*set)[i]&&(*set)[i]->binding.scalar.component==component){record=(*set)[i];row=i;break;}
  if(!record||!record->enabled.load()||record->ambiguous.load()||!receiver||!std::isfinite(input)||input<0||input>1)
   return original(receiver,component,input,offset);
  const auto address=reinterpret_cast<uintptr_t>(receiver);
  const auto witnessed=record->receiver.load();float output=input;
  std::unique_lock<std::mutex> lock(record->samples,std::try_to_lock);
  if(lock.owns_lock()){
   record->lastRaw=input;record->hasRaw=true;
   if(c&&record->settingsRevision!=c->revision){record->zone.reset();record->settingsRevision=c->revision;}
   if(c&&c->on&&now>=c->issued&&now<c->until&&now<record->metadataUntil.load()&&witnessed==address){
    const auto& finger=c->profile.fingers[row];
    frame_correction::DeadzoneSettings settings;settings.enabled=finger.selected;settings.manual=finger.threshold;
    settings.mode=finger.dynamic?frame_correction::DeadzoneMode::Dynamic:frame_correction::DeadzoneMode::Manual;
    double dt=record->lastSample&&now>record->lastSample?double(now-record->lastSample)/1000.:.01;
    if(c->learn==static_cast<int>(row)&&now<c->learnUntil)record->zone.observe(input,dt,true,false,settings);
    output=static_cast<float>(record->zone.map(input,settings));
   }
   record->lastSample=now;
  }
  // A replaced/disconnected record is invalidated before its successor publishes.
  if(!record->enabled.load())output=input;
  if(output!=input)changedCalls.fetch_add(1,std::memory_order_relaxed);
  const auto result=original(receiver,component,output,offset);
  if(result==0&&record->enabled.load()){
   record->successes.fetch_add(1,std::memory_order_relaxed);uint64_t expected=0;
   if(!record->receiver.compare_exchange_strong(expected,address)&&expected!=address)record->ambiguous.store(true);
  }else if(result!=0){record->failures.fetch_add(1,std::memory_order_relaxed);record->enabled.store(false);}
  return result;
 }
};
}
