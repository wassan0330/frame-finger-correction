#include <openvr.h>
#include <iostream>
#include <string_view>

int main(int argc,char** argv){
    if(argc!=3)return 2;
    const std::string_view action=argv[1];
    if(action!="register"&&action!="unregister"&&action!="check")return 2;
    constexpr auto key="frame.finger.correction.release";
    vr::EVRInitError error=vr::VRInitError_None;
    vr::VR_Init(&error,vr::VRApplication_Background);
    if(error!=vr::VRInitError_None){
        std::cerr<<vr::VR_GetVRInitErrorAsEnglishDescription(error)<<'\n';return 3;
    }
    auto apps=vr::VRApplications();
    auto result=vr::VRApplicationError_None;
    if(!apps){vr::VR_Shutdown();return 3;}
    if(action=="register")result=apps->AddApplicationManifest(argv[2],false);
    if(action=="unregister"&&apps->IsApplicationInstalled(key)){
        if(apps->GetApplicationAutoLaunch(key))result=apps->SetApplicationAutoLaunch(key,false);
        if(result==vr::VRApplicationError_None)result=apps->RemoveApplicationManifest(argv[2]);
    }
    const bool installed=apps->IsApplicationInstalled(key);
    std::cout<<"Frame Finger Correction: "<<(installed?"registered":"not registered")<<", result="<<int(result)<<'\n';
    vr::VR_Shutdown();
    if(result!=vr::VRApplicationError_None)return 4;
    if(action=="unregister")return installed?4:0;
    return installed?0:5;
}
