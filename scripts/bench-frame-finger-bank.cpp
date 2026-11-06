#include "../src/correction/frame-finger-bank.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>

static volatile float sink;
static int32_t original(void*,uint64_t,float value,double){sink=value;return 0;}
int main(){
    using namespace frame_beta;
    using namespace frame_scalar;
    FingerBank bank;
    std::array<std::optional<Binding>,8> bindings{};
    for(unsigned row=0;row<8;++row){
        unsigned device=row<4?1:2;
        auto hand=row<4?Hand::Left:Hand::Right;
        const std::string serial=row<4?"left":"right";
        bindings[row]=Binding{
            {(uint64_t(device)<<32)|(row%4+10),0x100000000ull|device,fingerPaths[row%4],serial,hand,true,true,0,false},
            {device,serial,"frame_controller",hand,true,0},0x4000+row*0x100};
    }
    frame_settings::Profile profile;
    profile.requested=true;
    for(auto& f:profile.fingers)f={true,false,.2};
    auto* receiver=reinterpret_cast<void*>(uintptr_t{0x1234});
    bank.configure(bindings,1000);
    if(!bank.renewStandalone(profile,1,true,1000,1000))return EXIT_FAILURE;
    for(auto& binding:bindings)bank.forward(original,receiver,binding->scalar.component,.5f,0,1001);
    const auto target=bindings.back()->scalar.component;
    constexpr unsigned n=1000000;
    for(auto handle:{target,uint64_t{0x300000001}}){
        auto start=std::chrono::steady_clock::now();
        for(unsigned i=0;i<n;++i)bank.forward(original,receiver,handle,.5f,0,1002);
        auto elapsed=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count();
        std::printf("%s %.1f ns/call (%u iterations)\n",handle==target?"last selected row":"unrelated",elapsed/n,n);
    }
}
