#pragma once
#include <cstdint>
#include <sstream>
#include <string>
namespace frame_observation {
struct DiagnosticStatus { std::string verdict, reason; unsigned witnessed=0; };
inline DiagnosticStatus parseDiagnosticStatus(const std::string& text, int64_t now) {
    if (text.size()>160) return {};
    std::istringstream stream(text);
    std::string tag, extra;
    DiagnosticStatus result;
    int64_t stamp=0;
    if (!(stream>>tag>>stamp>>result.verdict>>result.reason) || stream>>extra ||
        tag!="FRAME_COMPAT_STATUS_1" || stamp<0 || stamp>now || now-stamp>3)
        return {};
    if(result.verdict=="observing"||result.verdict=="verified"||result.verdict=="active"){
        if(result.reason.empty()||result.reason.size()>3)return {};
        for(char digit:result.reason){
            if(digit<'0'||digit>'9')return {};
            result.witnessed=result.witnessed*10+unsigned(digit-'0');
        }
        if(result.witnessed>255||(result.verdict!="observing"&&result.witnessed==0))return {};
        return result;
    }
    if((result.verdict=="stopped"||result.verdict=="waiting")&&result.reason=="none")return result;
    if (result.verdict!="structurally_compatible" && result.verdict!="rejected" &&
        !(result.verdict=="restore_failed" && result.reason=="restart_required")) return {};
    if (result.reason.empty() || result.reason.size()>48 ||
        result.reason.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789")!=std::string::npos)
        return {};
    return result;
}
}
