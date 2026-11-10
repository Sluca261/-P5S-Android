# Persona 5 Strikers — US runtime integration

## Regional target

The requested copy is the North American/US Nintendo Switch release.

- Base Program ID: `0100801011C3E000`
- Publicly catalogued base 1.0.0 Build ID: `C4DF04F647BDC727`

These are identification hints, not a verified read of the user's dump. Updates and related content may use related program IDs. The exact executable Build ID and CNMT version still need to be read from the local ExeFS before selecting version-specific patches.

## Adapter boundary

`games/p5s/game.cpp` defines the shared runtime's minimal `nx::game::Info` contract for P5S. It deliberately contains no P5R offsets, HLE hooks, shader cache IDs, frame pacing assumptions, or renderer patches. The callbacks remain empty until evidence from the US executable justifies a P5S-specific implementation.

## Current build status — 2026-11-10

The Android ARM64 runtime integration is now connected to the APK build:

1. The workflow checks out the shared runtime at pinned commit `0c3b233d07afef37ab5af5dae41bf1a71ea98acf` and applies auditable P5S-only adaptations.
2. It compiles `libp5s.so` for `arm64-v8a` with NCE enabled.
3. It builds the ARM64 NativeAOT Maxwell-to-SPIR-V translator as `libspirvharness.so`.
4. The launcher passes the locally selected NSP/XCI file descriptor and private `prod.keys` path to the NativeActivity runtime. The runtime's game-image path opens the container in place and checks the base Title ID against the P5S adapter.
5. The standalone NativeActivity export wrapper and both native libraries are checked in the APK before artifact publication.

The CI run [42765423050](https://github.com/Sluca261/-P5S-Android/actions/runs/42765423050) completed successfully. Its APK artifact passed static checks for ARM64 ELF architecture, the NativeActivity entry point, and the shader translator export.

**This proves compilation, packaging and static symbol validation only. It does not prove that P5S boots or is playable.** No real P5S NSP/XCI has been used in CI, and the APK has not yet been validated through the game's boot sequence on the target phone.

## Remaining compatibility work

The public P5R reference is a reusable architecture, not a drop-in P5S port. The shared loader can now be built into the app, but game-specific compatibility remains unproven:

1. Verify the user's local container metadata, CNMT version, ExeFS modules and actual Build ID without uploading the game or keys.
2. Confirm that P5S's ARM64 executable and its imported Switch services are supported by the shared runtime.
3. Capture and diagnose first-boot runtime logs; resolve missing services, imports, filesystem assumptions or memory mappings based on evidence.
4. Validate P5S's NVN shader/control data and Maxwell-to-SPIR-V translation on the target Adreno/Vulkan driver. The on-demand translator is integrated, but P5S shader coverage is not yet demonstrated.
5. Use the startup diagnostics to inspect the exact P5S main Build ID and up to 64 unresolved dynamic imports; the loader now reports these with the requesting module name. Optional base-game `subsdk1`–`subsdk9` modules are loaded when present, while `subsdk0` (the Switch NVN driver) remains excluded.
6. Validate video/cutscenes, AAudio, touch and physical controls, save/load and stability.
7. Progress through observable milestones: runtime boot, first rendered game scene, title/menu, interactive combat, then full gameplay sessions.

No P5R-specific hooks, offsets or shader caches should be copied into P5S without evidence from the matching P5S executable.

## Privacy and distribution

The project must not contain game assets, executable modules, NSP/XCI files, `prod.keys` or title keys. The user's local game and keys remain on their device; they must never be committed, uploaded to CI, or included in the APK. The runtime and translator binaries are built from pinned public source and their applicable notices/licenses must be preserved.

## Definition of a playable build

A successful build, APK artifact, launcher screen, Vulkan diagnostic model or NativeActivity launch alone is not a playable version. The project should only be marked playable after a real, locally supplied P5S copy reaches interactive gameplay with working rendering and controls. Audio and save/load should also be validated before calling the port complete.
