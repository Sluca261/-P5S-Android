# Rebuild trigger: include expanded ELF symbol diagnostics in the APK verifier.
# Rebuild trigger: register the direct GetTouchScreenState<16> HLE import.
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
    if (!read_whole(output + "/results.tsv", result_table)) {
        log("nvn: translator produced no results.tsv for %s", hash.c_str());
        return false;
    }
    std::unordered_map<std::string, std::string> translated;
    parse_results_tsv(result_table, translated);
    auto translated_it = translated.find(hash);
    if (translated_it == translated.end()) {
        log("nvn: translator produced no resource record for %s", hash.c_str());
        return false;
    }
    // An empty resources string is still a valid record: it distinguishes a shader
    // known to have no special NVN bindings from one whose metadata was never cached.
    resources = translated_it->second;

    // Persist resource metadata alongside the SPIR-V. Without this table, a shader
    // cached on first launch can be loaded on later launches without the c[0] offsets
    // needed to bind Maxwell storage buffers, causing compute pipelines to fail.
    const std::string results_path = shader_dir + "/results.tsv";
    std::string previous_table;
    std::string merged_table =
        "hash\tgroup\tstage\tstages_using\tstatus\tms\tspv_bytes\tresources\ttranslator_messages\terror\tdraw_time_queries\n";
    auto append_rows = [&](const std::string& table, bool skip_header) {
        size_t pos = 0;
        if (skip_header) {
            const size_t header_end = table.find('\n');
            if (header_end == std::string::npos) return;
            pos = header_end + 1;
        }
        while (pos < table.size()) {
            const size_t end = table.find('\n', pos);
            const size_t stop = end == std::string::npos ? table.size() : end;
            const std::string line = table.substr(pos, stop - pos);
            const size_t tab = line.find('\t');
            if (!line.empty() && tab != std::string::npos && line.substr(0, tab) != hash) {
                merged_table += line;
                merged_table.push_back('\n');
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
    };
    if (read_whole(results_path, previous_table)) append_rows(previous_table, true);
    append_rows(result_table, true);
    const std::string temporary_results = results_path + ".tmp";
    if (!write_whole(temporary_results, merged_table.data(), merged_table.size()) ||
        std::rename(temporary_results.c_str(), results_path.c_str()) != 0) {
        log("nvn: could not persist shader resource metadata for %s", hash.c_str());
        std::remove(temporary_results.c_str());
        return false;
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
new_missing = '''    const bool cached_spirv = read_whole(dir + hash + ".spv", bytes) &&
                              !bytes.empty() && bytes.size() % 4 == 0;
    const bool cached_resources = k.resources.find(hash) != k.resources.end();
    if (!cached_spirv || !cached_resources) {
        const std::string previous_spirv = bytes;
        std::string translated_resources;
        if (translate_shader_on_demand(hash, code, c.code_size, ctrl, bytes, translated_resources)) {
            k.resources[hash] = translated_resources;
            log("nvn: shader %s translated and cached with resource metadata (%zu bytes)",
                hash.c_str(), bytes.size());
        } else if (!cached_spirv) {
            log("nvn: shader %s (code %#x bytes, control %llx): no usable SPIR-V cache", hash.c_str(), c.code_size,
                (unsigned long long)control);
            if (std::getenv("NX_DUMP_SHADER_MISSES")) {
                std::string out = plat::data_root() + "/shader_misses/";
                plat::make_dirs(out);
                write_whole(out + "code_" + hash + ".bin", code, c.code_size);
                write_whole(out + "ctrl_" + hash + ".bin", ctrl, 0x880);
            }
            return nullptr;
        } else {
            // A previously cached shader can still be used if the translator is unavailable;
            // resource-dependent bindings will be diagnosed by make_shader_info below.
            bytes = previous_spirv;
            log("nvn: shader %s has SPIR-V but no resource record; using existing cache", hash.c_str());
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
# JNIEXPORT expands to default visibility on Android. The upstream entry point# already carries it; assert that contract rather than rewriting a non-existent form.
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
module_replacement = '''        } else if (name == "subsdk0") {
            // subsdk0 is often the Switch NVN driver, but some SDK builds put
            // NintendoSDK_movie there. Load it only if it supplies a movie import
            // that main actually needs; never load the GPU driver speculatively.
            std::unique_ptr<nce::NsoModule> candidate;
            if (image) {
                auto it = image->exefs().find(name);
                if (it != image->exefs().end())
                    candidate = nce::load_nso_bytes(it->second, std::string("ExeFS/") + name, name, base, error);
            } else {
                std::error_code ec;
                const auto path = fs::u8path(exefs) / fs::u8path(name);
                if (fs::is_regular_file(path, ec))
                    candidate = nce::load_nso(path.string(), name, base, error);
            }
            if (candidate) {
                bool supplies_movie_import = false;
                for (size_t imported_index = 0; imported_index < nso[0]->info.symbol_count; ++imported_index) {
                    const Symbol& imported = nso[0]->info.symbols[imported_index];
                    if (!imported.imported || std::strncmp(imported.name, "_ZN5movie", 9) != 0)
                        continue;
                        for (size_t exported_index = 0; exported_index < candidate->info.symbol_count; ++exported_index) {
                        const Symbol& exported = candidate->info.symbols[exported_index];
                        if (!exported.imported && std::strcmp(exported.name, imported.name) == 0) {
                            supplies_movie_import = true;
                            break;
                        }
                    }
                    if (supplies_movie_import) break;
                }
                if (supplies_movie_import) {
                    log("nce: loading ExeFS/subsdk0 because it supplies a movie import from main");
                    nso.push_back(std::move(candidate));
                } else {
                    log("nce: skipping ExeFS/subsdk0: no imported movie symbol from main is exported");
                }
            } else if (!error.empty()) {
                log("nce: could not inspect ExeFS/subsdk0: %s", error.c_str());
            }
        } else {
            // Load optional base-game subsdk1..9 modules; these may contain game middleware.
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


# Register the two hid server commands observed in P5S startup.
# Switchbrew documents cmd 101 as GetSupportedNpadStyleSet (u32 result) and
# cmd 128 as SetNpadHandheldActivationMode (PID, ARUID, s64 mode; no output).
# Keep this deliberately narrow: no fabricated touch contacts or unrelated HID commands.
ipc_cpp = root / "runtime/src/ipc.cpp"
ipc_text = ipc_cpp.read_text()
ipc_include = "#include <mutex>\n#include <set>"
ipc_include_new = "#include <atomic>\n#include <mutex>\n#include <set>"
if ipc_text.count(ipc_include) != 1:
    raise SystemExit("ipc.cpp: include insertion point not found uniquely")
ipc_text = ipc_text.replace(ipc_include, ipc_include_new)
registrar_anchor = """void register_service(const std::string& name, ServiceFactory f) { factories()[name] = std::move(f); }
"""
registrar_new = registrar_anchor + """
namespace {
struct P5SHidServer : Service {
    P5SHidServer() : Service("hid") {
        // NpadStyleTag_NpadFullKey: the player-1 pad exposed by the HLE layer.
        cmds[101] = [](IpcCtx& ctx) {
            ctx.push<uint32_t>(1u);
            log("ipc: hid GetSupportedNpadStyleSet -> NpadFullKey");
            return 0u;
        };
        // NpadHandheldActivationMode::Dual (0). The shared runtime presents
        // player 1 as a full-key controller; no Joy-Con pairing state is modeled.
        cmds[128] = [](IpcCtx& ctx) {
            const int64_t mode = ctx.arg<int64_t>(8);
            log("ipc: hid SetNpadHandheldActivationMode mode %lld (accepted)",
                (long long)mode);
            return 0u;
        };
    }
};ServiceRegistrar g_p5s_hid_registrar("hid", [] { return std::make_shared<P5SHidServer>(); });
}  // namespace
"""
if ipc_text.count(registrar_anchor) != 1:
    raise SystemExit("ipc.cpp: register_service insertion point not found uniquely")
ipc_text = ipc_text.replace(registrar_anchor, registrar_new);
handle_anchor = """uint32_t Service::handle(IpcCtx& ctx) {
    auto it = cmds.find(ctx.cmd);"""
handle_new = """uint32_t Service::handle(IpcCtx& ctx) {
    if (name == "hid") {
        static std::atomic<uint32_t> hid_requests{0};
        const uint32_t n = ++hid_requests;
        if (n <= 128) {
            log("ipc: HID request #%u cmd %u type %u data %zu desc A/B/X/C %zu/%zu/%zu/%zu objects %zu/%zu",
                n, ctx.cmd, ctx.type, ctx.in_size, ctx.a.size(), ctx.b.size(),
                ctx.x.size(), ctx.c.size(), ctx.in_objects.size(), ctx.in_services.size());
        }
    }
    auto it = cmds.find(ctx.cmd);"""
if ipc_text.count(handle_anchor) != 1:
    raise SystemExit("ipc.cpp: Service::handle instrumentation point not found uniquely")
ipc_cpp.write_text(ipc_text.replace(handle_anchor, handle_new))

hid_cpp = root / "runtime/src/hle_hid.cpp"
hid_text = hid_cpp.read_text()
touch_anchor = """void GetTouchScreenState(CPU& c) {
    st<int64_t>(c.x[0], int64_t(g_sampling));
    st<int32_t>(c.x[0] + 8, 0);
}"""
touch_new = """void GetTouchScreenState1(CPU& c) {
    static std::atomic<uint32_t> touch_reads{0};
    const uint32_t n = ++touch_reads;
    const uint64_t out = c.x[0];
    constexpr uint64_t kTouchState1Size = 0x38;
    const VmInfo region = vm_query(out);
    const bool in_region = out >= region.addr && out - region.addr <= region.size &&
                           kTouchState1Size <= region.size - (out - region.addr);
    const bool writable = in_region && (region.perm & 2u) != 0 &&
                          region.state != kMemFree && region.state != kMemInaccessible;
    if (n <= 32) {
        log("hid: GetTouchScreenState<1> #%u out=%s addr=%llx size=%llx state=%#x perm=%#x writable=%s sampling=%llu",
            n, describe(out).c_str(), (unsigned long long)region.addr,
            (unsigned long long)region.size, region.state, region.perm,
            writable ? "yes" : "no", (unsigned long long)g_sampling);
    }
    if (!writable) {
        if (n <= 32) log("hid: refusing SDK-1 touchscreen write to unmapped/read-only output");
        return;
    }
    std::memset(reinterpret_cast<void*>(out), 0, kTouchState1Size);
    st<int64_t>(out, int64_t(g_sampling));
}

void GetTouchScreenState(CPU& c) {
    static std::atomic<uint32_t> touch_reads{0};
    const uint32_t n = ++touch_reads;
    const uint64_t out = c.x[0];
    constexpr uint64_t kTouchState16Size = 0x290;
    const VmInfo region = vm_query(out);
    const bool in_region = out >= region.addr && out - region.addr <= region.size &&
                           kTouchState16Size <= region.size - (out - region.addr);
    const bool writable = in_region && (region.perm & 2u) != 0 &&
                          region.state != kMemFree && region.state != kMemInaccessible;
    if (n <= 32) {
        log("hid: GetTouchScreenState #%u out=%s addr=%llx size=%llx state=%#x perm=%#x writable=%s sampling=%llu",
            n, describe(out).c_str(), (unsigned long long)out,
            (unsigned long long)region.size, region.state, region.perm,
            writable ? "yes" : "no", (unsigned long long)g_sampling);
    }
    // Never dereference a guest pointer unless the complete output range is
    // represented by one writable guest-memory region.
    if (!writable) {
        if (n <= 32) log("hid: refusing touchscreen write to unmapped/read-only output");
        return;
    }
    std::memset(reinterpret_cast<void*>(out), 0, kTouchState16Size);
    st<int64_t>(out, int64_t(g_sampling));
}"""
if hid_text.count(touch_anchor) != 1:
    raise SystemExit("hle_hid.cpp: GetTouchScreenState instrumentation point not found uniquely")
hid_cpp.write_text(hid_text.replace(touch_anchor, touch_new))

print("Added narrow HID server handlers for observed commands 101/128 and a fully initialized empty touch state; bounded diagnostics remain enabled.")

# The pinned upstream runtime already exports the templated SDK-16 symbol.
# Reload the file after the touch-handler rewrite; otherwise the later HLE registration
# would write the stale pre-rewrite buffer and silently discard GetTouchScreenState1.
hid_text = hid_cpp.read_text()
# Its Itanium ABI mangling is important: the non-template spelling is NOT equivalent.
hid_register_16 = 'NX_HLE("_ZN2nn3hid19GetTouchScreenStateILm16EEEvPNS0_16TouchScreenStateIXT_EEE", GetTouchScreenState);'
hid_register_1 = 'NX_HLE("_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE", GetTouchScreenState1);'
if hid_text.count(hid_register_16) != 1:
    raise SystemExit("hle_hid.cpp: expected templated GetTouchScreenState<16> HLE export is missing or duplicated")
if hid_text.count(hid_register_1) != 0:
    raise SystemExit("hle_hid.cpp: templated GetTouchScreenState<1> HLE export already exists unexpectedly")
hid_text = hid_text.replace(hid_register_16, hid_register_16 + "\n" + hid_register_1)
hid_cpp.write_text(hid_text)

print("Registered both templated GetTouchScreenState<16> and the P5S-observed <1> specialization.")


# Rebuild trigger: validate GetTouchScreenState output against the guest VM map before writing.


# Rebuild trigger: register and safely handle the SDK-1 touchscreen specialization observed in the device log.



# Route nn::fs::CommitSaveData through the runtime's host-backed save mount.
# The SDK implementation calls into fsp-srv internally; that path is not
# intercepted by the imported nn::fs::Commit HLE and fails with ResultNotMounted
# because this Android runtime represents mounts in g_mounts instead of fsp-srv.
# Intercept the outer SDK entry point imported by the game and commit the
# already-flushed host files for that registered mount. Never report success
# for an unknown mount or a failed host flush.
fs_cpp = root / "runtime/src/hle_fs.cpp"
fs_text = fs_cpp.read_text()
if "#include <fcntl.h>" not in fs_text:
    fs_text = """#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
""" + fs_text

old_commit = "void Commit(CPU& c) { c.x[0] = kOk; }"
new_commit = """void CommitSaveData(CPU& c) {
    const std::string name = cstr(c.x[0]);
    std::lock_guard lk(g_fs);
    auto mount = g_mounts.find(name);
    if (name.empty() || mount == g_mounts.end()) {
        log("fs: CommitSaveData('%s') -> ResultNotMounted (registered mounts=%zu)",
            name.c_str(), g_mounts.size());
        c.x[0] = 0x35F202;
        return;
    }

    const std::string prefix = name + ":/";
    for (const auto& [handle, file] : g_files) {
        (void)handle;
        if (!file || !file->f || file->path.compare(0, prefix.size(), prefix) != 0)
            continue;
        errno = 0;
        if (fflush(file->f) != 0 || ferror(file->f)) {
            log("fs: CommitSaveData('%s') -> host fflush failed for %s (errno=%d)",
                name.c_str(), file->path.c_str(), errno);
            c.x[0] = kPathNotFound;
            return;
        }
    }

    // A save file may already have been closed when the game commits. Flush every
    // regular file beneath the mounted save directory, not just live FILE handles.
    std::error_code ec;
    std::vector<fs::path> directories;
    directories.push_back(mount->second);
    fs::recursive_directory_iterator it(
        mount->second, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    if (ec) {
        log("fs: CommitSaveData('%s') -> cannot scan save directory %s (error=%s)",
            name.c_str(), mount->second.string().c_str(), ec.message().c_str());
        c.x[0] = kPathNotFound;
        return;
    }
    while (it != end) {
        const fs::path path = it->path();
        const bool is_directory = it->is_directory(ec);
        if (ec) break;
        const bool is_regular_file = !is_directory && it->is_regular_file(ec);
        if (ec) break;
        if (is_directory) {
            directories.push_back(path);
        } else if (is_regular_file) {
#if defined(_WIN32)
            const int fd = _open(path.string().c_str(), _O_WRONLY | _O_BINARY);
            const bool synced = fd >= 0 && _commit(fd) == 0;
            if (fd >= 0) _close(fd);
#else
            const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
            const bool synced = fd >= 0 && fsync(fd) == 0;
            if (fd >= 0) ::close(fd);
#endif
            if (!synced) {
                log("fs: CommitSaveData('%s') -> host sync failed for %s (errno=%d)",
                    name.c_str(), path.string().c_str(), errno);
                c.x[0] = kPathNotFound;
                return;
            }
        }
        it.increment(ec);
        if (ec) break;
    }
    if (ec) {
        log("fs: CommitSaveData('%s') -> save scan failed (error=%s)",
            name.c_str(), ec.message().c_str());
        c.x[0] = kPathNotFound;
        return;
    }

#if !defined(_WIN32)
    // Persist directory entries too, deepest first, so newly created save files survive
    // a process/device interruption after CommitSaveData returns.
    for (auto dir = directories.rbegin(); dir != directories.rend(); ++dir) {
        const int fd = ::open(dir->c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        const bool synced = fd >= 0 && fsync(fd) == 0;
        if (fd >= 0) ::close(fd);
        if (!synced) {
            log("fs: CommitSaveData('%s') -> directory sync failed for %s (errno=%d)",
                name.c_str(), dir->string().c_str(), errno);
            c.x[0] = kPathNotFound;
            return;
        }
    }
#endif

    log("fs: CommitSaveData('%s') -> committed host save at %s",
        name.c_str(), mount->second.string().c_str());
    c.x[0] = kOk;
}

void Commit(CPU& c) { c.x[0] = kOk; }"""
if fs_text.count(old_commit) != 1:
    raise SystemExit("hle_fs.cpp: Commit handler anchor not found uniquely")
fs_text = fs_text.replace(old_commit, new_commit)
old_reg = 'NX_HLE("_ZN2nn2fs6CommitEPKc", Commit);'
new_reg = old_reg + '\nNX_HLE("_ZN2nn2fs14CommitSaveDataEPKc", CommitSaveData);'
if fs_text.count(old_reg) != 1:
    raise SystemExit("hle_fs.cpp: Commit HLE registration anchor not found uniquely")
fs_text = fs_text.replace(old_reg, new_reg)
fs_cpp.write_text(fs_text)
print("Installed host-backed CommitSaveData HLE; removed temporary save diagnostics.")

# Instrument the host-backed file operations. This does not claim to intercept
# CommitSaveData when the SDK calls it internally; that needs a build-ID-verified
# NCE hook, not an import HLE registration.
fs_anchor = "void WriteFile(CPU& c) {  // (FileHandle, s64 off, const void* buf, size_t size, const WriteOption&)\n    auto file = open_file(c.x[0]);\n    if (!file || !file->f) {  // the game image is read only\n        c.x[0] = kTargetNotFound;\n        return;\n    }\n    std::lock_guard lk(g_fs);\n    plat::fseek64(file->f, int64_t(c.x[1]), SEEK_SET);\n    fwrite(reinterpret_cast<const void*>(c.x[2]), 1, c.x[3], file->f);\n    fflush(file->f);  // positional reads bypass the FILE buffer (saves only: rare and small)\n    c.x[0] = kOk;\n}\n\nvoid FlushFile(CPU& c) {\n    if (auto file = open_file(c.x[0]); file && file->f) {\n        std::lock_guard lk(g_fs);\n        fflush(file->f);\n    }\n    c.x[0] = kOk;\n}"
fs_replacement = "void WriteFile(CPU& c) {  // (FileHandle, s64 off, const void* buf, size_t size, const WriteOption&)\n    auto file = open_file(c.x[0]);\n    if (!file || !file->f) {\n        c.x[0] = kTargetNotFound;\n        return;\n    }\n    std::lock_guard lk(g_fs);\n    errno = 0;\n    const int seek_result = plat::fseek64(file->f, int64_t(c.x[1]), SEEK_SET);\n    const size_t written = seek_result == 0\n        ? std::fwrite(reinterpret_cast<const void*>(c.x[2]), 1, size_t(c.x[3]), file->f)\n        : 0;\n    const int flush_result = seek_result == 0 && written == size_t(c.x[3])\n        ? std::fflush(file->f) : -1;\n    const int saved_errno = errno;\n    if (seek_result != 0 || written != size_t(c.x[3]) || flush_result != 0) {\n        log(\"fs: WriteFile failed %s offset=%lld requested=%llu written=%llu seek=%d flush=%d errno=%d\",\n            file->path.c_str(), (long long)c.x[1], (unsigned long long)c.x[3],\n            (unsigned long long)written, seek_result, flush_result, saved_errno);\n        c.x[0] = kPathNotFound;\n        return;\n    }\n    if (trace_fs() || file->path.find(\"user/save\") != std::string::npos)\n        log(\"fs: WriteFile %s offset=%lld bytes=%llu\",\n            file->path.c_str(), (long long)c.x[1], (unsigned long long)written);\n    c.x[0] = kOk;\n}\n\nvoid FlushFile(CPU& c) {\n    auto file = open_file(c.x[0]);\n    if (!file) {\n        c.x[0] = kTargetNotFound;\n        return;\n    }\n    if (!file->f) {\n        c.x[0] = kOk;\n        return;\n    }\n    std::lock_guard lk(g_fs);\n    errno = 0;\n    const int result = std::fflush(file->f);\n    const int saved_errno = errno;\n    if (result != 0) {\n        log(\"fs: FlushFile failed %s errno=%d\", file->path.c_str(), saved_errno);\n        c.x[0] = kPathNotFound;\n        return;\n    }\n    if (trace_fs()) log(\"fs: FlushFile %s -> ok\", file->path.c_str());\n    c.x[0] = kOk;\n}"
if fs_text.count(fs_anchor) != 1:
    raise SystemExit("hle_fs.cpp: WriteFile/FlushFile diagnostic anchor not found uniquely")
fs_text = fs_text.replace(fs_anchor, fs_replacement)
fs_cpp.write_text(fs_text)
print("Added save-write diagnostics and stopped masking host write/flush failures.")
