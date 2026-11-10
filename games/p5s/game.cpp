// Persona 5 Strikers (North American/US release) game adapter.
// This file follows the shared runtime contract in runtime/include/nx/game.h.
// Do not add P5R offsets, hooks, shader identifiers or version assumptions here.
#include <nx/game.h>

namespace nx::game {

const Info kInfo = {
    "p5s",
    "Persona 5 Strikers",
    0x0100801011C3E000ull,
    "com.p5s.android",
    "private/p5s",
    0,
    nullptr
};

void after_init() {}
void renderer_start() {}
void first_frame() {}

} // namespace nx::game
