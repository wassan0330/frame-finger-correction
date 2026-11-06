#include "frame-settings-profile.hpp"
#include <cassert>
#include <cstdio>
int main(){
    frame_settings::Profile profile;
    profile.requested=true;profile.fingers[7]={true,true,.35};
    auto text=frame_settings::serialize(profile);
    frame_settings::Profile restored;assert(frame_settings::parse(text,restored));
    assert(restored.requested&&restored.fingers[7].selected&&restored.fingers[7].dynamic&&restored.fingers[7].threshold==.35);
    assert(!restored.fingers[0].selected);
    assert(!frame_settings::parse(text+"extra",restored));
    assert(!frame_settings::parse("FRAME_FINGER_PROFILE_1 1\n0 0 1 manual nan",restored));
    assert(restored.fingers[7].threshold==.35); // Invalid input never replaces state.
    profile.fingers[0].threshold=1;assert(frame_settings::serialize(profile).empty());
    std::puts("PASS native VR preferences roundtrip, side isolation and invalid profile refusal");
}
