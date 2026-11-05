#pragma once
#include "supervised-scalar-publication.hpp"
#include "frame-ui-heartbeat.hpp"
#include <sstream>
namespace frame_supervision {
struct TrialIntent {std::string id;bool start=false;float threshold=0;};
inline bool parseSupervisedIntent(const std::string& text,int64_t wall,TrialIntent& out){
 if(text.size()>256)return false;
 std::istringstream input(text);std::string tag,id,action,extra;int64_t stamp=0;float threshold=0;
 if(!(input>>tag>>stamp>>id>>action>>threshold)||input>>extra||tag!="FRAME_VR_SUPERVISED_1"||
    stamp<0||stamp>wall||wall-stamp>2||id.size()!=16||!std::isfinite(threshold)||threshold<0||threshold>.9f)return false;
 for(char c:id)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
 if(action!="START"&&action!="STOP")return false;
 out={id,action=="START",threshold};return true;
}
// One explicit left-pinky trial per provider instance. No automatic re-arm.
// This class is not enabled in the read-only or existing 5-second build.
class SupervisedTrialController {
 SupervisedScalarPublication publication;
 bool used=false,active=false;
 uint64_t uiSession=0,session=0;
 std::string lastIntent,statusId,status="STOPPED";
public:
 bool ready=false;
 void stop(){publication.stop();active=false;ready=false;if(!statusId.empty()&&status!="ERROR")status="STOPPED";}
 const std::string& id()const{return statusId;}
 const std::string& state()const{return status;}
 bool running()const{return active;}
 uint64_t changed()const{return publication.changed();}
 void tick(const Identity& key,Evidence evidence,const UiHeartbeat& ui,bool uiValid,
           const std::string& request,int64_t wall,uint64_t now){
  evidence.heartbeatMs=ui.observedMs;evidence.heartbeatSequence=ui.sequence;
  if(active&&(!uiValid||ui.session!=uiSession||!publication.refresh(key,evidence,now))){stop();}
  ready=!used&&uiValid&&key.component&&key.receiver&&key.generation&&key.runtimeEpoch&&
      key.profileRevision==ui.revision&&!key.serial.empty()&&evidence.compatible&&evidence.connected&&
      evidence.identityVerified&&evidence.pcCorrectionOff&&evidence.metadataMs&&evidence.metadataMs<=now&&
      now-evidence.metadataMs<250&&evidence.pcProofUntilMs>now&&evidence.pcProofUntilMs-now<=250;
  TrialIntent intent;
  if(!parseSupervisedIntent(request,wall,intent)||intent.id==lastIntent)return;
  lastIntent=intent.id;
  if(!intent.start){stop();return;}
  statusId=intent.id;
  if(!ready){status="ERROR";stop();return;}
  used=true;ready=false;uiSession=ui.session;
  active=publication.start(true,++session,key,evidence,now,20,intent.threshold);
  status=active?"RUNNING":"ERROR";
 }
 int32_t forward(frame_scalar::ScalarUpdate original,void* receiver,uint64_t component,float value,double offset,uint64_t now){
  return publication.forward(original,receiver,component,value,offset,now);
 }
};
}
