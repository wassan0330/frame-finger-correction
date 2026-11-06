// Characterization tests: these pin the CURRENT behaviour of v0.1.0 so later
// changes are deliberate. A failing assertion here is not necessarily a bug in
// the new code; it means observable behaviour changed and must be reviewed.
// Assertions marked "known limitation" describe behaviour that Phase 4 may
// change on purpose (and then update together with this file).
#include "finger-deadzone.hpp"
#include "frame-finger-bank.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace frame_correction;

static bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

static DeadzoneSettings dynamicSettings() {
    DeadzoneSettings s; s.enabled = true; s.mode = DeadzoneMode::Dynamic; return s;
}

static void firstSampleSeedsBaseline() {
    // Known limitation: the first accepted sample becomes the baseline as-is,
    // so a single early outlier dominates a whole learning window.
    auto s = dynamicSettings();
    FingerDeadzone zone;
    assert(zone.observe(.9, .05, true, false, s));
    assert(near(zone.threshold(s), .92));
    for (int i = 0; i < 100; ++i) assert(zone.observe(.1, .05, true, false, s)); // 5 s of steady 0.1
    // Rate limit 0.02/s * 5 s = 0.1 total movement: baseline .9 -> .8.
    assert(near(zone.threshold(s), .82));
    // Same 5 s without the outlier converges to the true value immediately.
    FingerDeadzone clean;
    for (int i = 0; i < 100; ++i) assert(clean.observe(.1, .05, true, false, s));
    assert(near(clean.threshold(s), .12));
}

static void dtWindow() {
    auto s = dynamicSettings();
    FingerDeadzone zone;
    assert(zone.observe(.3, .25, true, false, s));          // dt == 0.25 accepted
    assert(!zone.observe(.1, .2500001, true, false, s));    // Known limitation: dt > 0.25 dropped
    assert(!zone.observe(.1, 1.0, true, false, s));
    assert(!zone.observe(.1, 0, true, false, s));
    assert(!zone.observe(.1, -1, true, false, s));
    assert(near(zone.threshold(s), .32));                   // rejected samples leave state untouched
    assert(zone.observe(.1, .25, true, false, s));
    assert(near(zone.threshold(s), .32 - .02 * .25));       // movement capped at updatePerSecond*dt
    // The first sample is also subject to the dt window.
    FingerDeadzone fresh;
    assert(!fresh.observe(.3, .3, true, false, s));
    assert(!fresh.ready());
}

static void inputBounds() {
    auto s = dynamicSettings();
    FingerDeadzone zone;
    assert(!zone.observe(.951, .05, true, false, s));       // above maximum: saturated / curled
    assert(!zone.observe(-0.001, .05, true, false, s));
    assert(zone.observe(.95, .05, true, false, s));
    assert(near(zone.threshold(s), .95));                   // capped at maximum, not baseline+margin
    assert(zone.map(.95, s) == 0);
    assert(zone.map(1, s) == 1);
}

// FNV-1a over float bit patterns of map() for a dense grid. Pins manual-mode
// output bit-for-bit (it must stay identical through Phase 4).
static uint64_t manualDigest() {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](float v) {
        uint32_t bits; std::memcpy(&bits, &v, sizeof bits);
        for (int i = 0; i < 4; ++i) { h ^= (bits >> (8 * i)) & 0xff; h *= 1099511628211ull; }
    };
    FingerDeadzone zone;
    DeadzoneSettings s; s.enabled = true; s.mode = DeadzoneMode::Manual;
    for (int t = 0; t <= 90; ++t) {
        s.manual = t / 100.0;
        for (int i = 0; i <= 1000; ++i) mix(static_cast<float>(zone.map(static_cast<float>(i / 1000.0), s)));
        mix(static_cast<float>(zone.map(std::numeric_limits<float>::denorm_min(), s)));
        mix(static_cast<float>(zone.map(std::nextafter(1.0f, 0.0f), s)));
    }
    return h;
}

static void manualBitExact() {
    const uint64_t digest = manualDigest();
    const uint64_t expected = 0xb8f5ab0f953a0230ull; // v0.1.0 baseline, Frame aarch64 g++ -O0 and -O2
    if (digest != expected) {
        std::printf("manual digest 0x%016llx (expected 0x%016llx)\n",
                    static_cast<unsigned long long>(digest), static_cast<unsigned long long>(expected));
        std::fflush(stdout);
        assert(digest == expected);
    }
    // Spot values that do not depend on the digest.
    FingerDeadzone zone; DeadzoneSettings s; s.enabled = true; s.manual = .2;
    assert(zone.map(.2, s) == 0);
    assert(static_cast<float>(zone.map(.6f, s)) == static_cast<float>((double(.6f) - .2) / .8));
    s.enabled = false; assert(zone.map(.6f, s) == double(.6f));
}

// Bank level: cv publishes a scalar only when it changes, so a held finger
// produces sparse calls. dt is measured from the previous call of the same
// record (learning or not).
using namespace frame_beta; using namespace frame_scalar;
static float received = 0;
static int32_t original(void*, uint64_t, float value, double) { received = value; return 0; }

static void sparseBankLearning() {
    std::array<std::optional<Binding>, 8> input;
    for (unsigned row = 0; row < 8; ++row) {
        unsigned device = row < 4 ? 1 : 2; auto hand = row < 4 ? Hand::Left : Hand::Right;
        std::string serial = row < 4 ? "left" : "right";
        input[row] = Binding{ExistingScalar{(uint64_t(device) << 32) | (row % 4 + 10), 0x100000000ull | device, fingerPaths[row % 4], serial, hand, true, true, 0, false},
                             DeviceSnapshot{device, serial, "frame_controller", hand, true, 0}, 0x4000 + row * 0x100};
    }
    auto receiver = reinterpret_cast<void*>(uintptr_t(0x1234));
    frame_settings::Profile profile; profile.requested = true;
    for (auto& f : profile.fingers) f = {true, false, .2};
    profile.fingers[3] = {true, true, .2};                 // left pinky dynamic
    const auto pinky = input[3]->scalar.component;

    FingerBank bank;
    bank.configure(input, 1000);
    assert(bank.renewStandalone(profile, 1, true, 1000, 1000));
    bank.forward(original, receiver, pinky, .3f, 0, 1001); // witness receiver, retain raw .3
    assert(received == .3f);
    assert(bank.renewStandalone(profile, 1, true, 1010, 1010, 3, 2500));
    assert(bank.status(1010).learned == 0);
    for (uint64_t now = 1300; now < 2500; now += 300) {
        bank.configure(input, now);
        assert(bank.renewStandalone(profile, 1, true, now, now, 3, 2500));
        bank.forward(original, receiver, pinky, .1f, 0, now);
    }
    bank.configure(input, 2500);
    assert(bank.renewStandalone(profile, 1, true, 2500, 2500, 3, 2500));
    auto result=bank.takeLearningResult();
    assert(result&&result->applied&&result->samples==4);
    bank.forward(original, receiver, pinky, .315f, 0, 2501);
    assert(received > .22f && received < .23f); // Old seed .3 + .02 mapped this to zero.

}

int main() {
    firstSampleSeedsBaseline();
    dtWindow();
    inputBounds();
    manualBitExact();
    sparseBankLearning();
    std::puts("PASS characterization: first-sample seed, dt<=0.25 window, input bounds, manual bit-exact digest, sparse vs dense bank learning");
}
