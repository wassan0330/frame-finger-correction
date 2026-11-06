#include "scalar-component-identity.hpp"
#include <cassert>
#include <iostream>
using namespace frame_scalar;
int main(){
    CreatedScalar binding{(uint64_t{2}<<32)|7,(uint64_t{1}<<32)|2,
                         "/input/capsense/pinky","pair-independent-example",Hand::Left,true,true,3};
    DeviceSnapshot live{2,"pair-independent-example","frame_controller",Hand::Left,true,3};
    assert(matches(binding,live));
    auto bad=live;bad.index=3;assert(!matches(binding,bad));
    bad=live;bad.serial="another-controller";assert(!matches(binding,bad));
    bad=live;bad.hand=Hand::Right;assert(!matches(binding,bad));
    bad=live;bad.connected=false;assert(!matches(binding,bad));
    bad=live;bad.generation=4;assert(!matches(binding,bad));
    bad=live;bad.hand=Hand::Unknown;assert(!matches(binding,bad));
    bad=live;bad.serial.clear();assert(!matches(binding,bad));
    bad=live;bad.type="other_controller";assert(!matches(binding,bad));
    auto wrong=binding;wrong.path="/input/trigger/value";assert(!matches(wrong,live));
    wrong=binding;wrong.component=(uint64_t{3}<<32)|7;assert(!matches(wrong,live));
    wrong=binding;wrong.normalizedOneSided=false;assert(!matches(wrong,live));
    wrong=binding;wrong.creationSucceeded=false;assert(!matches(wrong,live));
    wrong=binding;wrong.generation=0;bad=live;bad.generation=0;assert(!matches(wrong,bad));
    assert(!deviceFromContainer((uint64_t{6}<<32)|2));
    assert(!deviceFromContainer((uint64_t{1}<<32)|64));
    assert(!deviceFromComponent(uint64_t{2}<<32));
    // HMD index zero is representable; the type/role gate still excludes it.
    assert(deviceFromComponent(1)&&*deviceFromComponent(1)==0);
    std::cout<<"PASS component/container identity, role/serial/generation guards; no runtime calls\n";
}
