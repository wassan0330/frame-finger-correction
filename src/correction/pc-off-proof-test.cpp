#include "pc-off-proof.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace frame_supervision;PcOffProof p;
 assert(parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 8 9 1000 10",7,1000,p));
 assert(parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 8 9 1000 10",7,1249,p));
 assert(!parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 8 9 1000 10",7,1250,p));
 assert(!parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 8 9 1000 10",8,1000,p));
 assert(!parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 8 9 1001 10",7,1000,p));
 assert(!parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 0 9 1000 10",7,1000,p));
 assert(!parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 8 9 -1 10",7,1000,p));
 assert(!parsePcOffProof("FRAME_PC_OFF_PROOF_1 7 8 9 18446744073709551615 10",7,UINT64_MAX,p));
 std::puts("PASS PC OFF proof freshness, UI binding, malformed/overflow rejection");
}
