#pragma once

#include <cstdint>
#include <cstring>

namespace p5s::identity {

// Persona 5 Strikers (Nintendo Switch, base title).
// The low 12 bits are reserved for related content/update program IDs.
inline constexpr uint64_t kBaseProgramId = 0x0100801011C3E000ull;

// Publicly catalogued Build ID for the 1.0.0 base executable.
// This is an identification hint, not proof that every regional dump is supported.
inline constexpr char kKnownBaseBuildId[] = "C4DF04F647BDC727";

inline constexpr uint64_t kProgramIdMask = ~0xFFFull;

inline constexpr bool isPersona5StrikersProgram(uint64_t programId) {
    return (programId & kProgramIdMask) == kBaseProgramId;
}

inline bool isKnownBaseBuild(const char* buildId) {
    return buildId != nullptr && std::strcmp(buildId, kKnownBaseBuildId) == 0;
}

}  // namespace p5s::identity
