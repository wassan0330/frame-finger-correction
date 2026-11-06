#pragma once
// Pure, read-only compatibility classifier for the IVRDriverInput_005 scalar
// slot. It only judges facts that the caller has already gathered with the
// bounded self-memory reader, /proc/self/maps and sha256; it performs no reads,
// writes or calls itself, so every path is unit-testable off-device.
//
// Verdicts:
//   Known                   exact match with a shipped profile, using ONLY the
//                           conditions v0.1.0 already enforced. Additional
//                           structural findings are reported as diagnostics and
//                           never change a Known verdict.
//   StructurallyCompatible  the hashes are not a shipped profile, but every
//                           structural check passed. This authorizes nothing by
//                           itself: observation and correction stay separate,
//                           explicit, later steps.
//   Rejected                anything else, with the first failing reason.
// A similar instruction pattern alone is never enough for StructurallyCompatible.
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace frame_compat_check {

inline constexpr const char* serverPath = "/opt/steamvr/bin/linuxarm64/vrserver";
inline constexpr unsigned scalarSlot = 3, slotCount = 4, fingerRows = 8;
// Forwarding stub prefix shared by the shipped builds: mov x2,x1; ldr w1,[x0,#8];
// ldr x0,[x0,#16]. The fourth instruction must be an unconditional B whose
// target is re-validated. The #16 load is the same backend field the snapshot
// decoder follows, which is why the prefix is required byte-for-byte.
inline constexpr std::array<uint8_t, 12> stubPrefix{
    0xe2, 0x03, 0x01, 0xaa, 0x01, 0x08, 0x40, 0xb9, 0x00, 0x08, 0x40, 0xf9};
inline constexpr uint64_t backendOffset = 16;

struct Mapping {
    uint64_t low = 0, high = 0, offset = 0;
    bool read = false, write = false, exec = false, priv = false;
    std::string path;
    bool contains(uint64_t address, uint64_t size) const {
        return size && address >= low && address < high && high - address >= size;
    }
};

// One line of /proc/<pid>/maps. Returns nullopt on anything malformed.
inline std::optional<Mapping> parseMapsLine(const std::string& line) {
    Mapping m;
    size_t pos = 0;
    auto hex = [&](uint64_t& out, char stop) {
        size_t start = pos; out = 0;
        while (pos < line.size() && line[pos] != stop) {
            char c = line[pos]; unsigned digit;
            if (c >= '0' && c <= '9') digit = unsigned(c - '0');
            else if (c >= 'a' && c <= 'f') digit = unsigned(c - 'a' + 10);
            else return false;
            if (out >> 60) return false;
            out = out << 4 | digit; ++pos;
        }
        if (pos == start || pos - start > 16 || pos >= line.size()) return false;
        ++pos; return true;
    };
    if (!hex(m.low, '-') || !hex(m.high, ' ') || m.high <= m.low) return std::nullopt;
    if (line.size() < pos + 5 || line[pos + 4] != ' ') return std::nullopt;
    const char* p = line.c_str() + pos;
    if ((p[0] != 'r' && p[0] != '-') || (p[1] != 'w' && p[1] != '-') || (p[2] != 'x' && p[2] != '-') ||
        (p[3] != 'p' && p[3] != 's')) return std::nullopt;
    m.read = p[0] == 'r'; m.write = p[1] == 'w'; m.exec = p[2] == 'x'; m.priv = p[3] == 'p';
    pos += 5;
    if (!hex(m.offset, ' ')) return std::nullopt;
    // device and inode
    size_t fields = 0;
    while (fields < 2) {
        size_t start = pos;
        while (pos < line.size() && line[pos] != ' ' && line[pos] != '\n') ++pos;
        if (pos == start) return std::nullopt;
        ++fields;
        while (pos < line.size() && line[pos] == ' ') ++pos;
    }
    size_t end = line.find('\n', pos);
    m.path = line.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    return m;
}

inline const Mapping* findMapping(const std::vector<Mapping>& maps, uint64_t address, uint64_t size) {
    for (const auto& m : maps) if (m.contains(address, size)) return &m;
    return nullptr;
}

// Load base of a module: start of its file-offset-0 mapping (dladdr's dli_fbase for PIE).
inline std::optional<uint64_t> moduleBase(const std::vector<Mapping>& maps, const std::string& path) {
    std::optional<uint64_t> base;
    for (const auto& m : maps)
        if (m.path == path && m.offset == 0) {
            if (base) return std::nullopt;   // mapped twice: ambiguous
            base = m.low;
        }
    return base;
}

// AArch64 unconditional immediate branch (B, not BL). Returns the target.
inline std::optional<uint64_t> decodeBranch(uint32_t instruction, uint64_t pc) {
    if ((instruction & 0xfc000000u) != 0x14000000u) return std::nullopt;
    int64_t imm = int64_t(instruction & 0x03ffffffu);
    if (imm & 0x02000000) imm -= 0x04000000;
    return pc + uint64_t(imm * 4);
}

struct Profile {
    const char* serverSha256;
    const char* cvSha256;
    uint64_t tableRva;
    std::array<uint64_t, slotCount> entryRvas;
    std::array<uint8_t, 16> fingerprint;
};

struct Facts {
    bool interfaceFound = false;
    std::string selfSha256, serverSha256, cvSha256;  // sha256sum of /proc/self/exe, vrserver, driver_cv
    uint64_t object = 0;                              // live IVRDriverInput_005 object
    bool tableRead = false; uint64_t table = 0;       // *(object)
    bool slotsRead = false; std::array<uint64_t, slotCount> slots{};  // table[0..3]
    bool stubRead = false; std::array<uint8_t, 16> stub{};            // 16 bytes at slots[3]
    bool backendRead = false; uint64_t backend = 0;   // *(object+16)
    std::vector<Mapping> maps;                        // parsed /proc/self/maps
    // Census from stableSnapshot over connected frame controllers: count of
    // scalar components per (side*4+finger) with units==0, and whether every
    // contributing snapshot was stable and owner-consistent.
    bool censusTaken = false, censusStable = false;
    std::array<unsigned, fingerRows> fingerCounts{};
};

enum class Verdict : uint8_t { Known, StructurallyCompatible, Rejected };
enum class Reason : uint8_t {
    None,
    InterfaceMissing, SelfNotServer, TableUnreadable, TableMisaligned, TableNotInServer,
    SlotPageNotReadOnly, SlotsUnreadable, SlotNotInServerText, SlotMisaligned,
    StubUnreadable, StubPrefixMismatch, StubNotBranch, BranchTargetMisaligned,
    BranchTargetNotInServerText, BranchTargetIntoStub, BackendUnreadable, BackendNotHeap,
    ServerBaseAmbiguous, KnownHashLayoutMismatch, KnownHashFingerprintMismatch,
    CensusMissing, CensusUnstable, FingerMissing, FingerDuplicate,
};

inline const char* reasonName(Reason r) {
    switch (r) {
    case Reason::None: return "none";
    case Reason::InterfaceMissing: return "interface_missing";
    case Reason::SelfNotServer: return "self_not_server";
    case Reason::TableUnreadable: return "table_unreadable";
    case Reason::TableMisaligned: return "table_misaligned";
    case Reason::TableNotInServer: return "table_not_in_server";
    case Reason::SlotPageNotReadOnly: return "slot_page_not_readonly";
    case Reason::SlotsUnreadable: return "slots_unreadable";
    case Reason::SlotNotInServerText: return "slot_not_in_server_text";
    case Reason::SlotMisaligned: return "slot_misaligned";
    case Reason::StubUnreadable: return "stub_unreadable";
    case Reason::StubPrefixMismatch: return "stub_prefix_mismatch";
    case Reason::StubNotBranch: return "stub_not_branch";
    case Reason::BranchTargetMisaligned: return "branch_target_misaligned";
    case Reason::BranchTargetNotInServerText: return "branch_target_not_in_server_text";
    case Reason::BranchTargetIntoStub: return "branch_target_into_stub";
    case Reason::BackendUnreadable: return "backend_unreadable";
    case Reason::BackendNotHeap: return "backend_not_heap";
    case Reason::ServerBaseAmbiguous: return "server_base_ambiguous";
    case Reason::KnownHashLayoutMismatch: return "known_hash_layout_mismatch";
    case Reason::KnownHashFingerprintMismatch: return "known_hash_fingerprint_mismatch";
    case Reason::CensusMissing: return "census_missing";
    case Reason::CensusUnstable: return "census_unstable";
    case Reason::FingerMissing: return "finger_missing";
    case Reason::FingerDuplicate: return "finger_duplicate";
    }
    return "unknown";
}

struct Result {
    Verdict verdict = Verdict::Rejected;
    Reason reason = Reason::None;        // first failing check for Rejected
    Reason structural = Reason::None;    // first structural finding (diagnostic for Known)
    int profile = -1;                    // index of the matched shipped profile
    uint64_t entry = 0, branchTarget = 0;
    unsigned fingersMissing = 0, fingersDuplicate = 0;  // bit per row
};

inline bool sha256Text(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

// Structural checks shared by both paths. Returns the first failure.
inline Reason structuralCheck(const Facts& f, Result& out) {
    if (!f.interfaceFound || !f.object) return Reason::InterfaceMissing;
    if (!sha256Text(f.selfSha256) || f.selfSha256 != f.serverSha256) return Reason::SelfNotServer;
    if (!f.tableRead || !f.table) return Reason::TableUnreadable;
    if (f.table % 8) return Reason::TableMisaligned;
    const auto* tableMap = findMapping(f.maps, f.table, 8 * slotCount);
    if (!tableMap || tableMap->path != serverPath) return Reason::TableNotInServer;
    const auto* slotMap = findMapping(f.maps, f.table + 8 * scalarSlot, 8);
    if (!slotMap || !slotMap->read || slotMap->write || slotMap->exec || !slotMap->priv) return Reason::SlotPageNotReadOnly;
    if (!f.slotsRead) return Reason::SlotsUnreadable;
    for (auto slot : f.slots) {
        if (!slot || slot % 4) return Reason::SlotMisaligned;
        const auto* m = findMapping(f.maps, slot, 4);
        if (!m || m->path != serverPath || !m->exec || m->write) return Reason::SlotNotInServerText;
    }
    out.entry = f.slots[scalarSlot];
    if (!f.stubRead || !findMapping(f.maps, out.entry, 16)) return Reason::StubUnreadable;
    if (std::memcmp(f.stub.data(), stubPrefix.data(), stubPrefix.size())) return Reason::StubPrefixMismatch;
    uint32_t branch; std::memcpy(&branch, f.stub.data() + 12, 4);
    auto target = decodeBranch(branch, out.entry + 12);
    if (!target) return Reason::StubNotBranch;
    out.branchTarget = *target;
    if (*target % 4) return Reason::BranchTargetMisaligned;
    if (*target >= out.entry && *target < out.entry + 16) return Reason::BranchTargetIntoStub;
    const auto* targetMap = findMapping(f.maps, *target, 4);
    if (!targetMap || targetMap->path != serverPath || !targetMap->exec || targetMap->write) return Reason::BranchTargetNotInServerText;
    if (!f.backendRead) return Reason::BackendUnreadable;
    const auto* heap = findMapping(f.maps, f.backend, 8);
    if (!f.backend || f.backend % 8 || !heap || !heap->read || !heap->write || heap->exec) return Reason::BackendNotHeap;
    return Reason::None;
}

inline Reason censusCheck(const Facts& f, Result& out) {
    if (!f.censusTaken) return Reason::CensusMissing;
    if (!f.censusStable) return Reason::CensusUnstable;
    for (unsigned row = 0; row < fingerRows; ++row) {
        if (f.fingerCounts[row] == 0) out.fingersMissing |= 1u << row;
        if (f.fingerCounts[row] > 1) out.fingersDuplicate |= 1u << row;
    }
    if (out.fingersDuplicate) return Reason::FingerDuplicate;
    if (out.fingersMissing) return Reason::FingerMissing;
    return Reason::None;
}

// The v0.1.0 gate, unchanged in substance: exact server+cv hashes, self is the
// server, slot entries at the profile RVAs from the server's load base, table at
// the profile RVA, exact 16-byte fingerprint, slot page r--p, backend non-null.
inline Reason knownCheck(const Facts& f, const Profile& p) {
    if (!f.interfaceFound || !f.object) return Reason::InterfaceMissing;
    if (f.selfSha256 != p.serverSha256) return Reason::SelfNotServer;
    if (!f.tableRead || !f.table) return Reason::TableUnreadable;
    auto base = moduleBase(f.maps, serverPath);
    if (!base) return Reason::ServerBaseAmbiguous;
    if (!f.slotsRead) return Reason::SlotsUnreadable;
    for (unsigned i = 0; i < slotCount; ++i) {
        const auto* m = findMapping(f.maps, f.slots[i], 1);
        if (!m || m->path != serverPath) return Reason::SlotNotInServerText;
        if (f.slots[i] - *base != p.entryRvas[i]) return Reason::KnownHashLayoutMismatch;
    }
    if (f.table - *base != p.tableRva) return Reason::KnownHashLayoutMismatch;
    if (!f.stubRead) return Reason::StubUnreadable;
    if (std::memcmp(f.stub.data(), p.fingerprint.data(), 16)) return Reason::KnownHashFingerprintMismatch;
    const auto* slotMap = findMapping(f.maps, f.table + 8 * scalarSlot, 8);
    if (!slotMap || !slotMap->read || slotMap->write || slotMap->exec || !slotMap->priv) return Reason::SlotPageNotReadOnly;
    if (!f.backendRead || !f.backend) return Reason::BackendUnreadable;
    return Reason::None;
}

template <size_t N>
inline Result classify(const Facts& f, const std::array<Profile, N>& profiles) {
    Result out;
    out.structural = structuralCheck(f, out);
    for (size_t i = 0; i < N; ++i) {
        if (f.serverSha256 != profiles[i].serverSha256 || f.cvSha256 != profiles[i].cvSha256) continue;
        // A shipped hash pair decides the outcome: never fall back to the
        // structural path for a binary we claim to know.
        out.profile = int(i);
        out.reason = knownCheck(f, profiles[i]);
        out.verdict = out.reason == Reason::None ? Verdict::Known : Verdict::Rejected;
        return out;
    }
    if (out.structural != Reason::None) { out.reason = out.structural; return out; }
    out.reason = censusCheck(f, out);
    out.verdict = out.reason == Reason::None ? Verdict::StructurallyCompatible : Verdict::Rejected;
    return out;
}

}  // namespace frame_compat_check
