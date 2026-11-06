// Linux-only: exercises the real collectors against this test process.
#include "runtime-facts.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
using namespace frame_compat_check;
using namespace frame_runtime_facts;

static int a() { return 1; }
static int b() { return 2; }
static int c() { return 3; }
static int d() { return 4; }
using Fn = int (*)();
// Needs relocation, so a PIE places it in .data.rel.ro (r--p after RELRO).
static Fn const table[4] = {a, b, c, d};
struct Object { const Fn* vptr; uint64_t field8; uint64_t backend; };

int main() {
    // sha256sum parsing.
    const std::string h1(64, '1'), h2(64, '2'), h3(64, '3');
    auto parsed = parseSha256sum(h1 + "  /proc/7/exe\n" + h2 + "  " + serverPath + "\n" + h3 + " *" + cvPath + "\n", "/proc/7/exe");
    assert(parsed.self == h1 && parsed.server == h2 && parsed.cv == h3);
    parsed = parseSha256sum(h2 + "  /other\nnot a line\n" + std::string(64, 'Z') + "  " + serverPath + "\n", "/proc/7/exe");
    assert(parsed.self.empty() && parsed.server.empty() && parsed.cv.empty());
    parsed = parseSha256sum(h1 + "  " + serverPath + "\n" + h2 + "  " + serverPath + "\n", "x");
    assert(parsed.server == h1);  // first line wins

    // /proc/self/maps: our own executable is mapped and the table is read-only.
    auto maps = readSelfMaps();
    assert(!maps.empty());
    char exe[PATH_MAX]{};
    auto length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    assert(length > 0);
    const auto* text = findMapping(maps, reinterpret_cast<uint64_t>(&a), 4);
    assert(text && text->exec && !text->write && text->path == exe);
    const auto* relro = findMapping(maps, reinterpret_cast<uint64_t>(&table[3]), 8);
    assert(relro && relro->read && !relro->write && relro->path == exe);
    assert(moduleBase(maps, exe));

    // Structure reader against a fake object.
    static uint64_t heapish = 0;
    Object object{table, 7, reinterpret_cast<uint64_t>(&heapish)};
    Facts f;
    readStructure(&object, f);
    assert(f.interfaceFound && f.tableRead && f.table == reinterpret_cast<uint64_t>(table));
    assert(f.slotsRead && f.slots[0] == reinterpret_cast<uint64_t>(&a) && f.slots[3] == reinterpret_cast<uint64_t>(&d));
    assert(f.stubRead && !std::memcmp(f.stub.data(), reinterpret_cast<const void*>(&d), 16));
    assert(f.backendRead && f.backend == reinterpret_cast<uint64_t>(&heapish));
    // Not vrserver: rejected, and the census is never reached.
    f.selfSha256 = f.serverSha256 = std::string(64, 'e');
    std::array<Profile, 0> none{};
    auto r = classify(f, none);
    assert(r.verdict == Verdict::Rejected && r.reason == Reason::TableNotInServer);

    // Unreadable pointers fail closed instead of faulting.
    Object broken{reinterpret_cast<const Fn*>(uintptr_t(0x10)), 0, 0};
    Facts g; readStructure(&broken, g);
    assert(g.tableRead && !g.slotsRead && !g.stubRead);
    Facts nullFacts; readStructure(nullptr, nullFacts);
    assert(!nullFacts.interfaceFound && !nullFacts.tableRead);

    // Log line formatting (pure).
    Facts fake; fake.maps = {{0x10000, 0x20000, 0, true, false, true, true, serverPath}};
    fake.table = 0x10800; fake.slotsRead = true; fake.slots = {0x11000, 0x10ff0, 0x10fc0, 0x10fb0};
    Result result; result.verdict = Verdict::Rejected; result.reason = Reason::FingerMissing;
    result.entry = 0x10fb0; result.branchTarget = 0x18000; result.fingersMissing = 0x81;
    auto line = formatVerdict("unknown_build", result, fake, 12);
    assert(line.find("FrameCompat: unknown_build verdict=rejected reason=finger_missing structural=none") == 0);
    assert(line.find("table_rva=0x800 entry_rva=0xfb0 target_rva=0x8000 slot_delta=-16,-48,-16") != std::string::npos);
    assert(line.find("fingers_missing=0x81 fingers_duplicate=0x00; classifier read-only") != std::string::npos);
    fake.maps.clear();
    assert(formatVerdict("x", result, fake, 1).find("table_rva=0x0 entry_rva=0x0") != std::string::npos);
    assert(shippedProfiles()[0].tableRva == frame_compat::inputTableRva);

    // Real hashing of our own executable.
    auto hashes = hashRuntime();
    assert(sha256Text(hashes.self));
    std::printf("PASS runtime facts: sha256sum parse, self maps, relro table, structure reads, fail-closed reads (server hash %s)\n",
                hashes.server.empty() ? "absent" : "present");
}
