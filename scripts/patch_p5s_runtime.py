# Rebuild trigger: include expanded ELF symbol diagnostics in the APK verifier.
#!/usr/bin/env python3
"""Apply small, auditable P5S-only adaptations to the pinned shared runtime."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
platform = root / "runtime/src/platform_android.cpp"
text = platform.read_text()
old = 'static bool has_data(const std::string& root) { return access((root + kDataProbe).c_str(), R_OK) == 0; }'
new = """static bool has_data(const std::string& root) {
    // P5S is loaded directly from the user's NSP/XCI (NX_NCE). In that mode the
    // ExeFS is supplied by game_image.cpp, not by a pre-extracted /exefs folder.
    const char* game_fd = std::getenv("NX_GAME_FD");
    const char* keys = std::getenv("NX_KEYS");
    if (game_fd && *game_fd && keys && *keys) return true;
    return access((root + kDataProbe).c_str(), R_OK) == 0;
}"""
if text.count(old) != 1:
    raise SystemExit("platform_android.cpp: expected exactly one has_data() definition")
platform.write_text(text.replace(old, new))

# Reject a wrong NSP/XCI before the NCE loader starts executing its ExeFS.
image = root / "runtime/src/game_image.cpp"
text = image.read_text()
if '#include <nx/game.h>' not in text:
    text = text.replace('#include <nx/runtime.h>', '#include <nx/game.h>\n#include <nx/runtime.h>', 1)
old_open = '    if (!g_image) fatal("game: %s could not be read: %s", shown.c_str(), err.c_str());'
new_open = old_open + '''
    const uint64_t expected_title = game::kInfo.program_id & ~0xFFFull;
    const uint64_t actual_title = g_image->title_id() & ~0xFFFull;
    if (actual_title != expected_title)
        fatal("game: wrong Title ID %016llx (expected %016llx)",
              (unsigned long long)actual_title, (unsigned long long)expected_title);'''
if text.count(old_open) != 1:
    raise SystemExit("game_image.cpp: image-open validation point not found uniquely")
image.write_text(text.replace(old_open, new_open))

shaders = root / "runtime/src/nvn/nvn_shaders.cpp"
text = shaders.read_text()
old_include = """#include <memory>
#include <mutex>

#include <nx/runtime.h>"""
new_include = """#include <memory>
#include <mutex>
#if defined(__ANDROID__)
#include <dlfcn.h>
#endif

#include <nx/runtime.h>"""
if text.count(old_include) != 1:
    raise SystemExit("nvn_shaders.cpp: runtime include block not found uniquely")
text = text.replace(old_include, new_include)

anchor = """bool read_whole(const std::string& path, std::string& out) {
    FILE* f = plat::fopen_utf8(path, "rb");
    if (!f) return false;
    plat::fseek64(f, 0, SEEK_END);
    int64_t size = plat::ftell64(f);
    out.resize(size > 0 ? size_t(size) : 0);
    bool ok = size >= 0 && plat::read_at(f, 0, out.data(), out.size()) == out.size();
    std::fclose(f);
    return ok;
}
"""
addition = anchor + r'''

bool write_whole(const std::string& path, const void* data, size_t size) {
    FILE* f = plat::fopen_utf8(path, "wb");
    if (!f) return false;
    bool ok = std::fwrite(data, 1, size, f) == size;
    std::fclose(f);
    return ok;
}

#if defined(__ANDROID__)
// Translate a shader the first time the game asks for it. This is deliberately generic:
// it consumes the Maxwell code/control block passed through NVN, not P5R-specific CPK data.
bool translate_shader_on_demand(const std::string& hash, const uint8_t* code, uint32_t code_size,
                                const uint8_t* control, std::string& spirv, std::string& resources) {
    // NVN may request shaders on multiple rendering threads. The harness uses
    // fixed input/output paths and process-global batch state, so serialize calls.
    static std::mutex translation_mutex;
    const std::lock_guard<std::mutex> translation_lock(translation_mutex);

    using HarnessMain = int (*)(int, const char**);
    static void* library = dlopen("libspirvharness.so", RTLD_NOW);
    static HarnessMain run = library
        ? reinterpret_cast<HarnessMain>(dlsym(library, "spirv_harness_main"))
        : nullptr;
    if (!run) {
        const char* error = dlerror();
        log("nvn: on-demand shader translator unavailable: %s", error ? error : "missing export");
        return false;
    }

    const std::string base = plat::data_root() + "/shader_runtime";
    const std::string input = base + "/input";
    const std::string output = base + "/output";
    const std::string shader_dir = plat::data_root() + "/spirv";
    if (!plat::make_dirs(input) || !plat::make_dirs(output) || !plat::make_dirs(shader_dir)) return false;
    if (!write_whole(input + "/code.bin", code, code_size) ||
        !write_whole(input + "/ctrl.bin", control, 0x880)) return false;

    std::string manifest = "id\tname\tstage\tunused\tunused\tunused\tunused\tunused\tunused\tunused\thash\tcode\tcontrol\n";
    manifest.append(10, '\t');
    manifest += hash + "\tcode.bin\tctrl.bin\n";
    if (!write_whole(input + "/manifest.tsv", manifest.data(), manifest.size())) return false;

    const char* args[] = {"--batch", input.c_str(), "--out", output.c_str(), "--jobs", "1", "--no-val"};
    const int rc = run(7, args);
    if (rc != 0) {
        log("nvn: on-demand translation of %s failed (%d)", hash.c_str(), rc);
        return false;
    }

    if (!read_whole(output + "/" + hash + ".spv", spirv) || spirv.empty() || spirv.size() % 4) {
        log("nvn: translator produced no valid SPIR-V for %s", hash.c_str());
        return false;
    }
    std::string result_table;
    if (read_whole(output + "/results.tsv", result_table)) {
        std::unordered_map<std::string, std::string> translated;
        parse_results_tsv(result_table, translated);
        auto it = translated.find(hash);
        if (it != translated.end()) resources = it->second;
    }
    if (!write_whole(shader_dir + "/" + hash + ".spv", spirv.data(), spirv.size())) return false;

    // Keep the translated SPIR-V cache; discard only temporary source and report files.
    std::remove((input + "/code.bin").c_str());
    std::remove((input + "/ctrl.bin").c_str());
    std::remove((input + "/manifest.tsv").c_str());
    std::remove((output + "/" + hash + ".spv").c_str());
    std::remove((output + "/results.tsv").c_str());
    std::remove((output + "/summary.txt").c_str());
    return true;
}
#else
bool translate_shader_on_demand(const std::string&, const uint8_t*, uint32_t,
                                const uint8_t*, std::string&, std::string&) {
    return false;
}
#endif
'''
if text.count(anchor) != 1:
    raise SystemExit("nvn_shaders.cpp: read_whole() anchor not found uniquely")
text = text.replace(anchor, addition)

old_missing = '''    if (!read_whole(dir + hash + ".spv", bytes) || bytes.size() % 4 || bytes.empty()) {
        log("nvn: shader %s (code %#x bytes, control %llx): no %s%s.spv", hash.c_str(), c.code_size,
            (unsigned long long)control, dir.c_str(), hash.c_str());
        if (std::getenv("NX_DUMP_SHADER_MISSES")) {  // for translating shaders the offline inventory missed
            std::string out = plat::data_root() + "/shader_misses/";
            plat::make_dirs(out);
            if (FILE* f = plat::fopen_utf8(out + "code_" + hash + ".bin", "wb")) {
                std::fwrite(code, 1, c.code_size, f);
                std::fclose(f);
            }
            if (FILE* f = plat::fopen_utf8(out + "ctrl_" + hash + ".bin", "wb")) {
                std::fwrite(ctrl, 1, 0x880, f);
                std::fclose(f);
            }
        }
        return nullptr;
    }'''
new_missing = '''    if (!read_whole(dir + hash + ".spv", bytes) || bytes.size() % 4 || bytes.empty()) {
        std::string translated_resources;
        if (translate_shader_on_demand(hash, code, c.code_size, ctrl, bytes, translated_resources)) {
            if (!translated_resources.empty()) k.resources[hash] = translated_resources;
            log("nvn: shader %s translated and cached on demand (%zu bytes)", hash.c_str(), bytes.size());
        } else {
            log("nvn: shader %s (code %#x bytes, control %llx): no usable SPIR-V cache", hash.c_str(), c.code_size,
                (unsigned long long)control);
            if (std::getenv("NX_DUMP_SHADER_MISSES")) {
                std::string out = plat::data_root() + "/shader_misses/";
                plat::make_dirs(out);
                write_whole(out + "code_" + hash + ".bin", code, c.code_size);
                write_whole(out + "ctrl_" + hash + ".bin", ctrl, 0x880);
            }
            return nullptr;
        }
    }'''
if text.count(old_missing) != 1:
    raise SystemExit("nvn_shaders.cpp: shader cache miss block not found uniquely")
text = text.replace(old_missing, new_missing)
shaders.write_text(text)

# The upstream Android target compiles with -fvisibility=hidden. Its NativeActivity
# entry point therefore needs an explicit default-visibility annotation or Android
# cannot resolve ANativeActivity_onCreate from libp5s.so at launch.
platform_text = platform.read_text()
# JNIEXPORT expands to default visibility on Android. The upstream entry point
# already carries it; assert that contract rather than rewriting a non-existent form.
entry = 'extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void* saved, size_t size) {'
exported_entry = 'extern "C" JNIEXPORT __attribute__((visibility("default"))) void ANativeActivity_onCreate(ANativeActivity* activity, void* saved, size_t size) {'
if platform_text.count(entry) != 1:
    raise SystemExit("platform_android.cpp: NativeActivity entry point not found uniquely")
platform.write_text(platform_text.replace(entry, exported_entry))

# Export a small, independently compiled NativeActivity wrapper. The runtime's
# implementation is renamed in its translation unit, then called by this wrapper.
cmake = root / "CMakeLists.txt"
cmake_text = cmake.read_text()
link_anchor = '    target_link_libraries(${NX_GAME} PRIVATE ${NX_GAME_LIBS} android log aaudio mediandk)'
link_export = link_anchor + '\n    target_link_options(${NX_GAME} PRIVATE -u ANativeActivity_onCreate -Wl,--undefined=ANativeActivity_onCreate -Wl,--export-dynamic-symbol=ANativeActivity_onCreate)'
if cmake_text.count(link_anchor) != 1:
    raise SystemExit("CMakeLists.txt: Android runtime link block not found uniquely")
cmake_text = cmake_text.replace(link_anchor, link_export)
source_anchor = '    target_link_options(${NX_GAME} PRIVATE -u ANativeActivity_onCreate -Wl,--undefined=ANativeActivity_onCreate -Wl,--export-dynamic-symbol=ANativeActivity_onCreate)'
source_patch = source_anchor + '''
    set_source_files_properties(runtime/src/platform_android.cpp PROPERTIES
        COMPILE_DEFINITIONS "ANativeActivity_onCreate=p5s_internal_ANativeActivity_onCreate")
    target_sources(${NX_GAME} PRIVATE runtime/src/native_activity_export.cpp)'''
if cmake_text.count(source_anchor) != 1:
    raise SystemExit("CMakeLists.txt: NativeActivity link option anchor not found uniquely")
cmake.write_text(cmake_text.replace(source_anchor, source_patch))

export_file = root / "runtime/src/native_activity_export.cpp"
export_file.write_text("""#include <android/native_activity.h>
#include <cstddef>

extern "C" void p5s_internal_ANativeActivity_onCreate(ANativeActivity*, void*, size_t);

extern "C" __attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* activity, void* saved, size_t size) {
    p5s_internal_ANativeActivity_onCreate(activity, saved, size);
}
""")

print("Applied P5S runtime adaptations: NSP-backed data root, on-demand Maxwell shader translation, and exported NativeActivity entry point.")

# Rebuild trigger: verifier pipefail fix in verify_runtime_apk.sh.

# Rebuild trigger: verifier now checks launcher JNI bridge and defined ELF exports.

# Rebuild trigger: collect ELF export diagnostics for NativeAOT and launcher JNI.

# Load optional base-game subsdk1..9 modules from ExeFS. subsdk0 is the Switch NVN
# driver and is intentionally skipped; other modules may contain required game code.
module_cpp = root / "runtime/src/main.cpp"
module_text = module_cpp.read_text()
module_end = '''        }
    }
    if (nso.size() < 2) fatal("failed to load base modules (main and sdk required)");'''
module_replacement = '''        } else if (name != "subsdk0") {
            // Load optional base-game modules while keeping the Switch NVN driver out.
            if (image) {
                auto it = image->exefs().find(name);
                if (it != image->exefs().end()) {
                    auto m = nce::load_nso_bytes(it->second, std::string("ExeFS/") + name, name, base, error);
                    if (m) nso.push_back(std::move(m));
                    else log("nce: failed to load base ExeFS/%s: %s", name.c_str(), error.c_str());
                }
            } else {
                std::error_code ec;
                const auto path = fs::u8path(exefs) / fs::u8path(name);
                if (fs::is_regular_file(path, ec)) {
                    auto m = nce::load_nso(path.string(), name, base, error);
                    if (m) nso.push_back(std::move(m));
                    else log("nce: failed to load base exefs/%s: %s", name.c_str(), error.c_str());
                }
            }
        }
    }
    if (nso.size() < 2) fatal("failed to load base modules (main and sdk required)");'''
if module_text.count(module_end) != 1:
    raise SystemExit("main.cpp: module-loop insertion point not found uniquely")
module_cpp.write_text(module_text.replace(module_end, module_replacement))

# Report the actual PFS0/ExeFS main build ID on-device. P5S revisions differ by region/update,
# and this log is needed to choose any future version-specific compatibility work safely.
main_cpp = root / "runtime/src/main.cpp"
text = main_cpp.read_text()
main_anchor = '''    if (nso.size() < 2) fatal("failed to load base modules (main and sdk required)");

    if (const char* want = game::kInfo.main_build_id) {'''
main_replacement = '''    if (nso.size() < 2) fatal("failed to load base modules (main and sdk required)");

    // Record the exact game executable revision at startup. P5S dumps can vary by region/update;
    // this lets us identify the user's build without exporting or inspecting their game files.
    {
        std::string build;
        for (uint8_t b : nso[0]->build_id) {
            char hex[3];
            std::snprintf(hex, sizeof hex, "%02X", b);
            build += hex;
        }
        log("nce: game %s (title %016llx), main build ID %s",
            game::kInfo.title, (unsigned long long)game::kInfo.program_id, build.c_str());
    }

    if (const char* want = game::kInfo.main_build_id) {'''
if text.count(main_anchor) != 1:
    raise SystemExit("main.cpp: build-ID diagnostic insertion point not found uniquely")
main_cpp.write_text(text.replace(main_anchor, main_replacement))


# Log a bounded sample of unresolved dynamic imports. This is especially useful for P5S,
# whose service/SDK imports are not yet validated against a real game startup.
loader_cpp = root / "runtime/src/loader.cpp"
loader_text = loader_cpp.read_text()
loader_anchor = '''    size_t hle = 0, missing = 0, linked = 0;
    for (auto& lm : g_modules) {'''
loader_replacement = '''    size_t hle = 0, missing = 0, linked = 0, missing_logged = 0;
    for (auto& lm : g_modules) {'''
if loader_text.count(loader_anchor) != 1:
    raise SystemExit("loader.cpp: import diagnostic insertion point not found uniquely")
loader_text = loader_text.replace(loader_anchor, loader_replacement)
missing_anchor = '''                if (s.type == 2 || s.type == 0) target = hle_address(s.name);
                missing++;
            }
            *slot = target + uint64_t(r.addend);'''
missing_replacement = '''                if (s.type == 2 || s.type == 0) target = hle_address(s.name);
                if (missing_logged < 64) {
                    log("unresolved import: %s (module %s, type %u)", s.name, mi->name, s.type);
                    missing_logged++;
                }
                missing++;
            }
            *slot = target + uint64_t(r.addend);'''
if loader_text.count(missing_anchor) != 1:
    raise SystemExit("loader.cpp: unresolved import diagnostic insertion point not found uniquely")
loader_cpp.write_text(loader_text.replace(missing_anchor, missing_replacement))


# Give P5S a visible on-screen controller layout. The shared runtime's generic touch
# fallback accepts taps but draws no buttons for games without a registered layout.
# Reuse the neutral Switch face-button/shoulder/d-pad arrangement; this adds no P5R
# executable offsets or gameplay hooks.
touch_cpp = root / "runtime/src/touch_layout.cpp"
touch_text = touch_cpp.read_text()
touch_anchor = '''    {"p5r", kP5R, std::size(kP5R), true, 0, 0.15f, 0.76f},
};'''
touch_replacement = '''    {"p5r", kP5R, std::size(kP5R), true, 0, 0.15f, 0.76f},
    {"p5s", kP5R, std::size(kP5R), true, 0, 0.15f, 0.76f},
};'''
if touch_text.count(touch_anchor) != 1:
    raise SystemExit("touch_layout.cpp: P5R layout registration point not found uniquely")
touch_cpp.write_text(touch_text.replace(touch_anchor, touch_replacement))


# Add explicit boot milestones so the first real-device run can be localized to
# image parsing, module loading/linking, SDK startup, or the game's nnMain.
main_cpp = root / "runtime/src/main.cpp"
text = main_cpp.read_text()
boot_anchor = """    memory_init();
    kernel_init();
#if NX_NCE"""
boot_replacement = """    log("boot: P5S Android runtime entry");
    log("boot: reserving guest memory");
    memory_init();
    log("boot: initializing Horizon kernel services");
    kernel_init();
#if NX_NCE"""
if text.count(boot_anchor) != 1:
    raise SystemExit("main.cpp: boot milestone insertion point not found uniquely")
text = text.replace(boot_anchor, boot_replacement)
nce_anchor = """    nce::init();
    static std::vector<std::unique_ptr<nce::NsoModule>> nso;"""
nce_replacement = """    log("boot: initializing ARM64 native code execution");
    nce::init();
    log("boot: NCE initialized; opening selected NSP/XCI");
    static std::vector<std::unique_ptr<nce::NsoModule>> nso;"""
if text.count(nce_anchor) != 1:
    raise SystemExit("main.cpp: NCE milestone insertion point not found uniquely")
text = text.replace(nce_anchor, nce_replacement)
modules_anchor = """    g_main = &nso[0]->info, g_sdk = &nso[1]->info;
    std::vector<const ModuleInfo*> mods;"""
modules_replacement = """    log("boot: loaded %zu executable modules; preparing dynamic linker", nso.size());
    g_main = &nso[0]->info, g_sdk = &nso[1]->info;
    std::vector<const ModuleInfo*> mods;"""
if text.count(modules_anchor) != 1:
    raise SystemExit("main.cpp: module-link milestone insertion point not found uniquely")
text = text.replace(modules_anchor, modules_replacement)
link_anchor = """    load_modules(mods.data(), mods.size(), "", img_ptrs.data());
#else"""
link_replacement = """    log("boot: linking %zu executable modules", mods.size());
    load_modules(mods.data(), mods.size(), "", img_ptrs.data());
    log("boot: executable modules linked; preparing main thread");
#else"""
if text.count(link_anchor) != 1:
    raise SystemExit("main.cpp: linker milestone insertion point not found uniquely")
text = text.replace(link_anchor, link_replacement)
start_anchor = """    log("entering nn::init::Start at %s", describe(start).c_str());"""
start_replacement = """    log("boot: SDK startup symbol resolved");
    log("entering nn::init::Start at %s", describe(start).c_str());"""
if text.count(start_anchor) != 1:
    raise SystemExit("main.cpp: SDK startup milestone insertion point not found uniquely")
text = text.replace(start_anchor, start_replacement)
main_cpp.write_text(text)

# Distinguish successful container parsing from ExeFS/module loading in the log.
image = root / "runtime/src/game_image.cpp"
text = image.read_text()
image_anchor = """    g_image = nxi::GameImage::open(in, 0, err);"""
image_replacement = """    log("game: parsing %s and its local key material", shown.c_str());
    g_image = nxi::GameImage::open(in, 0, err);"""
if text.count(image_anchor) != 1:
    raise SystemExit("game_image.cpp: image parsing milestone insertion point not found uniquely")
text = text.replace(image_anchor, image_replacement)
image_log_anchor = """    log("game: %s read in place (title %016llx): %zu RomFS files, ExeFS %zu files", shown.c_str(),"""
image_log_replacement = """    log("game: container parsed successfully");
    log("game: %s read in place (title %016llx): %zu RomFS files, ExeFS %zu files", shown.c_str(),"""
if text.count(image_log_anchor) != 1:
    raise SystemExit("game_image.cpp: parsed-image log insertion point not found uniquely")
text = text.replace(image_log_anchor, image_log_replacement)
image.write_text(text)
