#include "runtime-compat-check.hpp"
#include "../diagnostics/frame-runtime-profile.hpp"
#include <cassert>
#include <cstdio>
using namespace frame_compat_check;

// Synthetic process image. The shipped profile's RVAs are laid out inside it so
// the Known path is exercised with the real constants; nothing here is read
// from a Valve binary.
static constexpr uint64_t B = 0x555500000000ull, Heap = 0x555600000000ull;
static const char* const knownServer = frame_compat::serverSha256;
static const char* const knownCv = frame_compat::cvSha256;
static const std::string otherServer(64, 'a'), otherCv(64, 'b');

static std::array<Profile, 2> profiles() {
    Profile shipped{frame_compat::serverSha256, frame_compat::cvSha256, frame_compat::inputTableRva, {}, {}};
    for (unsigned i = 0; i < 4; ++i) shipped.entryRvas[i] = frame_compat::inputEntries[i];
    for (unsigned i = 0; i < 16; ++i) shipped.fingerprint[i] = frame_compat::scalarFingerprint[i];
    // A second, synthetic profile proves selection is by hash pair.
    Profile synthetic{"1111111111111111111111111111111111111111111111111111111111111111",
                      "2222222222222222222222222222222222222222222222222222222222222222", 0x5b0100,
                      {0x1000, 0x1010, 0x1020, 0x1030}, {}};
    return {shipped, synthetic};
}

static std::vector<Mapping> image() {
    return {
        {B, B + 0x400000, 0, true, false, true, true, serverPath},             // text, file offset 0
        {B + 0x5b0000, B + 0x5d0000, 0x5b0000, true, false, false, true, serverPath},  // relro
        {B + 0x5d0000, B + 0x5e0000, 0x5d0000, true, true, false, true, serverPath},   // data
        {Heap, Heap + 0x100000, 0, true, true, false, true, "[heap]"},
        {0x7f0000000000ull, 0x7f0000100000ull, 0, true, false, true, true, "/usr/lib/libother.so"},
    };
}

static void putBranch(Facts& f, uint32_t instruction) { std::memcpy(f.stub.data() + 12, &instruction, 4); }

static Facts knownFacts() {
    Facts f;
    f.interfaceFound = true;
    f.selfSha256 = f.serverSha256 = knownServer; f.cvSha256 = knownCv;
    f.object = Heap + 0x100;
    f.tableRead = true; f.table = B + frame_compat::inputTableRva;
    f.slotsRead = true;
    for (unsigned i = 0; i < 4; ++i) f.slots[i] = B + frame_compat::inputEntries[i];
    f.stubRead = true;
    for (unsigned i = 0; i < 16; ++i) f.stub[i] = frame_compat::scalarFingerprint[i];
    f.backendRead = true; f.backend = Heap + 0x2000;
    f.maps = image();
    return f;
}

static Facts unknownFacts() {
    auto f = knownFacts();
    f.selfSha256 = f.serverSha256 = otherServer; f.cvSha256 = otherCv;
    // Simulate an update that moved the functions and the table.
    f.table += 0x40;
    for (auto& s : f.slots) s += 0x200;
    f.censusTaken = f.censusStable = true;
    f.fingerCounts.fill(1);
    return f;
}

static Result run(const Facts& f) { return classify(f, profiles()); }

static void expectRejected(Facts f, Reason reason) {
    auto r = run(f);
    if (r.verdict != Verdict::Rejected || r.reason != reason) {
        std::printf("expected %s, got verdict=%d reason=%s\n", reasonName(reason), int(r.verdict), reasonName(r.reason));
        std::fflush(stdout);
    }
    assert(r.verdict == Verdict::Rejected && r.reason == reason);
}

static void parsing() {
    auto m = parseMapsLine("5555a0000000-5555a0400000 r-xp 00000000 103:08 1234567                   /opt/steamvr/bin/linuxarm64/vrserver\n");
    assert(m && m->low == 0x5555a0000000ull && m->high == 0x5555a0400000ull && m->read && !m->write && m->exec && m->priv);
    assert(m->offset == 0 && m->path == serverPath);
    m = parseMapsLine("7f0000000000-7f0000001000 rw-s 0001a000 00:05 42   /dev/shm/with space (deleted)");
    assert(m && m->write && !m->priv && m->offset == 0x1a000 && m->path == "/dev/shm/with space (deleted)");
    m = parseMapsLine("7f0000000000-7f0000001000 ---p 00000000 00:00 0 ");
    assert(m && !m->read && m->path.empty());
    assert(!parseMapsLine(""));
    assert(!parseMapsLine("zz-10 r-xp 0 0:0 0 x"));
    assert(!parseMapsLine("2000-1000 r-xp 00000000 00:00 0 x"));            // high <= low
    assert(!parseMapsLine("1000-2000 rxp 00000000 00:00 0 x"));
    assert(!parseMapsLine("1000-2000 r-xq 00000000 00:00 0 x"));
    assert(!parseMapsLine("10000000000000000-20000000000000000 r-xp 0 0:0 0 x"));  // >64-bit
    Mapping range; range.low = 0x1000; range.high = 0x2000;
    assert(range.contains(0x1ff8, 8) && !range.contains(0x1ff9, 8) && !range.contains(0x0fff, 1) && !range.contains(0x1000, 0));
    auto maps = image();
    assert(moduleBase(maps, serverPath) == B);
    maps.push_back({0x600000000000ull, 0x600000001000ull, 0, true, false, false, true, serverPath});
    assert(!moduleBase(maps, serverPath));                                  // mapped twice
}

static void branches() {
    assert(decodeBranch(0x14000001u, 0x1000) == 0x1004u);
    assert(decodeBranch(0x17ffffffu, 0x1000) == 0x0ffcu);                   // -4
    assert(decodeBranch(0x16000000u, 0x8000000) == 0u);                     // most negative imm26
    assert(!decodeBranch(0x94000001u, 0x1000));                             // BL is not accepted
    assert(!decodeBranch(0x54000001u, 0x1000));                             // B.cond is not accepted
    assert(!decodeBranch(0xd61f0000u, 0x1000));                             // BR x0
}

static void knownPath() {
    auto r = run(knownFacts());
    assert(r.verdict == Verdict::Known && r.reason == Reason::None && r.profile == 0);
    assert(r.structural == Reason::None && r.entry == B + frame_compat::inputEntries[3]);
    assert(r.branchTarget > r.entry + 16 && r.branchTarget < B + 0x400000);
    // Census is not part of the v0.1.0 gate.
    auto f = knownFacts(); f.censusTaken = false; assert(run(f).verdict == Verdict::Known);
    // Extra structural findings are diagnostics only on the Known path.
    f = knownFacts(); f.maps[3].write = false;  // backend no longer in a writable mapping
    r = run(f); assert(r.verdict == Verdict::Known && r.structural == Reason::BackendNotHeap);
    // A shipped hash pair never falls back to the structural path.
    f = knownFacts(); f.table += 8; expectRejected(f, Reason::KnownHashLayoutMismatch);
    f = knownFacts(); f.slots[0] += 4; expectRejected(f, Reason::KnownHashLayoutMismatch);
    f = knownFacts(); f.stub[15] ^= 1; expectRejected(f, Reason::KnownHashFingerprintMismatch);
    f = knownFacts(); f.censusTaken = f.censusStable = true; f.fingerCounts.fill(1); f.table += 8;
    expectRejected(f, Reason::KnownHashLayoutMismatch);
    f = knownFacts(); f.selfSha256 = otherServer; expectRejected(f, Reason::SelfNotServer);
    f = knownFacts(); f.cvSha256 = otherCv; f.censusTaken = false;
    expectRejected(f, Reason::CensusMissing);  // unknown pair -> structural path, which needs the census
    f = knownFacts(); f.maps[1].write = true; expectRejected(f, Reason::SlotPageNotReadOnly);
    f = knownFacts(); f.backend = 0; expectRejected(f, Reason::BackendUnreadable);
    f = knownFacts(); f.maps.push_back({0x600000000000ull, 0x600000001000ull, 0, true, false, false, true, serverPath});
    expectRejected(f, Reason::ServerBaseAmbiguous);
    f = knownFacts(); f.slots[2] = 0x7f0000000100ull; expectRejected(f, Reason::SlotNotInServerText);
    f = knownFacts(); f.interfaceFound = false; expectRejected(f, Reason::InterfaceMissing);
}

static void structuralPath() {
    auto r = run(unknownFacts());
    assert(r.verdict == Verdict::StructurallyCompatible && r.reason == Reason::None && r.profile == -1);
    auto f = unknownFacts();
    putBranch(f, 0x14000000u | 0x100u);  // any in-text target is fine
    assert(run(f).verdict == Verdict::StructurallyCompatible);

    // Census.
    f = unknownFacts(); f.censusTaken = false; expectRejected(f, Reason::CensusMissing);
    f = unknownFacts(); f.censusStable = false; expectRejected(f, Reason::CensusUnstable);
    f = unknownFacts(); f.fingerCounts[6] = 0; expectRejected(f, Reason::FingerMissing);
    assert(run(f).fingersMissing == (1u << 6));
    f = unknownFacts(); f.fingerCounts[1] = 2; f.fingerCounts[5] = 0; expectRejected(f, Reason::FingerDuplicate);
    r = run(f); assert(r.fingersDuplicate == (1u << 1) && r.fingersMissing == (1u << 5));
    f = unknownFacts(); f.fingerCounts.fill(0); expectRejected(f, Reason::FingerMissing);

    // Identity and table.
    f = unknownFacts(); f.interfaceFound = false; expectRejected(f, Reason::InterfaceMissing);
    f = unknownFacts(); f.object = 0; expectRejected(f, Reason::InterfaceMissing);
    f = unknownFacts(); f.selfSha256 = std::string(64, 'c'); expectRejected(f, Reason::SelfNotServer);
    f = unknownFacts(); f.selfSha256 = f.serverSha256 = "short"; expectRejected(f, Reason::SelfNotServer);
    f = unknownFacts(); f.selfSha256 = f.serverSha256 = std::string(64, 'A'); expectRejected(f, Reason::SelfNotServer);
    f = unknownFacts(); f.tableRead = false; expectRejected(f, Reason::TableUnreadable);
    f = unknownFacts(); f.table += 4; expectRejected(f, Reason::TableMisaligned);
    f = unknownFacts(); f.table = Heap + 0x800; expectRejected(f, Reason::TableNotInServer);
    f = unknownFacts(); f.table = B + 0x5cfff0; expectRejected(f, Reason::TableNotInServer);  // runs off the mapping
    f = unknownFacts(); f.maps[1].write = true; expectRejected(f, Reason::SlotPageNotReadOnly);
    f = unknownFacts(); f.maps[1].exec = true; expectRejected(f, Reason::SlotPageNotReadOnly);
    f = unknownFacts(); f.maps[1].priv = false; expectRejected(f, Reason::SlotPageNotReadOnly);
    f = unknownFacts(); f.table = B + 0x5d0000; expectRejected(f, Reason::SlotPageNotReadOnly);  // .data

    // Slot entries.
    f = unknownFacts(); f.slotsRead = false; expectRejected(f, Reason::SlotsUnreadable);
    f = unknownFacts(); f.slots[1] = 0; expectRejected(f, Reason::SlotMisaligned);
    f = unknownFacts(); f.slots[2] += 2; expectRejected(f, Reason::SlotMisaligned);
    f = unknownFacts(); f.slots[0] = 0x7f0000000100ull; expectRejected(f, Reason::SlotNotInServerText);
    f = unknownFacts(); f.slots[3] = B + 0x5b0100; expectRejected(f, Reason::SlotNotInServerText);  // non-exec
    f = unknownFacts(); f.slots[3] = Heap + 0x100; expectRejected(f, Reason::SlotNotInServerText);

    // Stub shape and branch target.
    f = unknownFacts(); f.stubRead = false; expectRejected(f, Reason::StubUnreadable);
    f = unknownFacts(); f.slots[3] = B + 0x3ffff8; expectRejected(f, Reason::StubUnreadable);  // 16 bytes cross the end
    for (unsigned i = 0; i < 12; ++i) { f = unknownFacts(); f.stub[i] ^= 0x01; expectRejected(f, Reason::StubPrefixMismatch); }
    f = unknownFacts(); putBranch(f, 0x94000100u); expectRejected(f, Reason::StubNotBranch);   // BL
    f = unknownFacts(); putBranch(f, 0xd61f0000u); expectRejected(f, Reason::StubNotBranch);   // BR
    f = unknownFacts(); putBranch(f, 0x14000000u); expectRejected(f, Reason::BranchTargetIntoStub);  // b .
    f = unknownFacts(); putBranch(f, 0x17fffffdu); expectRejected(f, Reason::BranchTargetIntoStub);  // back to entry
    f = unknownFacts(); putBranch(f, 0x15ffffffu); expectRejected(f, Reason::BranchTargetNotInServerText);  // far forward
    f = unknownFacts(); f.slots[3] = B + 0x3fff00; putBranch(f, 0x14000100u);
    expectRejected(f, Reason::BranchTargetNotInServerText);  // into relro gap / non-exec
    // Backend field.
    f = unknownFacts(); f.backendRead = false; expectRejected(f, Reason::BackendUnreadable);
    f = unknownFacts(); f.backend = 0; expectRejected(f, Reason::BackendNotHeap);
    f = unknownFacts(); f.backend += 4; expectRejected(f, Reason::BackendNotHeap);
    f = unknownFacts(); f.backend = B + 0x100; expectRejected(f, Reason::BackendNotHeap);  // text
    f = unknownFacts(); f.backend = 0x100; expectRejected(f, Reason::BackendNotHeap);      // unmapped

    // Empty maps reject before anything else is trusted.
    f = unknownFacts(); f.maps.clear(); expectRejected(f, Reason::TableNotInServer);
}

static void synthetic() {
    // Selection is by the full hash pair; a second profile is independent.
    auto f = unknownFacts();
    f.selfSha256 = f.serverSha256 = profiles()[1].serverSha256; f.cvSha256 = profiles()[1].cvSha256;
    auto r = run(f);
    assert(r.verdict == Verdict::Rejected && r.profile == 1 && r.reason == Reason::KnownHashLayoutMismatch);
}

int main() {
    parsing();
    branches();
    knownPath();
    structuralPath();
    synthetic();
    for (int r = 0; r <= int(Reason::FingerDuplicate); ++r) assert(std::string(reasonName(Reason(r))) != "unknown");
    std::puts("PASS runtime compat: maps parse, B decode, known gate unchanged, structural rejection matrix, census, profile selection");
}
