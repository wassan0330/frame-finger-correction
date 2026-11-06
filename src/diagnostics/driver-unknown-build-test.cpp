// test-flags: -DFRAME_FINGER_RELEASE -DFRAME_NOV_BETA_CANDIDATE -DFRAME_SCALAR_ALL_FINGERS_BETA -DFRAME_SCALAR_CONTINUOUS_SHADOW -DFRAME_SCALAR_CANDIDATE_SHADOW -DFRAME_SCALAR_OFF_VTABLE_PROBE -DFRAME_SCALAR_STANDALONE_BETA -DFRAME_COMPAT_STRUCTURAL_AUDIT -Wno-unused-parameter -ldl
// Drives the real release driver (built with the structural audit) through a
// fake driver context. This process is not vrserver, so it stands in for an
// unknown SteamVR build: the provider must log exactly one read-only verdict,
// never touch the input table and never arm any observer/correction path.
#include "frame-scalar-audit-driver.cpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static std::vector<std::string> logs;
struct FakeLog final : vr::IVRDriverLog { void Log(const char* text) override { logs.emplace_back(text); } };
struct Props final : vr::IVRProperties {
    unsigned reads = 0;
    vr::ETrackedPropertyError ReadPropertyBatch(vr::PropertyContainerHandle_t, vr::PropertyRead_t*, uint32_t) override { ++reads; return vr::TrackedProp_InvalidDevice; }
    vr::ETrackedPropertyError WritePropertyBatch(vr::PropertyContainerHandle_t, vr::PropertyWrite_t*, uint32_t) override { assert(!"no writes"); return vr::TrackedProp_InvalidDevice; }
    const char* GetPropErrorNameFromEnum(vr::ETrackedPropertyError) override { return "x"; }
    vr::PropertyContainerHandle_t TrackedDeviceToPropertyContainer(vr::TrackedDeviceIndex_t index) override { return (uint64_t{1} << 32) | index; }
};
static unsigned calls = 0;
static int32_t slot0(void*) { ++calls; return 0; }
static int32_t slot1(void*) { ++calls; return 0; }
static int32_t slot2(void*) { ++calls; return 0; }
static int32_t slot3(void*, uint64_t, float, double) { ++calls; return 0; }
static void* const inputTable[4] = {reinterpret_cast<void*>(&slot0), reinterpret_cast<void*>(&slot1),
                                    reinterpret_cast<void*>(&slot2), reinterpret_cast<void*>(&slot3)};
struct FakeInput { void* const* vptr; uint64_t field8; uint64_t backend; };
static uint64_t backendStorage[64]{};
static FakeInput input{inputTable, 0, reinterpret_cast<uint64_t>(backendStorage)};

struct Context final : vr::IVRDriverContext {
    FakeLog log; Props props;
    void* GetGenericInterface(const char* version, vr::EVRInitError* error) override {
        if (error) *error = vr::VRInitError_None;
        if (!std::strcmp(version, vr::IVRDriverLog_Version)) return &log;
        if (!std::strcmp(version, "IVRDriverInput_005")) return &input;
        if (!std::strcmp(version, vr::IVRProperties_Version)) return &props;
        if (error) *error = vr::VRInitError_Init_InterfaceNotFound;  // no server host
        return nullptr;
    }
    vr::DriverHandle_t GetDriverHandle() override { return 1; }
};

static unsigned count(const char* needle) {
    unsigned n = 0; for (auto& l : logs) if (l.find(needle) != std::string::npos) ++n; return n;
}

int main() {
    int error = -1;
    auto* provider = static_cast<vr::IServerTrackedDeviceProvider*>(HmdDriverFactory(vr::IServerTrackedDeviceProvider_Version, &error));
    assert(provider && error == vr::VRInitError_None);
    Context context;
    assert(provider->Init(&context) == vr::VRInitError_None);
    assert(count("unknown layout/owner; audit inactive; correction OFF") == 1);
    assert(count("FrameCompat: layout not known; read-only structural audit scheduled") == 1);
    assert(count("capable but DISARMED") == 0);  // known-only arming message never appears
    for (int i = 0; i < 30; ++i) {  // spans several 1 s ticks
        provider->RunFrame();
        struct timespec pause{0, 50 * 1000 * 1000}; nanosleep(&pause, nullptr);
    }
    if (count("FrameCompat: unknown_build") != 1) for (auto& l : logs) std::puts(l.c_str());
    assert(count("FrameCompat: unknown_build verdict=rejected reason=self_not_server") == 1);
    assert(count("; classifier read-only; no hooks; correction OFF") == 1);
    assert(count("FrameScalarOffProbe: attached") == 0 && count("FrameFingerBeta:") == 0 && count("FrameScalarAudit: snapshot") == 0);
    assert(inputTable[3] == reinterpret_cast<void*>(&slot3) && calls == 0);  // table untouched, nothing called
    assert(context.props.reads == 0);                                          // census never reached
    for (auto& v : backendStorage) assert(v == 0);
    provider->Cleanup();
    const auto before = logs.size();
    provider->RunFrame();
    assert(logs.size() == before);
    // A second Init (new session) audits again from scratch.
    assert(provider->Init(&context) == vr::VRInitError_None);
    for (int i = 0; i < 3; ++i) provider->RunFrame();
    assert(count("FrameCompat: unknown_build") == 2);
    provider->Cleanup();
    std::puts("PASS release driver with structural audit on an unknown build: one read-only verdict, table untouched, nothing armed, no census before structural pass");
}
