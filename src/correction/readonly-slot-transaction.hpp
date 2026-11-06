#pragma once
#include <cstdint>

namespace frame_scalar {
enum class SlotResult {Changed,AlreadyOriginal,UnexpectedValue,ProtectionFailure,Invalid};
// A primitive, not a runtime hook installer. Caller must validate provenance,
// pin all replacement code, and supply the exact original page protection.
// No machine instructions or device state are changed by this primitive.
class ReadonlySlotTransaction {
    uintptr_t* slot;
    uintptr_t original,replacement;
public:
    ReadonlySlotTransaction(uintptr_t* location,uintptr_t source,uintptr_t target):
        slot(location),original(source),replacement(target){}
    template<class Protect>
    SlotResult install(Protect&& protect){
        if(!slot||reinterpret_cast<uintptr_t>(slot)%alignof(uintptr_t)||!original||!replacement||original==replacement)return SlotResult::Invalid;
        if(__atomic_load_n(slot,__ATOMIC_ACQUIRE)!=original)return SlotResult::UnexpectedValue;
        if(!protect(true))return SlotResult::ProtectionFailure;
        auto expected=original;
        bool exchanged=__atomic_compare_exchange_n(slot,&expected,replacement,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
        if(!protect(false)){
            if(exchanged&&protect(true)){expected=replacement;__atomic_compare_exchange_n(slot,&expected,original,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
            protect(false); // best effort; caller must surface a persistent failure
            return SlotResult::ProtectionFailure;
        }
        return exchanged?SlotResult::Changed:SlotResult::UnexpectedValue;
    }
    template<class Protect>
    SlotResult restore(Protect&& protect){
        if(!slot||reinterpret_cast<uintptr_t>(slot)%alignof(uintptr_t))return SlotResult::Invalid;
        auto value=__atomic_load_n(slot,__ATOMIC_ACQUIRE);
        if(value==original)return SlotResult::AlreadyOriginal;
        if(value!=replacement)return SlotResult::UnexpectedValue;
        if(!protect(true))return SlotResult::ProtectionFailure;
        auto expected=replacement;
        bool exchanged=__atomic_compare_exchange_n(slot,&expected,original,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
        if(!protect(false))return SlotResult::ProtectionFailure;
        return exchanged?SlotResult::Changed:SlotResult::UnexpectedValue;
    }
    // Control thread only. Never replace a foreign pointer. When a failed
    // restore already changed the slot, retry just the page protection.
    template<class Protect>
    SlotResult restoreBounded(Protect&& protect, bool protectionUncertain=false) {
        for(unsigned attempt=0;attempt<3;++attempt){
            auto result=restore(protect);
            if(result==SlotResult::AlreadyOriginal&&protectionUncertain)
                result=protect(false)?SlotResult::AlreadyOriginal:SlotResult::ProtectionFailure;
            if(result!=SlotResult::ProtectionFailure)return result;
            protectionUncertain=true;
        }
        return SlotResult::ProtectionFailure;
    }

};
}
