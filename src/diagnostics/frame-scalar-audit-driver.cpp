#include <pwd.h>
#if defined(FRAME_NOV_BETA_CANDIDATE) && defined(FRAME_SCALAR_CV_CACHE_AUDIT)
#error November beta cv cache layout is unverified
#endif
#if defined(FRAME_NOV_BETA_READONLY) && (defined(FRAME_SCALAR_OFF_VTABLE_PROBE) || defined(FRAME_SCALAR_CV_CACHE_AUDIT) || defined(FRAME_SCALAR_CANDIDATE_SHADOW))
#error November beta profile is read-only
#endif
#include "frame-runtime-profile.hpp"
// Default build: read-only layout/stored-value audit. No device input writes.
// Experimental FRAME_SCALAR_OFF_VTABLE_PROBE build adds an explicitly requested
// OFF-only slot observer. Never deploy that build as the read-only diagnostic.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../../third_party/openvr-driver/openvr_driver.h"
#pragma GCC diagnostic pop
#include <cstdio>
#include "../correction/scalar-activity-label.hpp"
#include <cstring>
#include <cstdint>
#include <dlfcn.h>
#include <chrono>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctime>
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
#if !defined(FRAME_SCALAR_CONTINUOUS_SHADOW) || defined(FRAME_SCALAR_BOUNDED_ON_CANDIDATE)
#error Supervised trial requires supervision and excludes the old bounded trial
#endif
#endif
#include "self-memory-reader.hpp"
#include "../correction/scalar-component-snapshot.hpp"
#include "../correction/scalar-monitor-request.hpp"
#ifdef FRAME_SCALAR_CONTINUOUS_SHADOW
#if !defined(FRAME_SCALAR_CANDIDATE_SHADOW) || defined(FRAME_SCALAR_BOUNDED_ON_CANDIDATE)
#error Continuous shadow requires the OFF-only shadow build
#endif
#include "../correction/frame-monitor-shadow.hpp"
#include "../correction/pc-off-proof.hpp"
#endif
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
#if !defined(FRAME_SCALAR_OFF_VTABLE_PROBE) || !defined(FRAME_SCALAR_CANDIDATE_SHADOW)
#error Bounded ON requires the verified shadow callback build
#endif
#include "../correction/bounded-request.hpp"
#endif
#ifdef FRAME_SCALAR_CV_CACHE_AUDIT
#include "cv-input-snapshot.hpp"
#include <link.h>
#endif
#if defined(FRAME_SCALAR_CANDIDATE_SHADOW) && !defined(FRAME_SCALAR_OFF_VTABLE_PROBE)
#error Shadow candidate requires the explicit OFF-only probe build
#endif
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
#include "scalar-off-vtable-probe.hpp"
#endif

namespace frame_scalar_audit {
inline std::string statePath(const char* relative){
#ifdef FRAME_FINGER_RELEASE
    static const std::string root=[]{
        const auto* user=getpwuid(getuid());
        if(!user||!user->pw_dir||user->pw_dir[0]!='/')return std::string{};
        return std::string(user->pw_dir)+"/.local/share/frame-finger-release/state/";
    }();
    return root.empty()?std::string{}:root+relative;
#else
    return std::string("/home/steamos/.local/share/frame-controller-recovery/paths-off-20261023/")+relative;
#endif
}

class Provider final:public vr::IServerTrackedDeviceProvider {
    bool active=false;
    vr::IVRDriverLog* logger=nullptr;
    vr::IVRProperties* properties=nullptr;
    vr::IVRServerDriverHost* host=nullptr;
    uint64_t backend=0;
    unsigned attempts=0;
    std::chrono::steady_clock::time_point next{};
    std::chrono::steady_clock::time_point monitorUntil{};
    std::string requestId,lastRequestId;
#ifdef FRAME_SCALAR_CONTINUOUS_SHADOW
    frame_supervision::FrameMonitorShadow continuousShadow;
    bool continuousEligible=false,uiFresh=false,pcOffFresh=false;
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
    uint64_t betaLearnUntil=0,betaNextStatus=0;int betaLearnRow=-1;std::string betaLearnId;
    static std::string readBetaFile(const char* name,size_t limit){
        std::string path=statePath("settings-ui/");path+=name;
        int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return {};
        struct stat info{};std::string body;ssize_t count=-1;
        if(!fstat(fd,&info)&&S_ISREG(info.st_mode)&&info.st_uid==getuid()&&!(info.st_mode&0022)&&info.st_size>0&&static_cast<uint64_t>(info.st_size)<=limit){
            body.resize(static_cast<size_t>(info.st_size));count=read(fd,body.data(),body.size());
        }
        close(fd);return count>0&&static_cast<size_t>(count)==body.size()?body:std::string{};
    }
    static void writeBetaStatus(const std::string& content){
        std::string path=statePath("settings-ui/beta-status.txt");
        auto pattern=path+".tmp-XXXXXX";std::vector<char> tmp(pattern.begin(),pattern.end());tmp.push_back(0);
        int fd=mkstemp(tmp.data());if(fd<0)return;
        bool ok=write(fd,content.data(),content.size())==static_cast<ssize_t>(content.size());if(close(fd)!=0)ok=false;
        if(ok&&rename(tmp.data(),path.c_str())==0)return;
        unlink(tmp.data());
    }
    void evaluateBeta(const frame_supervision::UiHeartbeat& ui,[[maybe_unused]] const frame_supervision::PcOffProof& proof,uint64_t now){
        frame_settings::Profile profile;
        auto text=readBetaFile("beta-live-profile.txt",4096);
#ifdef FRAME_SCALAR_STANDALONE_BETA
        if(!uiFresh||!frame_settings::parse(text,profile))
#else
        if(!uiFresh||!pcOffFresh||!frame_settings::parse(text,profile))
#endif
        {frame_off_probe::allFingers.off();return;}
        auto learn=readBetaFile("beta-learn.txt",160);std::istringstream input(learn);
        std::string tag,id,extra;int64_t stamp=0;int row=-1;auto wall=time(nullptr);
        if((input>>tag>>stamp>>id>>row)&&!(input>>extra)&&tag=="FRAME_BETA_LEARN_1"&&stamp>=0&&stamp<=wall&&wall-stamp<=2&&id.size()==16&&row>=0&&row<8&&id!=betaLearnId){
            bool valid=true;for(char c:id)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))valid=false;
            if(valid){betaLearnId=id;betaLearnRow=row;betaLearnUntil=now+1500;}
        }
        const auto revision=(ui.session*uint64_t{1099511628211})^ui.revision;
#ifdef FRAME_SCALAR_STANDALONE_BETA
        frame_off_probe::allFingers.renewStandalone(profile,revision?revision:1,offProbe.active(),ui.observedMs,now,betaLearnRow,betaLearnUntil);
#else
        frame_off_probe::allFingers.renew(profile,revision?revision:1,offProbe.active(),ui.observedMs,proof.issuedMs,now,betaLearnRow,betaLearnUntil);
#endif
        offProbe.renewVerifiedObserver();
    }
    void publishBeta(uint64_t now){
        if(!offProbe.active())frame_off_probe::allFingers.off();
        if(now<betaNextStatus)return;
        betaNextStatus=now+250;
        auto state=frame_off_probe::allFingers.status(now);
        std::ostringstream text;text<<"FRAME_BETA_STATUS_1 "<<time(nullptr)<<' '<<(state.on?"ON":"OFF")<<' '<<state.present<<' '<<state.witnessed<<' '<<state.learned<<'\n';
        writeBetaStatus(text.str());
    }
#endif

#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
    uint64_t nextSupervisedStatus=0;
    static std::string readSupervisedIntent(){
        const auto path=statePath("settings-ui/supervised-intent.txt");
        int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return {};
        struct stat info{};char body[257]{};ssize_t size=-1;
        if(!fstat(fd,&info)&&S_ISREG(info.st_mode)&&info.st_uid==getuid()&&!(info.st_mode&0022)&&info.st_size>0&&info.st_size<=256)
            size=read(fd,body,static_cast<size_t>(info.st_size));
        close(fd);return size>0&&size==info.st_size?std::string(body,static_cast<size_t>(size)):std::string{};
    }
    static void writeSupervised(const char* name,const std::string& body){
        std::string path=statePath("settings-ui/");
        path+=name;auto pattern=path+".tmp-XXXXXX";std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
        int fd=mkstemp(temporary.data());if(fd<0)return;
        bool ok=write(fd,body.data(),body.size())==static_cast<ssize_t>(body.size());
        if(close(fd)!=0)ok=false;
        if(ok&&rename(temporary.data(),path.c_str())==0)return;
        unlink(temporary.data());
    }
    void publishSupervised(uint64_t now){
        auto& trial=frame_off_probe::shadow.supervised;
        if(!offProbe.active())trial.stop();
        if(now<nextSupervisedStatus)return;
        nextSupervisedStatus=now+250;
        auto stamp=std::to_string(time(nullptr));
        writeSupervised("supervised-broker.txt","FRAME_VR_BROKER_1 "+stamp+" "+(trial.ready?"READY":"BUSY")+"\n");
        if(!trial.id().empty())writeSupervised("supervised-status.txt","FRAME_VR_STATUS_1 "+stamp+" "+trial.id()+" "+trial.state()+"\n");
    }
#endif
    void evaluateContinuous(uint64_t now){
        const auto file=statePath("settings-ui/ui-heartbeat.txt");
        int fd=open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
        frame_supervision::UiHeartbeat ui;char text[161]{};struct stat info{};ssize_t count=-1;
        if(fd>=0){
            if(!fstat(fd,&info)&&S_ISREG(info.st_mode)&&info.st_uid==getuid()&&!(info.st_mode&0022)&&info.st_size>0&&info.st_size<=160)
                count=read(fd,text,static_cast<size_t>(info.st_size));
            close(fd);
        }
        // Sample AFTER reading: a writer may have published a newer timestamp
        // while collectShadow/file reads ran. A pre-read timestamp can falsely
        // label that legitimate notification as coming from the future.
        now=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        uiFresh=count>0&&count==info.st_size&&frame_supervision::parseHeartbeat(std::string(text,static_cast<size_t>(count)),now,ui
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
                ,frame_beta::uiLifetimeMs
#endif
            );
        frame_supervision::PcOffProof proof;
#ifdef FRAME_SCALAR_STANDALONE_BETA
        // Frame is explicitly the sole correction owner. No PC attestation is
        // manufactured, read or substituted with a local timestamp.
        evaluateBeta(ui,proof,now);return;
#endif
        int proofFd=open(statePath("settings-ui/pc-off-proof.txt").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
        char proofText[257]{};struct stat proofInfo{};ssize_t proofCount=-1;
        if(proofFd>=0){
            if(!fstat(proofFd,&proofInfo)&&S_ISREG(proofInfo.st_mode)&&proofInfo.st_uid==getuid()&&!(proofInfo.st_mode&0022)&&proofInfo.st_size>0&&proofInfo.st_size<=256)
                proofCount=read(proofFd,proofText,static_cast<size_t>(proofInfo.st_size));
            close(proofFd);
        }
        now=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        pcOffFresh=uiFresh&&proofCount>0&&proofCount==proofInfo.st_size&&
            frame_supervision::parsePcOffProof(std::string(proofText,static_cast<size_t>(proofCount)),ui.session,now,proof
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
                ,frame_beta::pcLifetimeMs
#endif
            );
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
        evaluateBeta(ui,proof,now);return;
#endif
        auto facts=frame_off_probe::shadow.monitoringFacts();
        if(!uiFresh){continuousShadow.stop();continuousEligible=false;
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
            frame_off_probe::shadow.supervised.stop();
#endif
            return;
        }
        frame_supervision::Identity key{facts.serial,facts.component,facts.receiver,facts.generation,1,ui.revision};
        frame_supervision::Evidence evidence{offProbe.active(),facts.verified,facts.verified,pcOffFresh,facts.observedMs,0,0,pcOffFresh?proof.issuedMs+250:0};
        continuousEligible=continuousShadow.evaluate(key,evidence,ui,now);
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
        auto intent=readSupervisedIntent();
        now=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        frame_off_probe::shadow.supervised.tick(key,evidence,ui,true,intent,time(nullptr),now);
#endif
    }
#endif
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
    std::string lastBoundedRequest;
    void pollBoundedRequest(uint64_t nowMs){
        auto deadline=frame_off_probe::shadow.correctionDeadline();
        if(deadline&&nowMs>=deadline){
            frame_off_probe::shadow.stopCorrection();
            logger->Log("FrameScalarBounded: lease ended; correction OFF; original values forwarded");
        }
        const auto file=statePath("scalar-audit/bounded-request");
        int fd=open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return;
        struct stat info{};char text[257]{};ssize_t count=-1;
        if(!fstat(fd,&info)&&S_ISREG(info.st_mode)&&info.st_uid==getuid()&&!(info.st_mode&0022)&&
           info.st_size>0&&info.st_size<=256)count=read(fd,text,static_cast<size_t>(info.st_size));
        close(fd);if(count<=0||count!=info.st_size)return;
        frame_scalar::BoundedRequest request;
        auto wall=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if(!frame_scalar::parseBoundedRequest(std::string(text,static_cast<size_t>(count)),wall,request)||request.id==lastBoundedRequest)return;
        lastBoundedRequest=request.id;
        if(!request.on){frame_off_probe::shadow.stopCorrection();logger->Log("FrameScalarBounded: explicit OFF received");return;}
        auto state=frame_off_probe::shadow.status();
        bool armed=offProbe.active()&&state.generation==request.generation&&
            frame_off_probe::shadow.armOnce(true,true,request.pcOffConfirmed,nowMs,request.seconds,request.deadzone);
        char message[240]{};
        std::snprintf(message,sizeof(message),"FrameScalarBounded: request=%s result=%s target=left_pinky seconds=%u deadzone=%.6g deadline_ms=%llu",
            request.id.c_str(),armed?"ARMED":"REFUSED",request.seconds,request.deadzone,
            static_cast<unsigned long long>(frame_off_probe::shadow.correctionDeadline()));
        logger->Log(message);
    }
#endif
#ifdef FRAME_SCALAR_CV_CACHE_AUDIT
    uintptr_t cvBase=0,ownInput=0;
    bool cvHashChecked=false;
    void auditCvInput(){
        if(!cvHashChecked){
            cvHashChecked=true;
            FILE* pipe=popen("/usr/bin/sha256sum /opt/steamvr/drivers/cv/bin/linuxarm64/driver_cv.so","r");
            if(!pipe){logger->Log("FrameCvInputAudit: hash check unavailable; inactive");return;}
            char line[512]{};bool known=fgets(line,sizeof(line),pipe)&&
                !std::strncmp(line,frame_compat::cvSha256,64)&&line[64]==' ';
            if(pclose(pipe)!=0||!known){logger->Log("FrameCvInputAudit: unknown cv hash; inactive");return;}
            dl_iterate_phdr([](dl_phdr_info* info,size_t,void* data){
                if(info->dlpi_name&&!std::strcmp(info->dlpi_name,"/opt/steamvr/drivers/cv/bin/linuxarm64/driver_cv.so")){
                    *static_cast<uintptr_t*>(data)=info->dlpi_addr;return 1;
                }return 0;
            },&cvBase);
        }
        if(!cvBase)return;
        SelfMemoryReader reader;
        auto sample=frame_cv_audit::snapshot(reader,cvBase);
        if(!sample.valid){logger->Log("FrameCvInputAudit: cache unavailable/unstable; read-only");return;}
        uint64_t table=0,update=0;
        if(!reader.read(ownInput,&table,8)||!table||table>UINT64_MAX-24||!reader.read(table+24,&update,8))return;
        Dl_info owner{};bool resolved=dladdr(reinterpret_cast<void*>(sample.update),&owner)&&owner.dli_fbase&&owner.dli_fname;
        bool server=resolved&&!std::strcmp(owner.dli_fname,"/opt/steamvr/bin/linuxarm64/vrserver");
        auto rva=resolved?sample.update-reinterpret_cast<uintptr_t>(owner.dli_fbase):0;
        char text[256]{};
        std::snprintf(text,sizeof(text),"FrameCvInputAudit: stable=1 same_object=%d same_table=%d same_update=%d update_in_vrserver=%d update_rva=0x%llx " FRAME_SCALAR_ACTIVITY " no_calls",
            sample.object==ownInput,sample.table==table,sample.update==update,server,static_cast<unsigned long long>(rva));
        logger->Log(text);
    }
#endif
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
    frame_off_probe::Probe offProbe;
    void* qualifiedInput=nullptr;
    std::string lastOffRequest;
    void pollOffRequest(){
        const auto file=statePath("scalar-audit/off-vtable-request");
        int fd=open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return;
        struct stat info{};char text[161]{};ssize_t count=-1;
        if(!fstat(fd,&info)&&S_ISREG(info.st_mode)&&info.st_uid==getuid()&&!(info.st_mode&0022)&&
           info.st_size>0&&info.st_size<=160)count=read(fd,text,static_cast<size_t>(info.st_size));
        close(fd);if(count<=0||count!=info.st_size)return;
        std::string body(text,static_cast<size_t>(count));
        const std::string format="FRAME_SCALAR_OFF_PROBE_1";
        if(body.compare(0,format.size(),format))return;
        body.replace(0,format.size(),"FRAME_SCALAR_MONITOR_1");
        auto wall=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        frame_scalar::MonitorRequest request;
        if(!frame_scalar::parseMonitorRequest(body,wall,request)||request.seconds>frame_off_probe::maximumObserverSeconds||request.id==lastOffRequest)return;
        lastOffRequest=request.id;
        if(request.seconds==0)offProbe.stop();
        else offProbe.begin(qualifiedInput,request.seconds,[this](const char* message){logger->Log(message);});
    }
#endif
    void pollRequest(std::chrono::steady_clock::time_point now){
        const auto file=statePath("scalar-audit/monitor-request");
        int fd=open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
        if(fd<0)return;
        struct stat info{};char text[161]{};ssize_t count=-1;
        if(!fstat(fd,&info)&&S_ISREG(info.st_mode)&&info.st_uid==getuid()&&!(info.st_mode&0022)&&
           info.st_size>0&&info.st_size<=160)count=read(fd,text,static_cast<size_t>(info.st_size));
        close(fd);
        if(count<=0||count!=info.st_size)return;
        auto wall=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        frame_scalar::MonitorRequest request;
        if(!frame_scalar::parseMonitorRequest(std::string(text,static_cast<size_t>(count)),wall,request)||request.id==lastRequestId)return;
        lastRequestId=request.id;requestId=request.id;monitorUntil=now+std::chrono::seconds(request.seconds);
        char message[128]{};
        std::snprintf(message,sizeof(message),"FrameScalarAudit: monitor_request id=%s seconds=%u; read-only stored samples",requestId.c_str(),request.seconds);
        logger->Log(message);
    }
    struct Owner {std::string serial;int32_t role=0;};
    Owner owner(uint64_t container){
        char serial[128]{},type[128]{};int32_t role=0;
        vr::PropertyRead_t entries[3]{};
        entries[0].prop=vr::Prop_SerialNumber_String;entries[0].pvBuffer=serial;entries[0].unBufferSize=sizeof(serial);
        entries[1].prop=vr::Prop_ControllerType_String;entries[1].pvBuffer=type;entries[1].unBufferSize=sizeof(type);
        entries[2].prop=vr::Prop_ControllerRoleHint_Int32;entries[2].pvBuffer=&role;entries[2].unBufferSize=sizeof(role);
        if(properties->ReadPropertyBatch(container,entries,3)!=vr::TrackedProp_Success)return {};
        for(unsigned i=0;i<3;++i){
            auto& entry=entries[i];
            if(entry.eError!=vr::TrackedProp_Success||!entry.unRequiredBufferSize||entry.unRequiredBufferSize>entry.unBufferSize)return {};
            if(i<2){
                auto* text=static_cast<char*>(entry.pvBuffer);
                if(entry.unTag!=vr::k_unStringPropertyTag||text[entry.unRequiredBufferSize-1]||
                   std::memchr(text,0,entry.unRequiredBufferSize)!=text+entry.unRequiredBufferSize-1)return {};
                for(unsigned j=0;j+1<entry.unRequiredBufferSize;++j)if(text[j]<32||text[j]>126)return {};
            }else if(entry.unTag!=vr::k_unInt32PropertyTag||entry.unRequiredBufferSize!=sizeof(role))return {};
        }
        if(!serial[0]||std::strcmp(type,"frame_controller")||
           (role!=vr::TrackedControllerRole_LeftHand&&role!=vr::TrackedControllerRole_RightHand))return {};
        return {serial,role};
    }
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
    std::chrono::steady_clock::time_point nextShadow{};
    uint64_t shadowTicks=0,shadowMaxUs=0;
    void collectShadow(std::chrono::steady_clock::time_point now){
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
        std::array<std::optional<frame_beta::Binding>,8> bindings;std::array<unsigned,8> counts{};
        if(!host){frame_off_probe::allFingers.clear();return;}
        vr::TrackedDevicePose_t beforePoses[64]{},afterPoses[64]{};host->GetRawTrackedDevicePoses(0,beforePoses,64);
        for(uint32_t index=0;index<64;++index){
            if(!beforePoses[index].bDeviceIsConnected)continue;
            auto container=properties->TrackedDeviceToPropertyContainer(index);auto before=owner(container);
            if(before.serial.empty())continue;
            SelfMemoryReader reader;auto snapshot=frame_scalar::stableSnapshot(reader,backend,index);auto after=owner(container);
            if(!snapshot.valid||before.serial!=after.serial||before.role!=after.role)continue;
            const unsigned side=after.role==vr::TrackedControllerRole_LeftHand?0:1;
            auto hand=side?frame_scalar::Hand::Right:frame_scalar::Hand::Left;
            for(const auto& component:snapshot.components){
                int finger=frame_beta::fingerForPath(component.path);if(finger<0||component.units!=0)continue;
                const unsigned row=side*4+static_cast<unsigned>(finger);++counts[row];
                frame_scalar::ExistingScalar scalar{component.handle,container,component.path,after.serial,hand,true,true,0,false};
                frame_scalar::DeviceSnapshot device{index,after.serial,"frame_controller",hand,true,0};
                bindings[row]=frame_beta::Binding{std::move(scalar),std::move(device),component.nodeAddress};
            }
        }
        host->GetRawTrackedDevicePoses(0,afterPoses,64);
        for(unsigned row=0;row<8;++row)if(counts[row]!=1||!bindings[row]||!afterPoses[bindings[row]->device.index].bDeviceIsConnected)bindings[row].reset();
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        frame_off_probe::allFingers.configure(bindings,static_cast<uint64_t>(ms));++shadowTicks;
#else

        if(!host){frame_off_probe::shadow.clear();return;}
        vr::TrackedDevicePose_t beforePoses[64]{},afterPoses[64]{};
        host->GetRawTrackedDevicePoses(0,beforePoses,64);
        frame_scalar::ExistingScalar binding;
        frame_scalar::DeviceSnapshot device;
        uint64_t node=0;unsigned found=0;
        for(uint32_t index=0;index<64;++index){
            if(!beforePoses[index].bDeviceIsConnected)continue;
            auto container=properties->TrackedDeviceToPropertyContainer(index);
            auto before=owner(container);
            if(before.serial.empty()||before.role!=vr::TrackedControllerRole_LeftHand)continue;
            SelfMemoryReader reader;
            auto snapshot=frame_scalar::stableSnapshot(reader,backend,index);
            auto after=owner(container);
            if(!snapshot.valid||before.serial!=after.serial||before.role!=after.role)continue;
            for(const auto& component:snapshot.components){
                if(component.path!="/input/capsense/pinky"||component.units!=0)continue;
                ++found;node=component.nodeAddress;
                binding={component.handle,container,component.path,after.serial,frame_scalar::Hand::Left,true,true,0,false};
                device={index,after.serial,"frame_controller",frame_scalar::Hand::Left,true,0};
            }
        }
        host->GetRawTrackedDevicePoses(0,afterPoses,64);
        // Timestamp at collection start: a slow scan cannot look artificially fresh.
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        if(found!=1||!afterPoses[device.index].bDeviceIsConnected)
            frame_off_probe::shadow.clear();
        else frame_off_probe::shadow.configure(std::move(binding),std::move(device),node,static_cast<uint64_t>(ms));
        ++shadowTicks;
        auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-now).count();
        if(elapsed>0)shadowMaxUs=std::max(shadowMaxUs,static_cast<uint64_t>(elapsed));
#endif
    }
#endif
    void collect(bool monitoring){
        unsigned matched=0;
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
        std::vector<uint64_t> targets;
#endif
        vr::TrackedDevicePose_t poses[64]{};
        if(monitoring&&host)host->GetRawTrackedDevicePoses(0,poses,64);
        for(uint32_t device=0;device<64;++device){
            auto container=properties->TrackedDeviceToPropertyContainer(device);
            auto index=frame_scalar::deviceFromContainer(container);
            if(!index||*index!=device)continue;
            auto before=owner(container);if(before.serial.empty())continue;
            SelfMemoryReader reader;
            auto snapshot=frame_scalar::stableSnapshot(reader,backend,device);
            std::vector<std::pair<bool,float>> values;
            if(monitoring&&snapshot.valid)for(const auto& component:snapshot.components){
                float value=0;bool valid=frame_scalar::storedScalar(reader,component,value);
                values.emplace_back(valid,value);
            }
            auto after=owner(container);
            if(!snapshot.valid||before.serial!=after.serial||before.role!=after.role)continue;
            unsigned position=0;
            for(const auto& component:snapshot.components){
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
                targets.push_back(component.handle);
#endif
                char text[512]{};
                if(monitoring){
                    bool connected=host&&poses[device].bDeviceIsConnected;
                    bool valid=connected&&values[position].first;
                    char scalar[32]="null";
                    if(valid)std::snprintf(scalar,sizeof(scalar),"%.9g",values[position].second);
                    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    std::snprintf(text,sizeof(text),"FrameScalarAudit: stored_sample unix_ms=%lld id=%s serial=%s role=%s device=%u component=0x%llx path=%s connected=%d pose_valid=%d valid=%d value=%s " FRAME_SCALAR_ACTIVITY "",
                        static_cast<long long>(ms),requestId.c_str(),after.serial.c_str(),
                        after.role==vr::TrackedControllerRole_LeftHand?"left":"right",device,
                        static_cast<unsigned long long>(component.handle),component.path.c_str(),connected,
                        host&&poses[device].bPoseIsValid,valid,scalar);
                    logger->Log(text);++position;++matched;continue;
                }
                std::snprintf(text,sizeof(text),"FrameScalarAudit: metadata_only serial=%s role=%s device=%u component=0x%llx path=%s units=%u " FRAME_SCALAR_ACTIVITY "",
                    after.serial.c_str(),after.role==vr::TrackedControllerRole_LeftHand?"left":"right",device,
                    static_cast<unsigned long long>(component.handle),component.path.c_str(),component.units);
                logger->Log(text);++matched;
            }
        }
        char text[128]{};
        std::snprintf(text,sizeof(text),"FrameScalarAudit: %s attempt=%u matched=%u; no input updates or hooks",monitoring?"monitor_tick":"snapshot",attempts,matched);
        logger->Log(text);
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
        offProbe.targets(targets);
#endif
        if(!monitoring&&matched>=4)attempts=12;
    }
public:
    vr::EVRInitError Init(vr::IVRDriverContext* context)override{
        if(!context||active)return vr::VRInitError_Driver_Failed;
        active=true;
        vr::EVRInitError error=vr::VRInitError_None;
        auto* log=static_cast<vr::IVRDriverLog*>(context->GetGenericInterface(vr::IVRDriverLog_Version,&error));
        if(error!=vr::VRInitError_None)log=nullptr;
        auto emit=[&](const char* text){if(log)log->Log(text);};
        emit("FrameScalarAudit: " FRAME_SCALAR_ACTIVITY "; read-only layout audit; no hooks");
        error=vr::VRInitError_None;
        void* input=context->GetGenericInterface("IVRDriverInput_005",&error);
        if(!input||error!=vr::VRInitError_None){
            emit("FrameScalarAudit: 005 unavailable; audit inactive");return vr::VRInitError_None;
        }
        // Do not cast the private 005 object to the public 004 C++ interface.
        // The runtime returned this live object to this provider. Read its vptr
        // and first four function pointers only; never call these functions.
        void** table=nullptr;
        std::memcpy(&table,input,sizeof(table));
        if(!table){emit("FrameScalarAudit: null table; audit inactive");return vr::VRInitError_None;}
#ifdef FRAME_NOV_BETA_READONLY
        if(std::system("/usr/bin/sha256sum /proc/$PPID/exe /opt/steamvr/bin/linuxarm64/vrserver "
                       "/opt/steamvr/drivers/cv/bin/linuxarm64/driver_cv.so | "
                       "/usr/bin/awk 'NR<=2 && $1!=\"430c259a8e47ef94824462d98dd0d5bc78d0d1b0ed2f0e2c50e9562982fe06ad\" {bad=1} "
                       "NR==3 && $1!=\"e54b20b9aba50b63b80a37e6b7d7b61454808dedd3f967d8c81f0c9460d18b7b\" {bad=1} "
                       "END {exit (bad || NR!=3)}'")!=0){
            emit("FrameScalarAudit: November beta hash mismatch; read-only audit inactive");
            return vr::VRInitError_None;
        }
        constexpr uintptr_t expected[]{0x779b8,0x779a0,0x77970,0x77960};
#else
        constexpr auto& expected=frame_compat::inputEntries;
#endif
        bool match=true;
        for(unsigned slot=0;slot<4;++slot){
            void* function=nullptr;std::memcpy(&function,table+slot,sizeof(function));
            Dl_info owner{};
            if(!function||!dladdr(function,&owner)||!owner.dli_fname||!owner.dli_fbase||
               std::strcmp(owner.dli_fname,"/opt/steamvr/bin/linuxarm64/vrserver")){
                match=false;break;
            }
            auto rva=reinterpret_cast<uintptr_t>(function)-reinterpret_cast<uintptr_t>(owner.dli_fbase);
            char text[128]{};
            std::snprintf(text,sizeof(text),"FrameScalarAudit: 005 slot=%u rva=0x%lx known_layout=%d",
                          slot,static_cast<unsigned long>(rva),rva==expected[slot]);
            emit(text);if(rva!=expected[slot])match=false;
        }
        // Matching offsets alone are not a runtime compatibility authorization.
        emit(match?"FrameScalarAudit: candidate layout matches; correction remains OFF":
                   "FrameScalarAudit: unknown layout/owner; audit inactive; correction OFF");
        if(match&&log){
            // Verify the exact adapter layout before following its backend field.
#ifdef FRAME_NOV_BETA_READONLY
            constexpr unsigned char code[]{0xe2,0x03,0x01,0xaa,0x01,0x08,0x40,0xb9,0x00,0x08,0x40,0xf9,0x87,0x9f,0x00,0x14};
#else
            const auto& code=frame_compat::scalarFingerprint;
#endif
            SelfMemoryReader reader;unsigned char actual[sizeof(code)]{};
            uint64_t object=reinterpret_cast<uint64_t>(input),candidate=0;
            void* update=nullptr;std::memcpy(&update,table+3,sizeof(update));
            if(reader.read(reinterpret_cast<uint64_t>(update),actual,sizeof(actual))&&
               !std::memcmp(actual,code,sizeof(code))&&reader.read(object+16,&candidate,8)&&candidate){
                error=vr::VRInitError_None;
                auto* props=static_cast<vr::IVRProperties*>(context->GetGenericInterface(vr::IVRProperties_Version,&error));
                if(props&&error==vr::VRInitError_None){
                    logger=log;properties=props;backend=candidate;attempts=0;next={};
#ifdef FRAME_SCALAR_CV_CACHE_AUDIT
                    ownInput=reinterpret_cast<uintptr_t>(input);cvBase=0;cvHashChecked=false;
                    emit("FrameCvInputAudit: read-only cache comparison enabled; no hooks or input method calls");
#endif
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
                    qualifiedInput=input;
                    emit("FrameScalarOffProbe: capable but DISARMED; explicit fresh OFF-only request required");
#ifdef FRAME_SCALAR_CONTINUOUS_SHADOW
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
#ifdef FRAME_SCALAR_STANDALONE_BETA
                    char betaMarker[160]{};std::snprintf(betaMarker,sizeof(betaMarker),"FrameFingerBeta: build=standalone_v1 pid=%ld; authority=Frame-only; UI required; no PC helper",static_cast<long>(getpid()));emit(betaMarker);
#else
                    char betaMarker[160]{};std::snprintf(betaMarker,sizeof(betaMarker),"FrameFingerBeta: build=all8_v1 pid=%ld; software-only; OFF until fresh UI and PC proof",static_cast<long>(getpid()));emit(betaMarker);
#endif
#elif defined(FRAME_SCALAR_SUPERVISED_ON_CANDIDATE)
                    emit("FrameScalarSupervised: DISARMED; fresh VR START required; LEFT PINKY only; max20s; one trial per provider; no firmware commands");
#else
                    emit("FrameContinuousShadow: diagnostic loaded; no correction ON API; explicit OFF probe required");
#endif
#endif
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
                    emit("FrameScalarBounded: capable but DISARMED; explicit generation-bound left-pinky request required; max 5 seconds");
#endif
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
                    emit("FrameScalarShadow: OFF-only candidate loaded; 50ms identity checks run only during an explicit OFF probe");
#endif
#endif
                    error=vr::VRInitError_None;
                    host=static_cast<vr::IVRServerDriverHost*>(context->GetGenericInterface(vr::IVRServerDriverHost_Version,&error));
                    if(error!=vr::VRInitError_None)host=nullptr;
                    emit("FrameScalarAudit: bounded self-memory metadata snapshots armed; no writes or hooks");
                }
            }
        }
        return vr::VRInitError_None;
    }
    void Cleanup()override{
#ifdef FRAME_SCALAR_CONTINUOUS_SHADOW
        continuousShadow.stop();continuousEligible=false;uiFresh=false;
#endif
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
        offProbe.stop();qualifiedInput=nullptr;
#endif
        active=false;backend=0;properties=nullptr;logger=nullptr;host=nullptr;monitorUntil={};
    }
    const char* const* GetInterfaceVersions()override{return vr::k_InterfaceVersions;}
    void RunFrame()override{
        auto now=std::chrono::steady_clock::now();
        if(!active||!backend||!properties||!logger)return;
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
        try{
            if(offProbe.active()&&now>=nextShadow){
                nextShadow=now+std::chrono::milliseconds(50);collectShadow(now);
#ifdef FRAME_SCALAR_CONTINUOUS_SHADOW
                evaluateContinuous(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
#endif
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
                auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                pollBoundedRequest(static_cast<uint64_t>(ms));
#endif
            }
        }catch(...){frame_off_probe::shadow.clear();offProbe.stop();}
#endif
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
        publishSupervised(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()));
#endif
#if defined(FRAME_SCALAR_ALL_FINGERS_BETA) && !defined(FRAME_NOV_BETA_READONLY)
        publishBeta(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()));
#endif
        if(now<next)return;
        next=now+std::chrono::seconds(1);
        try{
#ifdef FRAME_SCALAR_CV_CACHE_AUDIT
            if(attempts<12||now<monitorUntil)auditCvInput();
#endif
            pollRequest(now);
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
            pollOffRequest();
            if(offProbe.active()){
                offProbe.tick();
#ifndef FRAME_SCALAR_STANDALONE_BETA
#ifdef FRAME_SCALAR_CANDIDATE_SHADOW
                auto state=frame_off_probe::shadow.status();char message[384]{};
                std::snprintf(message,sizeof(message),"FrameScalarShadow: generation=%llu receiver_observed=%d ambiguous=%d ticks=%llu max_scan_us=%llu attempts=%llu successes=%llu failures=%llu last_result=%d target=0x%llx " FRAME_SCALAR_ACTIVITY "",
                    static_cast<unsigned long long>(state.generation),state.receiverObserved,state.ambiguous,
                    static_cast<unsigned long long>(shadowTicks),static_cast<unsigned long long>(shadowMaxUs),
                    static_cast<unsigned long long>(state.attempts),static_cast<unsigned long long>(state.successes),
                    static_cast<unsigned long long>(state.failures),state.lastResult,static_cast<unsigned long long>(state.component));
                logger->Log(message);
#ifdef FRAME_SCALAR_CONTINUOUS_SHADOW
                char supervision[192]{};
                std::snprintf(supervision,sizeof(supervision),"FrameContinuousShadow: ui_fresh=%d eligible=%d pc_off_fresh=%d reason=%u " FRAME_SCALAR_ACTIVITY "",
                    uiFresh,continuousEligible,pcOffFresh,static_cast<unsigned>(continuousShadow.reason()));
                logger->Log(supervision);
#ifdef FRAME_SCALAR_ALL_FINGERS_BETA
                auto betaState=frame_off_probe::allFingers.status(static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()));
                char betaLog[240]{};std::snprintf(betaLog,sizeof(betaLog),"FrameFingerBeta: on=%d present=%u witnessed=%u learned=%u changed_calls=%llu",betaState.on,betaState.present,betaState.witnessed,betaState.learned,static_cast<unsigned long long>(betaState.changed));logger->Log(betaLog);
#endif
#ifdef FRAME_SCALAR_SUPERVISED_ON_CANDIDATE
                char status[240]{};auto& trial=frame_off_probe::shadow.supervised;
                std::snprintf(status,sizeof(status),"FrameScalarSupervised: state=%s ready=%d running=%d changed_calls=%llu",trial.state().c_str(),trial.ready,trial.running(),static_cast<unsigned long long>(trial.changed()));
                logger->Log(status);
#endif
#endif
#ifdef FRAME_SCALAR_BOUNDED_ON_CANDIDATE
                frame_scalar::ScalarTrace trace;uint64_t changed=0,dropped=0;
                if(frame_off_probe::shadow.trace(trace,changed,dropped)){
                    char sample[320]{};
                    std::snprintf(sample,sizeof(sample),"FrameScalarBounded: sample_ms=%llu deadline_ms=%llu input=%.9g output=%.9g changed=%d changed_calls=%llu trace_dropped=%llu",
                        static_cast<unsigned long long>(trace.nowMs),static_cast<unsigned long long>(trace.deadlineMs),
                        trace.input,trace.output,trace.mapped,static_cast<unsigned long long>(changed),static_cast<unsigned long long>(dropped));
                    logger->Log(sample);
                }
#endif
#endif
                if(offProbe.active())collect(false);
#endif
                return;
            }
#endif
            if(now<monitorUntil)collect(true);
            else if(attempts<12){++attempts;collect(false);}
        }catch(...){
#ifdef FRAME_SCALAR_OFF_VTABLE_PROBE
            offProbe.stop();
#endif
            attempts=12;monitorUntil={};logger->Log("FrameScalarAudit: snapshot stopped after audit failure; correction OFF");
        }
    }
    bool ShouldBlockStandbyMode()override{return false;}
    void EnterStandby()override{}
    void LeaveStandby()override{}
};
}
extern "C" __attribute__((visibility("default"))) void* HmdDriverFactory(const char* name,int* error){
    static frame_scalar_audit::Provider provider;
    if(name&&std::strcmp(name,vr::IServerTrackedDeviceProvider_Version)==0){
        if(error)*error=vr::VRInitError_None;
        return &provider;
    }
    if(error)*error=vr::VRInitError_Init_InterfaceNotFound;
    return nullptr;
}
