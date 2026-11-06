#include "frame-beta-status.hpp"
#include <cassert>
using frame_beta::LearnOutcome;
using frame_beta::parseLearnResult;
int main(){
    const std::string id="0123456789abcdef";
    assert(parseLearnResult("FRAME_LEARN_RESULT_2 10 "+id+" 7 2 insufficient",id,11)==LearnOutcome::Insufficient);
    assert(parseLearnResult("FRAME_LEARN_RESULT_2 10 "+id+" 7 63 applied",id,11)==LearnOutcome::Applied);
    assert(parseLearnResult("FRAME_LEARN_RESULT_2 10 "+id+" 7 2 applied",id,11)==LearnOutcome::Invalid);
    assert(parseLearnResult("FRAME_LEARN_RESULT_2 10 "+id+" 7 63 applied",id,14)==LearnOutcome::Invalid);
    assert(parseLearnResult("FRAME_LEARN_RESULT_2 10 "+id+" 7 63 applied","ffffffffffffffff",11)==LearnOutcome::Invalid);
}
