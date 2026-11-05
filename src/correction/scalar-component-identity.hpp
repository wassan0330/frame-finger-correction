#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace frame_scalar {
// Layout is statically verified only for vrserver cb97844f... . It must never
// be used as an update authorization without an independently verified runtime.
inline std::optional<uint32_t> deviceFromContainer(uint64_t container){
    if((container>>32)!=1||uint32_t(container)>=64)return std::nullopt;
    return uint32_t(container);
}
inline std::optional<uint32_t> deviceFromComponent(uint64_t component){
    if(!uint32_t(component)||(component>>32)>=64)return std::nullopt;
    return uint32_t(component>>32);
}
enum class Hand {Unknown,Left,Right};
struct DeviceSnapshot {
    uint32_t index=64;
    std::string serial,type;
    Hand hand=Hand::Unknown;
    bool connected=false;
    uint64_t generation=0;
};
struct CreatedScalar {
    uint64_t component=0,container=0;
    std::string path,serial;
    Hand hand=Hand::Unknown;
    bool creationSucceeded=false,normalizedOneSided=false;
    uint64_t generation=0;
};
// An alternative proof for an already-created component. It must combine the
// checked record with an actual successful update observation; metadata alone
// is insufficient and must never be mislabeled as a CreateScalar event.
struct ExistingScalar {
    uint64_t component=0,container=0;
    std::string path,serial;
    Hand hand=Hand::Unknown;
    bool metadataVerified=false,normalizedOneSided=false;
    uint64_t generation=0;
    bool successfulUpdateVerified=false;
};
// This gate does not discover a handle or write anything. A caller must first
// observe a successful scalar creation, with its real container and path.
inline bool matches(const CreatedScalar& created,const DeviceSnapshot& current){
    auto owner=deviceFromContainer(created.container);
    auto componentOwner=deviceFromComponent(created.component);
    return created.creationSucceeded&&created.normalizedOneSided&&owner&&componentOwner&&
        *owner==*componentOwner&&*owner==current.index&&current.connected&&
        current.type=="frame_controller"&&!current.serial.empty()&&current.serial==created.serial&&
        current.hand!=Hand::Unknown&&current.hand==created.hand&&
        current.generation>0&&current.generation==created.generation&&
        (created.path=="/input/capsense/ring"||created.path=="/input/capsense/pinky");
}
inline bool matchesWithUpdateEvidence(const ExistingScalar& existing,const DeviceSnapshot& current,bool successfulUpdate){
    auto owner=deviceFromContainer(existing.container);
    auto componentOwner=deviceFromComponent(existing.component);
    return existing.metadataVerified&&successfulUpdate&&existing.normalizedOneSided&&
        owner&&componentOwner&&*owner==*componentOwner&&*owner==current.index&&current.connected&&
        current.type=="frame_controller"&&!current.serial.empty()&&current.serial==existing.serial&&
        current.hand!=Hand::Unknown&&current.hand==existing.hand&&current.generation>0&&
        current.generation==existing.generation&&
        (existing.path=="/input/capsense/ring"||existing.path=="/input/capsense/pinky");
}
inline bool matches(const ExistingScalar& existing,const DeviceSnapshot& current){
    return matchesWithUpdateEvidence(existing,current,existing.successfulUpdateVerified);
}
}
