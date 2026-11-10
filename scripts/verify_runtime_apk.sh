#!/usr/bin/env bash
set -euo pipefail

apk_dir="app/build/outputs/apk/debug"
mapfile -t apks < <(find "$apk_dir" -maxdepth 1 -type f -name '*.apk' -print)
if [[ ${#apks[@]} -ne 1 ]]; then
  echo "Expected exactly one debug APK, found ${#apks[@]}" >&2
  exit 1
fi
apk="${apks[0]}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

for entry in \
  lib/arm64-v8a/libp5s.so \
  lib/arm64-v8a/libspirvharness.so; do
  if ! unzip -Z1 "$apk" | grep -Fxq "$entry"; then
    echo "APK is missing required runtime library: $entry" >&2
    exit 1
  fi
  unzip -p "$apk" "$entry" > "$tmp/$(basename "$entry")"
done

sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
ndk="$sdk/ndk/28.2.13676358"
readelf="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf"
if [[ ! -x "$readelf" ]]; then
  echo "Android NDK llvm-readelf not found: $readelf" >&2
  exit 1
fi

check_arm64() {
  local lib="$1"
  if ! "$readelf" --file-header "$lib" | grep -q 'Machine:.*AArch64'; then
    echo "Unexpected ELF architecture in $lib" >&2
    exit 1
  fi
}
check_arm64 "$tmp/libp5s.so"
check_arm64 "$tmp/libspirvharness.so"

if ! "$readelf" --dyn-syms --wide "$tmp/libp5s.so" | grep -q 'ANativeActivity_onCreate'; then
  echo "libp5s.so does not export ANativeActivity_onCreate" >&2
  exit 1
fi
if ! "$readelf" --dyn-syms --wide "$tmp/libspirvharness.so" | grep -q 'spirv_harness_main'; then
  echo "libspirvharness.so does not export spirv_harness_main" >&2
  exit 1
fi

echo "Runtime APK verified: ARM64 NativeActivity and on-demand shader translator are packaged with required exports."
