#include "frame-settings-store.hpp"
#include <cassert>
#include <cstdio>
int main(){
    char temporary[]="/tmp/frame-settings-store-XXXXXX";
    auto directory=mkdtemp(temporary);assert(directory);
    auto root=std::filesystem::canonical(directory);
    assert(root.parent_path()==std::filesystem::canonical("/tmp"));
    auto path=(root/"preferences.txt").string();
    frame_settings::Profile profile;profile.requested=true;profile.fingers[3]={true,false,.8};
    assert(frame_settings::saveFile(path,profile));
    frame_settings::Profile loaded;assert(frame_settings::loadFile(path,loaded));
    assert(loaded.requested&&loaded.fingers[3].selected&&loaded.fingers[3].threshold==.8&&!loaded.fingers[7].selected);
    profile.fingers[3].threshold=2;assert(!frame_settings::saveFile(path,profile));
    assert(frame_settings::loadFile(path,loaded)&&loaded.fingers[3].threshold==.8);
    {std::ofstream file(path);file<<"bad data";}
    assert(!frame_settings::loadFile(path,loaded)&&loaded.fingers[3].threshold==.8);
    assert(unlink(path.c_str())==0);assert(rmdir(directory)==0);
    std::puts("PASS actual settings save/reload, invalid-save preservation and malformed-load refusal");
}
