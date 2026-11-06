#include "readonly-slot-transaction.hpp"
#include <cassert>
using namespace frame_scalar;
int main(){
    uintptr_t slot=2;
    ReadonlySlotTransaction transaction(&slot,1,2);
    unsigned calls=0;
    auto result=transaction.restoreBounded([&](bool writable){
        ++calls;
        return writable || calls>2;
    });
    assert(result==SlotResult::AlreadyOriginal && slot==1 && calls==3);
    slot=2; calls=0;
    result=transaction.restoreBounded([&](bool){++calls;return false;});
    assert(result==SlotResult::ProtectionFailure && slot==2 && calls==3);
    slot=3; calls=0;
    result=transaction.restoreBounded([&](bool){++calls;return true;});
    assert(result==SlotResult::UnexpectedValue && slot==3 && calls==0);
    slot=1; calls=0;
    result=transaction.restoreBounded([&](bool writable){++calls;return !writable;},true);
    assert(result==SlotResult::AlreadyOriginal && calls==1);
}
