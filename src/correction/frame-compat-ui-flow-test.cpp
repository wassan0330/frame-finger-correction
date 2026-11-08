#include "frame-compat-ui-flow.hpp"
#include <cassert>
using frame_compat_ui::Command;
int main(){
    frame_compat_ui::Flow savedOn;
    assert(savedOn.next("waiting",true)==Command::None);
    assert(savedOn.next("structurally_compatible",true)==Command::Observe);
    assert(savedOn.next("structurally_compatible",true)==Command::None);
    assert(savedOn.next("observing",true)==Command::None);
    assert(savedOn.next("verified",false)==Command::None);
    assert(savedOn.next("verified",true)==Command::Activate);
    assert(savedOn.next("verified",true)==Command::None);
    assert(savedOn.next("active",false)==Command::None);
    assert(savedOn.next("active",true)==Command::None);
    frame_compat_ui::Flow stale;
    assert(stale.next("verified",true)==Command::None);
    frame_compat_ui::Flow off;
    for(auto state:{"waiting","structurally_compatible","verified","active"})
        assert(off.next(state,false)==Command::None);
    for(auto state:{"","rejected","restore_failed","stopped","known"})
        assert(off.next(state,true)==Command::None);
    assert(off.next("structurally_compatible",true)==Command::Observe);
    off.failed();
    assert(off.next("verified",true)==Command::None);
    assert(off.next("structurally_compatible",true)==Command::None);
}
