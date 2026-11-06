#pragma once
// Gathers the read-only facts that frame_compat_check::classify judges.
// Everything here reads this process only: /proc/self/maps, the bounded
// SelfMemoryReader (process_vm_readv; unmapped reads fail instead of faulting)
// and sha256sum of fixed paths. Nothing is written and no runtime function is
// called. The census only follows the backend pointer after the structural
// checks that vouch for it have passed.
#include "../correction/runtime-compat-check.hpp"
#include "frame-runtime-profile.hpp"
#include "self-memory-reader.hpp"
#include <utility>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

namespace frame_runtime_facts {

inline constexpr const char* cvPath = "/opt/steamvr/drivers/cv/bin/linuxarm64/driver_cv.so";

inline std::vector<frame_compat_check::Mapping> readSelfMaps(size_t limit = 8192) {
    std::vector<frame_compat_check::Mapping> out;
    FILE* maps = std::fopen("/proc/self/maps", "r");
    if (!maps) return out;
    char line[1024]{};
    while (out.size() < limit && std::fgets(line, sizeof(line), maps)) {
        auto m = frame_compat_check::parseMapsLine(line);
        if (!m) { out.clear(); break; }   // a malformed line makes the whole view untrusted
        out.push_back(std::move(*m));
    }
    std::fclose(maps);
    return out;
}

struct Hashes { std::string self, server, cv; };

// Parses "sha256sum" output for the three fixed inputs, matched by path.
inline Hashes parseSha256sum(const std::string& output, const std::string& selfPath) {
    Hashes h;
    size_t start = 0;
    while (start < output.size()) {
        size_t end = output.find('\n', start);
        std::string line = output.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? output.size() : end + 1;
        if (line.size() < 67 || line[64] != ' ' || (line[65] != ' ' && line[65] != '*')) continue;
        std::string digest = line.substr(0, 64), path = line.substr(66);
        if (!frame_compat_check::sha256Text(digest)) continue;
        if (path == selfPath && h.self.empty()) h.self = digest;
        else if (path == frame_compat_check::serverPath && h.server.empty()) h.server = digest;
        else if (path == cvPath && h.cv.empty()) h.cv = digest;
    }
    return h;
}

// Fixed executable and numeric self pid only, like the v0.1.0 check.
inline Hashes hashRuntime() {
    char self[64]{}, command[384]{};
    std::snprintf(self, sizeof(self), "/proc/%ld/exe", static_cast<long>(getpid()));
    std::snprintf(command, sizeof(command), "/usr/bin/sha256sum %s %s %s 2>/dev/null", self,
                  frame_compat_check::serverPath, cvPath);
    FILE* pipe = popen(command, "r");
    if (!pipe) return {};
    std::string output; char buffer[512]{};
    while (output.size() < 4096 && std::fgets(buffer, sizeof(buffer), pipe)) output += buffer;
    pclose(pipe);  // a missing file only leaves that digest empty
    return parseSha256sum(output, self);
}

// Fills everything except hashes and the census.
inline void readStructure(void* input, frame_compat_check::Facts& f) {
    using namespace frame_compat_check;
    f.interfaceFound = input != nullptr;
    f.object = reinterpret_cast<uint64_t>(input);
    f.maps = readSelfMaps();
    if (!f.object) return;
    SelfMemoryReader reader;
    f.tableRead = reader.read(f.object, &f.table, 8);
    if (f.tableRead && f.table && f.table % 8 == 0)
        f.slotsRead = reader.read(f.table, f.slots.data(), 8 * slotCount);
    if (f.slotsRead && f.slots[scalarSlot])
        f.stubRead = reader.read(f.slots[scalarSlot], f.stub.data(), f.stub.size());
    f.backendRead = reader.read(f.object + backendOffset, &f.backend, 8);
}

// The profile this binary was built for (one per build, as in v0.1.0).
inline std::array<frame_compat_check::Profile, 1> shippedProfiles() {
    frame_compat_check::Profile p{frame_compat::serverSha256, frame_compat::cvSha256, frame_compat::inputTableRva, {}, {}};
    for (unsigned i = 0; i < frame_compat_check::slotCount; ++i) p.entryRvas[i] = frame_compat::inputEntries[i];
    for (unsigned i = 0; i < 16; ++i) p.fingerprint[i] = frame_compat::scalarFingerprint[i];
    return {p};
}

inline const char* verdictName(frame_compat_check::Verdict v) {
    switch (v) {
    case frame_compat_check::Verdict::Known: return "known";
    case frame_compat_check::Verdict::StructurallyCompatible: return "structurally_compatible";
    case frame_compat_check::Verdict::Rejected: return "rejected";
    }
    return "invalid";
}

// One diagnostic log line. RVAs are relative to the server load base when it
// is unambiguous; slot spacing is reported for diagnosis only.
inline std::string formatVerdict(const char* context, const frame_compat_check::Result& r,
                                 const frame_compat_check::Facts& f, unsigned attempts) {
    using namespace frame_compat_check;
    auto base = moduleBase(f.maps, serverPath);
    auto rva = [&](uint64_t address) -> unsigned long long {
        return base && address >= *base ? static_cast<unsigned long long>(address - *base) : 0ull;
    };
    long long stride[3]{};
    if (f.slotsRead) for (unsigned i = 0; i < 3; ++i) stride[i] = static_cast<long long>(f.slots[i + 1] - f.slots[i]);
    char text[480]{};
    std::snprintf(text, sizeof(text),
        "FrameCompat: %s verdict=%s reason=%s structural=%s profile=%d attempts=%u table_rva=0x%llx entry_rva=0x%llx "
        "target_rva=0x%llx slot_delta=%lld,%lld,%lld fingers_missing=0x%02x fingers_duplicate=0x%02x; classifier read-only",
        context, verdictName(r.verdict), reasonName(r.reason), reasonName(r.structural), r.profile, attempts,
        rva(f.table), rva(r.entry), rva(r.branchTarget), stride[0], stride[1], stride[2],
        r.fingersMissing, r.fingersDuplicate);
    return text;
}

// Census-free classification of the live input object (used where v0.1.0
// already decided; the result is logged, never acted upon).
inline std::pair<frame_compat_check::Result, frame_compat_check::Facts> classifyLive(void* input) {
    frame_compat_check::Facts f;
    readStructure(input, f);
    auto hashes = hashRuntime();
    f.selfSha256 = hashes.self; f.serverSha256 = hashes.server; f.cvSha256 = hashes.cv;
    return {frame_compat_check::classify(f, shippedProfiles()), std::move(f)};
}

}  // namespace frame_runtime_facts
