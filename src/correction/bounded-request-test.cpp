#include "bounded-request.hpp"
#include <cassert>
#include <cstdio>
int main(){
    frame_scalar::BoundedRequest request;
    auto parse=[&](const std::string& suffix,int64_t now=1000){
        return frame_scalar::parseBoundedRequest("FRAME_SCALAR_BOUNDED_1 1000 0123456789abcdef "+suffix,now,request);
    };
    assert(parse("ON 5 0.8 3 PC_OFF_CONFIRMED")&&request.on&&request.seconds==5&&request.generation==3);
    assert(!parse("ON 6 0.8 3 PC_OFF_CONFIRMED"));
    assert(!parse("ON 5 0.8 3 PC_ON"));
    assert(!parse("ON 5 0.8 3 PC_OFF_CONFIRMED extra"));
    assert(!parse("ON 5 0.8 -1 PC_OFF_CONFIRMED"));
    assert(!parse("ON 5 0.8 18446744073709551616 PC_OFF_CONFIRMED"));
    assert(!parse("ON 5 0.8 0 PC_OFF_CONFIRMED"));
    assert(!parse("ON 5 0.8 3 PC_OFF_CONFIRMED",1003));
    assert(!parse("ON 5 0.8 3 PC_OFF_CONFIRMED",999));
    assert(parse("OFF 0 0 0 STOP")&&!request.on);
    assert(!parse("OFF 5 0 0 STOP"));
    assert(!parse("ON 5 0.91 3 PC_OFF_CONFIRMED"));
    assert(!parse("ON 5 nan 3 PC_OFF_CONFIRMED"));
    std::puts("PASS bounded request: duration, freshness, OFF, PC assertion, generation/overflow and extra-token refusal");
}
