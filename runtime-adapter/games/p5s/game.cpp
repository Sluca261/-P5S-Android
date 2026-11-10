// Persona 5 Strikers (US Switch release): game-specific identity for the shared Switch runtime.
// Keep this adapter deliberately free of P5R offsets, hooks, shader IDs and assumptions.
#include <nx/game.h>

namespace nx::game {

const Info kInfo = {
    "p5s",
    "Persona 5 Strikers",
    0x0100801011C3E000ull, // US base program ID; confirm against the user's local metadata.
    "com.p5s.android",
    "private/p5s",
    0,
    nullptr // Do not reject other builds until regional/update IDs are verified locally.
};

// P5S-specific patches and renderer adjustments belong here only after they are measured
// against the US executable. Empty callbacks are intentional: no P5R hooks are reused.
void after_init() {}
void renderer_start() {}
void first_frame() {}

} // namespace nx::game
