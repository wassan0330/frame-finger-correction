#include "scalar-component-snapshot.hpp"
#include <array>
#include <cassert>
#include <iostream>
using namespace frame_scalar;
struct Memory {
    std::array<unsigned char,8192> bytes{};
    bool change=false;unsigned roots=0;
    bool read(uint64_t address,void* output,size_t size){
        if(address<4096||address-4096>bytes.size()||size>bytes.size()-(address-4096))return false;
        if(change&&address==4096+280&&++roots==3)put(6000+32,(uint64_t{2}<<32)|9);
        std::memcpy(output,bytes.data()+address-4096,size);return true;
    }
    template<class T>void put(uint64_t address,T value){std::memcpy(bytes.data()+address-4096,&value,sizeof(value));}
    Memory(){
        put(4096+280,uint64_t{5000});put(5000+32,uint64_t{0x100000002});put(5000+40,uint64_t{5504});
        put(5504+16,uint64_t{6000});put(6000+32,(uint64_t{2}<<32)|7);
        put(6000+40,uint64_t{7000});put(6000+48,uint64_t{21});
        const char* name="/input/capsense/pinky";
        put(6000+48,uint64_t(std::strlen(name)));
        std::memcpy(bytes.data()+7000-4096,name,std::strlen(name)+1);
        put(6000+72,uint32_t{2});put(6000+84,uint32_t{0});
    }
};
int main(){
    Memory good;auto result=stableSnapshot(good,4096,2);
    assert(result.valid&&result.components.size()==1&&result.components[0].handle==0x200000007);
    assert(result.components[0].path=="/input/capsense/pinky");
    good.put(6000+116,float{.75f});float value=0;
    assert(storedScalar(good,result.components[0],value)&&value==.75f);
    good.put(6000+116,float{2.f});assert(!storedScalar(good,result.components[0],value));
    good.put(6000+116,uint32_t{0x7fc00000});assert(!storedScalar(good,result.components[0],value));
    good.put(6000+116,float{.25f});good.put(6000+32,uint64_t{0x200000009});
    assert(!storedScalar(good,result.components[0],value));
    good=Memory{};
    Memory bad;bad.put(6000+16,uint64_t{6000});assert(!stableSnapshot(bad,4096,2).valid);
    bad=Memory{};bad.put(6000+40,uint64_t{999999});assert(!stableSnapshot(bad,4096,2).valid);
    bad=Memory{};bad.put(6000+32,uint64_t{0x300000007});assert(!stableSnapshot(bad,4096,2).valid);
    bad=Memory{};bad.put(6000+48,uint64_t{129});assert(!stableSnapshot(bad,4096,2).valid);
    bad=Memory{};bad.change=true;assert(!stableSnapshot(bad,4096,2).valid);
    bad=Memory{};bad.put(5504+16,uint64_t{0});assert(stableSnapshot(bad,4096,2).valid);
    assert(!stableSnapshot(good,4096,64).valid);
    assert(!stableSnapshot(good,UINT64_MAX,2).valid);
    std::cout<<"PASS bounded read-only component snapshot, cycle/unmapped/owner/length/change rejection\n";
}
