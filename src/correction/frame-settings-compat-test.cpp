// Characterization: preferences.txt written by v0.1.0 must keep loading, and
// the 4096-byte limit must stay exact on both the parse and the file paths.
#include "frame-settings-store.hpp"
#include <cassert>
#include <cstdio>
#include <string>

// Exact bytes v0.1.0 writes for a default profile (what an upgraded user may have on disk).
static const char defaultV010[] =
    "FRAME_FINGER_PROFILE_1 0\n"
    "0 0 0 manual 0.20000000000000001\n0 1 0 manual 0.20000000000000001\n"
    "0 2 0 manual 0.20000000000000001\n0 3 0 manual 0.20000000000000001\n"
    "1 0 0 manual 0.20000000000000001\n1 1 0 manual 0.20000000000000001\n"
    "1 2 0 manual 0.20000000000000001\n1 3 0 manual 0.20000000000000001\n";

int main() {
    frame_settings::Profile profile;
    assert(frame_settings::serialize(profile) == defaultV010);

    frame_settings::Profile mixed; mixed.requested = true;
    mixed.fingers[0] = {true, false, .9}; mixed.fingers[5] = {true, true, 0};
    const std::string text = frame_settings::serialize(mixed);
    assert(text == "FRAME_FINGER_PROFILE_1 1\n"
                   "0 0 1 manual 0.90000000000000002\n0 1 0 manual 0.20000000000000001\n"
                   "0 2 0 manual 0.20000000000000001\n0 3 0 manual 0.20000000000000001\n"
                   "1 0 0 manual 0.20000000000000001\n1 1 1 dynamic 0\n"
                   "1 2 0 manual 0.20000000000000001\n1 3 0 manual 0.20000000000000001\n");

    // Hand-written / older shorter forms still parse (whitespace-separated tokens).
    frame_settings::Profile loaded;
    assert(frame_settings::parse("FRAME_FINGER_PROFILE_1 1 0 0 1 manual .5 0 1 0 manual .2 0 2 0 manual .2 0 3 0 manual .2 "
                                 "1 0 0 manual .2 1 1 0 manual .2 1 2 0 manual .2 1 3 1 dynamic .3", loaded));
    assert(loaded.requested && loaded.fingers[0].threshold == .5 && loaded.fingers[7].dynamic);

    // 4096 bytes accepted (trailing whitespace padding), 4097 rejected, state untouched on refusal.
    std::string padded = text; padded.resize(4096, ' ');
    assert(frame_settings::parse(padded, loaded) && loaded.fingers[0].threshold == .9);
    std::string over = text; over.resize(4097, ' ');
    frame_settings::Profile untouched;
    assert(!frame_settings::parse(over, untouched) && !untouched.requested);
    // Boundary out-of-range values.
    std::string tooHigh = defaultV010; tooHigh.replace(tooHigh.find("0.20000000000000001"), 19, "0.90000000000000013");
    assert(!frame_settings::parse(tooHigh, loaded));

    char temporary[] = "/tmp/frame-settings-compat-XXXXXX";
    auto directory = mkdtemp(temporary); assert(directory);
    auto path = std::string(directory) + "/preferences.txt";
    { std::ofstream file(path, std::ios::binary); file << defaultV010; }
    assert(frame_settings::loadFile(path, loaded) && !loaded.requested);
    { std::ofstream file(path, std::ios::binary); file << padded; }
    assert(frame_settings::loadFile(path, loaded));
    { std::ofstream file(path, std::ios::binary); file << over; }
    assert(!frame_settings::loadFile(path, loaded));
    assert(unlink(path.c_str()) == 0 && rmdir(directory) == 0);
    std::puts("PASS settings compat: v0.1.0 exact bytes, legacy token layout, 4096/4097 boundary on parse and file load");
}
