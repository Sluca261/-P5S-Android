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
  lib/arm64-v8a/libnative-lib.so \
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
check_arm64 "$tmp/libnative-lib.so"
check_arm64 "$tmp/libp5s.so"
check_arm64 "$tmp/libspirvharness.so"

echo "---- NativeActivity dynamic symbols ----"
"$readelf" --dyn-syms --wide "$tmp/libp5s.so" | grep -E 'ANativeActivity|p5s_internal|glue_ANative' || true
echo "---- NativeActivity full symbol table ----"
"$readelf" --symbols --wide "$tmp/libp5s.so" | grep -E 'ANativeActivity|p5s_internal|glue_ANative' || true
echo "---- NativeActivity dynamic export via nm ----"
"$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm" -D --defined-only "$tmp/libp5s.so" | grep -E 'ANativeActivity|p5s_internal|glue_ANative' || true

p5s_dyn_symbols="$("$readelf" --dyn-syms --wide "$tmp/libp5s.so")"
spirv_dyn_symbols="$("$readelf" --dyn-syms --wide "$tmp/libspirvharness.so")"
app_dyn_symbols="$("$readelf" --dyn-syms --wide "$tmp/libnative-lib.so")"

# Avoid grep -q in a pipe under pipefail: an early match can SIGPIPE readelf.
has_global_export() {
  local symbols="$1"
  local name="$2"
  awk -v wanted="$name" '$4 == "FUNC" && $5 == "GLOBAL" && $6 == "DEFAULT" && $7 != "UND" && $8 == wanted { found = 1 } END { exit !found }' <<<"$symbols"
}

if ! has_global_export "$p5s_dyn_symbols" "ANativeActivity_onCreate"; then
  echo "libp5s.so does not define the global NativeActivity entry point" >&2
  exit 1
fi
if ! has_global_export "$spirv_dyn_symbols" "spirv_harness_main"; then
  echo "libspirvharness.so does not define the global shader translator entry point" >&2
  exit 1
fi
if ! has_global_export "$app_dyn_symbols" "Java_com_p5s_android_MainActivity_configureRuntime"; then
  echo "libnative-lib.so does not define the launcher JNI runtime bridge" >&2
  exit 1
fi

echo "Runtime APK verified: all three ARM64 libraries, NativeActivity entry point, shader translator and launcher JNI bridge are present."
