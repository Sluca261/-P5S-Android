# Persona 5 Strikers — US runtime integration

## Regional target

The requested copy is the North American/US Nintendo Switch release. The initial catalogue reference is:

- Base Program ID: `0100801011C3E000`
- Publicly catalogued base 1.0.0 Build ID: `C4DF04F647BDC727`

These are identification hints, not a verified read of the user's dump. Updates and related content may use related program IDs, and the exact executable Build ID must be read from the local ExeFS before selecting version-specific patches.

## Adapter boundary

`runtime-adapter/games/p5s/game.cpp` defines the shared runtime's minimal `nx::game::Info` contract for P5S. It deliberately contains no P5R offsets, HLE hooks, shader cache IDs, frame pacing assumptions, or renderer patches. The callbacks are empty until evidence from the US executable justifies a P5S-specific implementation.

This adapter is not yet connected to the Android app's CMake build. The current app still builds only `native-lib.cpp`; it does not build the shared Switch runtime and this file is not a playable implementation.

## Runtime integration requirements

The public P5R reference demonstrates a reusable architecture, not a drop-in P5S port. Before runtime integration can be called successful, the project needs to:

1. Vendor or pin an auditable, licensed shared runtime and its Android platform layer, retaining upstream attribution and notices.
2. Build the shared runtime for `arm64-v8a` with native AArch64 execution (NCE), without bundling any game executable or keys.
3. Connect Android's document-provider file descriptor to the runtime's importer. The runtime must open the user's local NSP/XCI and local key files in place; none of these may be uploaded or placed in the APK.
4. Verify the program ID, CNMT version and ExeFS Build ID from the user's local file before choosing a game adapter or applying patches.
5. Determine P5S-specific system services, graphics/shader requirements, audio/video, input and save behavior from actual runtime logs and controlled tests. P5R game-specific hooks and shaders must not be copied.
6. Build, install and test on the target Android device, progressing from boot to title screen, menus, combat, cutscenes, audio and save/load.

## Current status

This commit adds only the P5S adapter boundary. It does not add NSP/XCI decryption, key management, NCE execution, NVN translation, shader extraction, or a game runtime. The existing diagnostic APK remains non-playable. Do not label a build as a playable Alpha until the runtime boots the US game and reaches interactive gameplay.
