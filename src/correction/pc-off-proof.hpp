#pragma once
#include <cstdint>
#include <sstream>
#include <string>
namespace frame_supervision {
struct PcOffProof {uint64_t uiSession=0,relaySession=0,sequence=0,issuedMs=0,pcSequence=0;};
inline bool parsePcOffProof(const std::string& text,uint64_t uiSession,uint64_t now,PcOffProof& output,uint64_t lifetime=250){
    if(!lifetime||lifetime>5000||text.size()>256||text.find('-')!=std::string::npos)return false;
    std::istringstream input(text);std::string tag,extra;PcOffProof next;
    if(!(input>>tag>>next.uiSession>>next.relaySession>>next.sequence>>next.issuedMs>>next.pcSequence)||input>>extra||
       tag!="FRAME_PC_OFF_PROOF_1"||!uiSession||next.uiSession!=uiSession||!next.relaySession||!next.sequence||
       !next.pcSequence||!next.issuedMs||next.issuedMs>now||now-next.issuedMs>=lifetime||next.issuedMs>UINT64_MAX-lifetime)return false;
    output=next;return true;
}
}
