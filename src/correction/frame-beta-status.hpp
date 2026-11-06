#pragma once
#include <cstdint>
#include <sstream>
#include <string>
namespace frame_beta {
struct UiStatus {bool valid=false,on=false;unsigned present=0,witnessed=0,learned=0;};
inline UiStatus parseUiStatus(const std::string& text,int64_t now){
 UiStatus out;std::istringstream input(text);std::string tag,state,extra;int64_t stamp=0;
 if(text.size()>160||text.find('-')!=std::string::npos||!(input>>tag>>stamp>>state>>out.present>>out.witnessed>>out.learned)||input>>extra||
    tag!="FRAME_BETA_STATUS_1"||(state!="ON"&&state!="OFF")||stamp<0||stamp>now||now-stamp>2||
    out.present>255||out.witnessed>255||out.learned>255)return {};
 out.valid=true;out.on=state=="ON";return out;
}
enum class LearnOutcome { Invalid, Applied, Insufficient };
inline LearnOutcome parseLearnResult(const std::string& body,const std::string& expected,int64_t now){
 if(body.size()>128||expected.size()!=16)return LearnOutcome::Invalid;
 std::istringstream parsed(body);
 std::string tag,id,outcome,extra;int64_t stamp=0;unsigned row=0,count=0;
 if(!(parsed>>tag>>stamp>>id>>row>>count>>outcome)||parsed>>extra||
    tag!="FRAME_LEARN_RESULT_2"||id!=expected||stamp<0||stamp>now||now-stamp>3||row>=8||count>63)
  return LearnOutcome::Invalid;
 if(outcome=="applied"&&count>=3)return LearnOutcome::Applied;
 if(outcome=="insufficient"&&count<3)return LearnOutcome::Insufficient;
 return LearnOutcome::Invalid;
}

}
