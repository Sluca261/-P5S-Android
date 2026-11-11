#!/usr/bin/env python3
"""Fail CI if an expected P5S-only runtime adaptation is absent after patching."""
from pathlib import Path
import sys

root = Path(sys.argv[1])

def require(path: str, *needles: str) -> None:
    source = (root / path).read_text()
    missing = [needle for needle in needles if needle not in source]
    if missing:
        raise SystemExit(f"{path}: missing required P5S adaptation(s): {missing}")

require(
    "runtime/src/ipc.cpp",
    "struct P5SHidServer : Service",
    "cmds[101]",
    "cmds[128]",
    'ServiceRegistrar g_p5s_hid_registrar("hid"',
    "ipc: HID request",
)
require(
    "runtime/src/hle_hid.cpp",
    "GetTouchScreenState1",
    "kTouchState1Size = 0x38",
    "kTouchState16Size = 0x290",
    "_ZN2nn3hid19GetTouchScreenStateILm16EEEvPNS0_16TouchScreenStateIXT_EEE",
    "_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE",
)
require(
    "runtime/src/hle_fs.cpp",
    "void CommitSaveData(CPU& c)",
    "fsync(fd)",
    '_ZN2nn2fs14CommitSaveDataEPKc',
)
require(
    "runtime/src/nvn/nvn_shaders.cpp",
    "translate_shader_on_demand",
    'dlopen("libspirvharness.so", RTLD_NOW)',
    "spirv_harness_main",
    "resource metadata",
)
require(
    "runtime/src/game_image.cpp",
    "container parsed successfully",
    "wrong Title ID",
)
require(
    "runtime/src/touch_layout.cpp",
    '{"p5s", kP5R, std::size(kP5R), true',
)

print("P5S adaptation audit passed: HID IPC/HLE, save commit, shader translation, game identity and touch layout.")
