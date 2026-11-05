#pragma once
#include <cstdint>

namespace frame_compat {
// Layout v1: verified against this exact binary pair. A different layout
// requires an independently validated backend, not just different addresses.
#ifdef FRAME_NOV_BETA_CANDIDATE
inline constexpr char serverSha256[] = "430c259a8e47ef94824462d98dd0d5bc78d0d1b0ed2f0e2c50e9562982fe06ad";
inline constexpr char cvSha256[] = "e54b20b9aba50b63b80a37e6b7d7b61454808dedd3f967d8c81f0c9460d18b7b";
inline constexpr std::uintptr_t inputTableRva = 0x5ce620;
inline constexpr std::uintptr_t inputEntries[]{0x779b8,0x779a0,0x77970,0x77960};
inline constexpr unsigned char scalarFingerprint[]{0xe2,0x03,0x01,0xaa,0x01,0x08,0x40,0xb9,0x00,0x08,0x40,0xf9,0x87,0x9f,0x00,0x14};
#else
inline constexpr char serverSha256[] = "cb97844fb2f315637bf3ef3ae9620052c4f3a1e8d537a4933b5010ce11be5b72";
inline constexpr char cvSha256[] = "44e19eeb547a3663c8db3b77b5743cca6596c4fa5e03d6348e83320b686d1e83";
inline constexpr std::uintptr_t inputTableRva = 0x5bd8e0;
inline constexpr std::uintptr_t inputEntries[]{0x76b60,0x76b48,0x76b18,0x76b08};
inline constexpr unsigned char scalarFingerprint[]{0xe2,0x03,0x01,0xaa,0x01,0x08,0x40,0xb9,0x00,0x08,0x40,0xf9,0xd9,0x9d,0x00,0x14};
#endif
#ifndef FRAME_NOV_BETA_CANDIDATE
inline constexpr std::uintptr_t cvInputCacheRva = 0x4a43f8;
#endif
}
