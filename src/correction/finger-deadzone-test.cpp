#include "finger-deadzone.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace frame_correction;
int main(){
 FingerDeadzone leftPinky,rightPinky;
 DeadzoneSettings s;s.enabled=true;s.manual=.6;
 assert(leftPinky.map(.5,s)==0);assert(std::abs(leftPinky.map(.8,s)-.5)<1e-9);
 assert(leftPinky.map(1,s)==1);
 s.mode=DeadzoneMode::Dynamic;
 assert(leftPinky.map(.8,s)==.8);
 assert(!leftPinky.observe(.6,.1,false,false,s));assert(!leftPinky.ready());
 assert(leftPinky.observe(.6,.1,true,false,s));assert(!rightPinky.ready());
 auto before=leftPinky.threshold(s);
 assert(!leftPinky.observe(.8,.1,true,true,s));assert(leftPinky.threshold(s)==before);
 assert(leftPinky.observe(.8,.1,true,false,s));
 assert(std::abs(leftPinky.threshold(s)-before-.002)<1e-9);
 assert(!leftPinky.observe(1,.1,true,false,s));
 assert(!leftPinky.observe(.8,10,true,false,s));
 auto nan=std::numeric_limits<double>::quiet_NaN();
 assert(!leftPinky.observe(nan,.1,true,false,s));assert(std::isnan(leftPinky.map(nan,s)));
 s.enabled=false;assert(leftPinky.map(.8,s)==.8);
 leftPinky.reset();assert(!leftPinky.ready());
 std::cout<<"PASS: manual mapping, dynamic explicit extension, freeze, rate limit, saturation rejection, independent state, reset, bypass\n";
}
