#include <openvr.h>
#include "../correction/frame-settings-profile.hpp"
#include "../correction/frame-settings-store.hpp"
#include "../correction/vr-trial-status.hpp"
#ifdef FRAME_UI_ALL_FINGERS_BETA
#include "../correction/frame-beta-status.hpp"
#ifdef FRAME_COMPAT_OBSERVATION_UI
#include "../correction/frame-observation-status.hpp"
#include "../correction/frame-compat-ui-flow.hpp"
#endif
#endif
#include <cairo.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <string_view>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>

namespace {
#if defined(FRAME_UI_SUPERVISED_TRIAL) || defined(FRAME_UI_ALL_FINGERS_BETA)
#ifdef FRAME_UI_ALL_FINGERS_BETA
#ifdef FRAME_FINGER_RELEASE
constexpr const char* overlayKey="frame.finger.correction.release";
#else
constexpr const char* overlayKey="frame.finger.settings.beta";
#endif
#else
constexpr const char* overlayKey="frame.finger.settings.supervised";
#endif
constexpr const char* intentName="supervised-intent.txt";
constexpr const char* statusName="supervised-status.txt";
constexpr const char* brokerName="supervised-broker.txt";
constexpr const char* requestTag="FRAME_VR_SUPERVISED_1";
constexpr const char* trialDescription="LEFT PINKY manual, maximum 20s. Other fingers / learning: pending.";
constexpr const char* trialButton="START LEFT PINKY - UP TO 20s";
constexpr const char* startMessage="LEFT PINKY only, maximum 20s; heartbeat and PC OFF required.";
#else
constexpr const char* overlayKey="frame.finger.settings.preview";
constexpr const char* intentName="trial-intent.txt";
constexpr const char* statusName="trial-status.txt";
constexpr const char* brokerName="trial-broker.txt";
constexpr const char* requestTag="FRAME_VR_TRIAL_1";
constexpr const char* trialDescription="Live verification: LEFT PINKY manual (5s trial). Other fingers / learning: pending.";
constexpr const char* trialButton="TEST LEFT PINKY - 5 SECONDS";
constexpr const char* startMessage="LEFT PINKY only, maximum 5s. PC OFF confirmation required.";
#endif
volatile std::sig_atomic_t running=1;
void stop(int){running=0;}
constexpr int width=1200,height=850;
bool japanese=false;
std::string localized(std::string_view label){
    if(!japanese)return std::string(label);
    static constexpr std::pair<std::string_view,std::string_view> labels[]{
        {"STEAM FRAME - FINGER SETTINGS","Steam Frame 指トラ補正"},
        {"ALL 8 FINGERS / SOFTWARE DEADZONE / No firmware or calibration changes","左右8指のデッドゾーン補正"},
        {"LEFT","左手"},
        {"RIGHT","右手"},
        {"INDEX","人差し指"},
        {"MIDDLE","中指"},
        {"RING","薬指"},
        {"PINKY","小指"},
        {"SELECTED","対象"},
        {"SELECT","対象外"},
        {"DYNAMIC","学習モード"},
        {"MANUAL","手動"},
        {"LEARN","学習"},
        {"LEARNED","学習済み"},
        {"READY","準備完了"},
        {"WAITING","待機中"},
        {"DYNAMIC: extend the selected finger, click LEARN, hold 1.5s. No automatic learning.","学習する指を伸ばし、学習ボタンを押して1.5秒間その姿勢を保ってください。"},
        {"CORRECTION: ON (click to turn OFF)","補正設定：オン"},
        {"CORRECTION: OFF (click to turn ON)","補正設定：オフ"},
        {"SAVE SETTINGS","設定を保存"},
        {"CLOSE","閉じる"},
        {"STEAMVR AUTO-START: ON","SteamVR 自動起動：オン"},
        {"STEAMVR AUTO-START: OFF","SteamVR 自動起動：オフ"},
        {"Restart Frame to load the installed driver","ドライバーを読み込むにはFrameを再起動してください"},
        {"SteamVR update detected: correction unavailable","未対応のSteamVR更新を検出：補正停止中"},
        {"Correction unavailable: see startup log","補正できません：起動ログを確認してください"},
        {"Native status: DISCONNECTED","補正状態：未接続"},
        {"Native status: ON","補正状態：オン"},
        {"Native status: OFF","補正状態：オフ"},
        {"Settings are applied and saved immediately. Closing this app stops correction.","変更はすぐに保存されます。アプリを閉じると補正は止まります。"},
        {"Changes apply immediately. The previous ON/OFF setting is restored at startup.","起動時には前回の補正設定を引き継ぎます。"},
        {"SteamVR auto-start preference saved.","自動起動設定を保存しました。"},
        {"Auto-start setting failed. App registration is required.","自動起動を設定できませんでした。SteamVRへの登録を確認してください。"},
        {"Turn correction ON and select this finger in DYNAMIC mode first.","補正をオンにし、対象の指を学習モードにしてください。"},
        {"Learning 1.5s: hold this finger EXTENDED. Then learning freezes.","学習中：指を伸ばしたまま1.5秒間保ってください。"},
        {"Learning request failed.","学習を開始できませんでした。"},
        {"Settings saved. Trial state is shown separately.","設定を保存しました。"},
        {"Save failed; previous profile retained.","保存に失敗しました。以前の設定を保持します。"},
        {"Applied and saved. Extend a finger before clicking LEARN.","変更を反映して保存しました。"},
        {"Apply/save failed; check application status.","反映または保存に失敗しました。"},
        {"Language preference saved.","表示言語を保存しました。"},
        {"Could not save language preference.","表示言語を保存できませんでした。"},
        {"Language: 日本語 / English","言語：日本語 / English"},
        {"Deadzone: ","デッドゾーン："},
    };
    for(const auto& [en,ja]:labels)if(label==en)return std::string(ja);
    return std::string(label);
}
bool saveLanguage(const std::string& path,bool ja){
    std::string temp=path+".tmp-XXXXXX";
    int fd=mkstemp(temp.data());if(fd<0)return false;
    bool ok=write(fd,ja?"ja":"en",2)==2;
    if(close(fd)!=0)ok=false;
    if(ok&&rename(temp.c_str(),path.c_str())==0)return true;
    unlink(temp.c_str());return false;
}
struct Button {double x,y,w,h;int action;std::string label;};
void text(cairo_t* cr,double x,double y,double size,const std::string& label){
    cairo_set_source_rgb(cr,.94,.96,1);cairo_set_font_size(cr,size);cairo_move_to(cr,x,y);cairo_show_text(cr,localized(label).c_str());
}
std::string requestTrial(const std::string& directory,bool start,double threshold){
    auto ticks=std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream nonce;nonce<<std::hex<<std::setw(16)<<std::setfill('0')<<static_cast<uint64_t>(ticks);
    std::string pattern=directory+"/trial-intent.tmp-XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    int fd=mkstemp(name.data());if(fd<0)return {};
    std::ostringstream body;body<<requestTag<<' '<<time(nullptr)<<' '<<nonce.str()<<' '<<(start?"START":"STOP")<<' '<<std::setprecision(17)<<threshold<<'\n';
    auto content=body.str();
    bool ok=write(fd,content.data(),content.size())==static_cast<ssize_t>(content.size());
    if(ok&&fsync(fd)!=0)ok=false;
    if(close(fd)!=0)ok=false;
    if(ok&&rename(name.data(),(directory+"/"+intentName).c_str())!=0)ok=false;
    if(!ok){unlink(name.data());return {};}
    return nonce.str();
}
#ifdef FRAME_UI_ALL_FINGERS_BETA
std::string requestBetaLearn(const std::string& directory,unsigned row){
    std::ostringstream nonce;nonce<<std::hex<<std::setw(16)<<std::setfill('0')<<static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::ostringstream body;body<<"FRAME_BETA_LEARN_1 "<<time(nullptr)<<' '<<nonce.str()<<' '<<row<<'\n';
    auto content=body.str(),pattern=directory+"/beta-learn.tmp-XXXXXX";std::vector<char> tmp(pattern.begin(),pattern.end());tmp.push_back(0);
    int fd=mkstemp(tmp.data());if(fd<0)return {};
    bool ok=write(fd,content.data(),content.size())==static_cast<ssize_t>(content.size());if(close(fd)!=0)ok=false;
    if(ok&&rename(tmp.data(),(directory+"/beta-learn.txt").c_str())==0)return nonce.str();
    unlink(tmp.data());return {};
}
#endif
#ifdef FRAME_COMPAT_OBSERVATION_UI
bool writeObservationRequest(const std::string& directory,const char* file,uint64_t session){
    auto pattern=directory+"/observe.tmp-XXXXXX";
    int fd=mkstemp(pattern.data());if(fd<0)return false;
    const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto body="FRAME_OBSERVE_1 "+std::to_string(session)+" "+std::to_string(now)+"\n";
    bool ok=write(fd,body.data(),body.size())==static_cast<ssize_t>(body.size());
    if(close(fd)!=0)ok=false;
    if(ok&&rename(pattern.c_str(),(directory+"/"+file).c_str())==0)return true;
    unlink(pattern.c_str());return false;
}
#endif
bool writeHeartbeat(const std::string& directory,uint64_t session,uint64_t sequence,uint64_t now,uint64_t revision){
    std::ostringstream body;body<<"FRAME_UI_HEARTBEAT_1 "<<session<<' '<<sequence<<' '<<now<<' '<<revision<<'\n';
    auto content=body.str();std::string pattern=directory+"/ui-heartbeat.tmp-XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    int fd=mkstemp(name.data());if(fd<0)return false;
    bool ok=write(fd,content.data(),content.size())==static_cast<ssize_t>(content.size());
    if(close(fd)!=0)ok=false;
    if(ok&&rename(name.data(),(directory+"/ui-heartbeat.txt").c_str())!=0)ok=false;
    if(!ok)unlink(name.data());
    return ok;
}
}
int main(int argc,char** argv){
    bool startOff=false;
    std::string path,snapshot;bool show=false;double duration=0;
    for(int i=1;i<argc;++i){
        std::string arg=argv[i];
        if(arg=="--start-off"){startOff=true;continue;}
        if(arg=="--help"){std::cout<<"frame-finger-settings-overlay --profile FILE [--show-dashboard] [--seconds N] [--snapshot FILE]\nSettings UI. A trial requires a matching native candidate and explicit START; no firmware commands.\n";return 0;}
        if(arg=="--show-dashboard"){show=true;continue;}
        if(++i>=argc)return 2;
        if(arg=="--profile")path=argv[i];
        else if(arg=="--snapshot")snapshot=argv[i];
        else if(arg=="--seconds"){try{duration=std::stod(argv[i]);}catch(...){return 2;}if(!std::isfinite(duration)||duration<=0)return 2;}
        else return 2;
    }
    if(path.empty()||!std::filesystem::path(path).is_absolute())return 2;
    auto directory=std::filesystem::path(path).parent_path().string();
    frame_settings::Profile profile;
    if(std::filesystem::exists(path)){
        if(!frame_settings::loadFile(path,profile)){std::cerr<<"Invalid profile; unchanged\n";return 2;}
        std::cout<<"SettingsUI: saved profile loaded; live correction OFF\n"<<std::flush;
    }
#ifdef FRAME_UI_ALL_FINGERS_BETA
    if(startOff)profile.requested=false;
    if(!frame_settings::saveFile(directory+"/beta-live-profile.txt",profile))return 2;
    frame_beta::UiStatus betaStatus;
    int64_t learnPoll=-1;
    std::string pendingLearnId;
    std::chrono::steady_clock::time_point learnDeadline{};
#endif
    vr::EVRInitError init;vr::VR_Init(&init,vr::VRApplication_Overlay);
    if(init!=vr::VRInitError_None){std::cerr<<vr::VR_GetVRInitErrorAsEnglishDescription(init)<<'\n';return 1;}
    struct Cleanup {
        vr::IVROverlay* overlay=nullptr;vr::VROverlayHandle_t main=0,iconHandle=0;
        cairo_surface_t* surface=nullptr;cairo_t* cr=nullptr;
        ~Cleanup(){
            if(overlay){if(main){overlay->ClearOverlayTexture(main);overlay->DestroyOverlay(main);}if(iconHandle){overlay->ClearOverlayTexture(iconHandle);overlay->DestroyOverlay(iconHandle);}}
            if(cr)cairo_destroy(cr);
            if(surface)cairo_surface_destroy(surface);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            vr::VR_Shutdown();
        }
    } cleanup;
    cleanup.overlay=vr::VROverlay();auto overlay=cleanup.overlay;if(!overlay)return 1;
    if(overlay->CreateDashboardOverlay(overlayKey,"Frame Finger Correction",&cleanup.main,&cleanup.iconHandle)!=0)return 1;
    vr::HmdVector2_t scale{{float(width),float(height)}};
    if(overlay->SetOverlayWidthInMeters(cleanup.main,1.4f)!=0||overlay->SetOverlayInputMethod(cleanup.main,vr::VROverlayInputMethod_Mouse)!=0||overlay->SetOverlayMouseScale(cleanup.main,&scale)!=0)return 1;
    cleanup.surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);cleanup.cr=cairo_create(cleanup.surface);
    if(cairo_surface_status(cleanup.surface)!=CAIRO_STATUS_SUCCESS||cairo_status(cleanup.cr)!=CAIRO_STATUS_SUCCESS)return 1;
#ifdef FRAME_UI_ALL_FINGERS_BETA
    const auto languagePath=directory+"/language.txt";
    {std::ifstream language(languagePath);std::string saved;language>>saved;japanese=saved!="en";}
#endif
    cairo_select_font_face(cleanup.cr,"Noto Sans CJK JP",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL);
    std::vector<uint8_t> rgba(width*height*4);std::vector<Button> buttons;
    bool dirty=true,iconReady=false;std::string message="Preferences do not start correction. Trial requires the guarded broker.";
#ifdef FRAME_UI_ALL_FINGERS_BETA
    message="Changes apply immediately. The previous ON/OFF setting is restored at startup.";
#endif
#ifdef FRAME_UI_ALL_FINGERS_BETA
    bool autoLaunch=vr::VRApplications()->GetApplicationAutoLaunch(overlayKey);
#endif
    std::string startupIssue;
#ifdef FRAME_COMPAT_OBSERVATION_UI
    std::string compatLine, compatVerdict;
    unsigned compatWitnessed=0;
    frame_compat_ui::Flow compatFlow;
    int64_t compatPolled=-1;
#endif
    std::string trialId,trialStatus="No trial requested";
    bool brokerReady=false,trialPending=false;
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);auto start=std::chrono::steady_clock::now();
    uint64_t heartbeatSession=static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(start.time_since_epoch()).count());
    uint64_t heartbeatSequence=0,profileRevision=1;auto nextHeartbeat=start;
    const char* fingers[]{"INDEX","MIDDLE","RING","PINKY"};
    while(running&&(duration==0||std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<duration)){
        auto heartbeatNow=std::chrono::steady_clock::now();
        if(heartbeatNow>=nextHeartbeat){
            nextHeartbeat=heartbeatNow+std::chrono::milliseconds(100);
            writeHeartbeat(directory,heartbeatSession,++heartbeatSequence,
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(heartbeatNow.time_since_epoch()).count()),profileRevision);
        }
#ifdef FRAME_UI_ALL_FINGERS_BETA
        {
            std::ifstream issueFile(directory+"/startup-issue.txt");std::string issue;
            std::getline(issueFile,issue);
            if(issue!="RESTART_REQUIRED"&&issue!="START_FAILED"&&issue!="UPDATE_REQUIRED")issue.clear();
            if(issue!=startupIssue){startupIssue=issue;dirty=true;}
#ifdef FRAME_COMPAT_OBSERVATION_UI
            const auto wall=time(nullptr);
            if(wall!=compatPolled){
                compatPolled=wall;
                std::ifstream status(directory+"/compat-status.txt");
                char body[161]{};
                status.read(body,sizeof(body));
                auto diagnostic=frame_observation::parseDiagnosticStatus(std::string(body,static_cast<size_t>(status.gcount())),wall);
                std::string line;
                if(diagnostic.verdict=="structurally_compatible")
                    line="Structural candidate; correction unavailable";
                else if(diagnostic.verdict=="restore_failed")line="restart_required";
                else if(diagnostic.verdict=="rejected")
                    line=diagnostic.reason;
                if(line!=compatLine||diagnostic.verdict!=compatVerdict||diagnostic.witnessed!=compatWitnessed){
                    compatLine=std::move(line);compatVerdict=diagnostic.verdict;
                    compatWitnessed=diagnostic.witnessed;dirty=true;
                }
                const auto command=compatFlow.next(compatVerdict,profile.requested);
                if(command!=frame_compat_ui::Command::None){
                    const char* file=command==frame_compat_ui::Command::Observe?"observe-request.txt":"observe-activate.txt";
                    if(!frame_settings::saveFile(directory+"/beta-live-profile.txt",profile)||
                       !writeObservationRequest(directory,file,heartbeatSession)){
                        compatFlow.failed();profile.requested=false;++profileRevision;
                        frame_settings::saveFile(directory+"/beta-live-profile.txt",profile);
                        frame_settings::saveFile(path,profile);
                        message=japanese?"\u88dc\u6b63\u8981\u6c42\u3092\u4fdd\u5b58\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002":"Could not save correction request.";
                    }
                    dirty=true;
                }
            }
#endif
            std::ifstream input(directory+"/beta-status.txt");std::string content((std::istreambuf_iterator<char>(input)),{});
            auto latest=frame_beta::parseUiStatus(content,time(nullptr));
            const auto learnWall=time(nullptr);
            if(!pendingLearnId.empty()&&learnPoll!=learnWall){
                learnPoll=learnWall;
                std::ifstream result(directory+"/learn-result.txt");
                char buffer[129]{};result.read(buffer,sizeof(buffer));
                std::string body(buffer,static_cast<size_t>(result.gcount()));
                auto outcome=frame_beta::parseLearnResult(body,pendingLearnId,learnWall);
                if(outcome!=frame_beta::LearnOutcome::Invalid){
                    pendingLearnId.clear();
                    message=outcome==frame_beta::LearnOutcome::Applied?(japanese?"学習結果を反映しました。":"Learning applied."):
                        (japanese?"入力サンプル不足：前の補正値を維持しました。":"Not enough input samples; previous baseline kept.");
                    dirty=true;
                }else if(std::chrono::steady_clock::now()>=learnDeadline){
                    pendingLearnId.clear();
                    message=japanese?"学習結果を確認できませんでした。":"Learning result could not be confirmed.";
                    dirty=true;
                }
            }
            if(latest.valid!=betaStatus.valid||latest.on!=betaStatus.on||latest.present!=betaStatus.present||latest.witnessed!=betaStatus.witnessed||latest.learned!=betaStatus.learned){betaStatus=latest;dirty=true;}
        }
#endif
        vr::VREvent_t event{};
        {
            std::ifstream broker(directory+"/"+brokerName);
            std::string content((std::istreambuf_iterator<char>(broker)),{});
            bool ready=content.size()<=128&&frame_settings::brokerReady(content,time(nullptr));
            if(ready!=brokerReady){brokerReady=ready;dirty=true;}
        }
        if(!trialId.empty()){
            std::ifstream status(directory+"/"+statusName);
            std::string content((std::istreambuf_iterator<char>(status)),{});
            std::string latest=content.size()<=256?frame_settings::trialStatus(content,trialId,time(nullptr)):"Status unconfirmed";
            if(latest=="STOPPED"||latest=="ERROR")trialPending=false;
            if(latest!=trialStatus){trialStatus=latest;dirty=true;}
        }
        while(overlay->PollNextOverlayEvent(cleanup.main,&event,sizeof(event))){
            if(event.eventType==vr::VREvent_Quit){running=0;continue;}
            if(event.eventType!=vr::VREvent_MouseButtonUp||event.data.mouse.button!=vr::VRMouseButton_Left)continue;
            double x=event.data.mouse.x,y=height-event.data.mouse.y;
            for(const auto& button:buttons)if(x>=button.x&&x<button.x+button.w&&y>=button.y&&y<button.y+button.h){
                int action=button.action;
                std::cout<<"SettingsUI: click action="<<action<<'\n'<<std::flush;
#ifdef FRAME_UI_ALL_FINGERS_BETA
#ifdef FRAME_COMPAT_OBSERVATION_UI

#endif
                if(action==106){
                    if(saveLanguage(languagePath,!japanese)){japanese=!japanese;message="Language preference saved.";}
                    else message="Could not save language preference.";
                    dirty=true;break;
                }
                if(action==105){
                    auto apps=vr::VRApplications();bool desired=!apps->GetApplicationAutoLaunch(overlayKey);
                    auto result=apps->SetApplicationAutoLaunch(overlayKey,desired);
                    autoLaunch=apps->GetApplicationAutoLaunch(overlayKey);
                    message=result==vr::VRApplicationError_None&&autoLaunch==desired?"SteamVR auto-start preference saved.":"Auto-start setting failed. App registration is required.";
                    dirty=true;break;
                }
                if(action>=200&&action<208){
                    const auto row=static_cast<unsigned>(action-200);const auto& finger=profile.fingers[row];
                    if(!profile.requested||!finger.selected||!finger.dynamic)message="Turn correction ON and select this finger in DYNAMIC mode first.";
                    else{
                        pendingLearnId=requestBetaLearn(directory,row);
                        learnDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(6);
                        message=pendingLearnId.empty()?"Learning request failed.":"Learning 1.5s: hold this finger EXTENDED. Then learning freezes.";
                    }
                    dirty=true;break;
                }
#endif
                if(action==103||action==104){
                    if(action==103&&(!brokerReady||trialPending)){
                        message=trialPending?"Trial pending. Use STOP; do not retry.":"Trial broker is offline. START is disabled.";dirty=true;break;
                    }
                    auto& entry=profile.fingers[3];
                    if(action==103&&(!entry.selected||entry.dynamic)){message="Select LEFT PINKY in MANUAL mode first.";dirty=true;break;}
                    auto id=requestTrial(directory,action==103,entry.threshold);
                    if(action==103&&!id.empty()){trialId=id;trialStatus="Requested; waiting for guarded broker";trialPending=true;}
                    message=id.empty()?"Request failed.":action==103?startMessage:"Stop requested; awaiting restoration acknowledgement.";
                    dirty=true;break;
                }
                if(action==100)profile.requested=!profile.requested;
                else if(action==101){bool saved=frame_settings::saveFile(path,profile);message=saved?"Settings saved. Trial state is shown separately.":"Save failed; previous profile retained.";std::cout<<"SettingsUI: save="<<saved<<'\n'<<std::flush;dirty=true;break;}
                else if(action==102){running=0;break;}
                else {auto& entry=profile.fingers[action/4];switch(action%4){case 0:entry.selected=!entry.selected;break;case 1:entry.dynamic=!entry.dynamic;break;case 2:entry.threshold=std::max(0.,entry.threshold-.05);break;case 3:entry.threshold=std::min(.9,entry.threshold+.05);break;}}
                ++profileRevision;
#ifdef FRAME_UI_ALL_FINGERS_BETA
                bool applied=frame_settings::saveFile(directory+"/beta-live-profile.txt",profile);
                bool saved=frame_settings::saveFile(path,profile);
                message=applied&&saved?"Applied and saved. Extend a finger before clicking LEARN.":"Apply/save failed; check application status.";dirty=true;break;
#endif
                message="Unsaved settings. Trial state is shown separately.";dirty=true;break;
            }
        }
        if(dirty){
            auto cr=cleanup.cr;cairo_set_source_rgb(cr,.045,.065,.09);cairo_paint(cr);buttons.clear();
            text(cr,25,45,30,"STEAM FRAME - FINGER SETTINGS");
            #ifdef FRAME_UI_ALL_FINGERS_BETA
            text(cr,25,80,20,"ALL 8 FINGERS / SOFTWARE DEADZONE / No firmware or calibration changes");
#else
            text(cr,25,80,20,"Bounded trial only / Other fingers: preferences only / No firmware changes");
#endif
            auto button=[&](double x,double y,double w,int action,const std::string& label){
                buttons.push_back({x,y,w,48,action,label});
                bool disabled=action==103&&(!brokerReady||trialPending);
                cairo_set_source_rgb(cr,disabled?.12:.11,disabled?.13:.22,disabled?.14:.30);cairo_rectangle(cr,x,y,w,48);cairo_fill(cr);text(cr,x+10,y+31,18,label);
            };
            for(unsigned side=0;side<2;++side){
                double x=25+side*590;text(cr,x,125,25,side?"RIGHT":"LEFT");
                for(unsigned finger=0;finger<4;++finger){
                    unsigned row=side*4+finger;auto& entry=profile.fingers[row];double y=150+finger*105;
                    std::ostringstream value;value<<std::fixed<<std::setprecision(2)<<entry.threshold;
                    text(cr,x,y+20,21,localized(fingers[finger])+"   "+localized("Deadzone: ")+value.str());
                    button(x,y+30,130,row*4,entry.selected?"SELECTED":"SELECT");
                    button(x+140,y+30,170,row*4+1,entry.dynamic?"DYNAMIC":"MANUAL");
                    button(x+320,y+30,70,row*4+2,"-");button(x+400,y+30,70,row*4+3,"+");
#ifdef FRAME_UI_ALL_FINGERS_BETA
                    button(x+480,y+30,90,200+static_cast<int>(row),"LEARN");
#ifdef FRAME_COMPAT_OBSERVATION_UI
                    if(compatVerdict=="observing")text(cr,x+480,y+20,14,
                        (compatWitnessed&(1u<<row))?(japanese?"確認済み":"Verified"):(japanese?"未確認":"Pending"));
                    else
#endif
                    text(cr,x+480,y+20,14,(betaStatus.learned&(1u<<row))?"LEARNED":(betaStatus.witnessed&(1u<<row))?"READY":"WAITING");
#endif
                }
            }
#ifdef FRAME_UI_ALL_FINGERS_BETA
#ifdef FRAME_COMPAT_OBSERVATION_UI
            if(compatVerdict=="observing")
                text(cr,25,600,18,japanese?"\u691c\u8a3c\u4e2d\uff1a\u5de6\u53f3\u306e\u6307\u3092\u66f2\u3052\u4f38\u3070\u3057\u3057\u3066\u304f\u3060\u3055\u3044 (\u88dc\u6b63OFF)":"Checking: flex both hands (correction OFF)");
            else
#endif
            text(cr,25,600,18,"DYNAMIC: extend the selected finger, click LEARN, hold 1.5s. No automatic learning.");
            button(25,625,540,100,profile.requested?"CORRECTION: ON (click to turn OFF)":"CORRECTION: OFF (click to turn ON)");
#else
            text(cr,25,600,18,trialDescription);
            button(25,625,540,100,profile.requested?"STARTUP PREFERENCE: ON (not active)":"STARTUP PREFERENCE: OFF");
#endif
            button(600,625,255,101,"SAVE SETTINGS");button(875,625,290,102,"CLOSE");
#ifdef FRAME_UI_ALL_FINGERS_BETA
            button(600,690,565,106,"Language: 日本語 / English");
            button(25,690,540,105,autoLaunch?"STEAMVR AUTO-START: ON":"STEAMVR AUTO-START: OFF");
#ifdef FRAME_COMPAT_OBSERVATION_UI
            if(compatVerdict=="waiting")text(cr,25,765,20,japanese?"Controller\u306e\u63a5\u7d9a\u30fb\u5165\u529b\u6e96\u5099\u3092\u5f85\u3063\u3066\u3044\u307e\u3059 (\u88dc\u6b63OFF)":"Waiting for controller inputs (correction OFF)");
            else if(compatVerdict=="observing")text(cr,25,765,20,japanese?"観測中：入力は変更していません":"Observing: input unchanged");
            else if(compatVerdict=="verified")text(cr,25,765,20,japanese?"8指を確認：補正設定に従って動作します":"Eight fingers verified: following correction setting");
            else if(compatVerdict=="active")text(cr,25,765,20,profile.requested?(japanese?"構造検証済み：補正オン":"Structurally verified: correction ON"):(japanese?"構造検証済み：補正オフ":"Structurally verified: correction OFF"));
            else if(compatVerdict=="stopped")text(cr,25,765,20,japanese?"停止・復元済み":"Stopped: slot restored");
            else if(compatLine=="restart_required")text(cr,25,765,20,japanese?"\u5fa9\u5143\u672a\u78ba\u8a8d\uff1aSteamVR\u3092\u518d\u8d77\u52d5\u3057\u3066\u304f\u3060\u3055\u3044":"Restore unconfirmed: restart SteamVR");
            else if(compatLine=="Structural candidate; correction unavailable")text(cr,25,765,20,japanese?"構造検証に合格：補正ONで自動検証します":"Structure passed: turn correction ON to verify");
            else if(!compatLine.empty())text(cr,25,765,20,(japanese?"未対応：":"Unsupported: ")+compatLine);
            else
#endif
            text(cr,25,765,23,startupIssue=="RESTART_REQUIRED"?"Restart Frame to load the installed driver":startupIssue=="UPDATE_REQUIRED"?"SteamVR update detected: correction unavailable":startupIssue=="START_FAILED"?"Correction unavailable: see startup log":!betaStatus.valid?"Native status: DISCONNECTED":betaStatus.on?"Native status: ON":"Native status: OFF");
            text(cr,25,790,18,"Settings are applied and saved immediately. Closing this app stops correction.");text(cr,25,835,17,message);
#else
            button(25,700,540,103,trialButton);button(600,700,565,104,"STOP TRIAL");
            text(cr,25,790,20,"Trial: "+trialStatus+" / Broker: "+(brokerReady?"READY":"OFFLINE/BUSY"));text(cr,25,835,17,message);
#endif
            cairo_surface_flush(cleanup.surface);auto source=cairo_image_surface_get_data(cleanup.surface);int stride=cairo_image_surface_get_stride(cleanup.surface);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto a=source+y*stride+x*4;auto b=rgba.data()+(y*width+x)*4;b[0]=a[2];b[1]=a[1];b[2]=a[0];b[3]=a[3];}
            auto result=overlay->SetOverlayRaw(cleanup.main,rgba.data(),width,height,4);
            if(result!=vr::VROverlayError_None){std::cerr<<"SetOverlayRaw failed: "<<int(result)<<'\n';return 1;}
            if(!iconReady)iconReady=overlay->SetOverlayRaw(cleanup.iconHandle,rgba.data(),width,height,4)==vr::VROverlayError_None;
            dirty=false;
        }
        if(show){overlay->ShowDashboard(overlayKey);show=false;}
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if(!trialId.empty())requestTrial(directory,false,profile.fingers[3].threshold);
    writeHeartbeat(directory,0,0,0,0);
    if(!snapshot.empty()&&cairo_surface_write_to_png(cleanup.surface,snapshot.c_str())!=CAIRO_STATUS_SUCCESS)return 1;
}

