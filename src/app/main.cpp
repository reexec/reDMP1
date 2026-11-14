#include "redmp1/dmp1_profile.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if REDMP1_HAVE_RECOMP
#include <atomic>
#include "recomp_abi.h"
#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <repsp/codec/loader.h>
#include <repsp/hle/atrac.h>
#include <repsp/hle/audio.h>
#include <repsp/hle/ctrl.h>
#include <repsp/hle/display.h>
#include <repsp/hle/dmac.h>
#include <repsp/hle/ge.h>
#include <repsp/hle/kernel.h>
#include <repsp/hle/mpeg.h>
#include <repsp/hle/sascore.h>
#include <repsp/hle/utility.h>
#include <repsp/platform/guarded_run.h>
#include <repsp/platform/path.h>
#include <repsp/platform/input.h>
#include <repsp/platform/path.h>
#include <repsp/platform/platform.h>
#include <repsp/recomp/guest_cpu.h>
#include <repsp/runtime/guest_memory.h>
#include <repsp/runtime/nid_names.h>
#include <repsp/runtime/nid_registry.h>
#include <repsp/runtime/prx_loader.h>
#include <repsp/runtime/scheduler.h>
#include <repsp/runtime/vblank_clock.h>

extern "C" void psp_recomp_register(void);
extern "C" uint64_t psp_dispatch_calls(void);
#endif

#ifndef REDMP1_VERSION
#define REDMP1_VERSION "0.0.0"
#endif

namespace
{

void PrintUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " <game_directory> [options]\n\n"
              << "Arguments:\n"
              << "  <game_directory>       Path to extracted game directory (containing PSP_GAME/)\n\n"
              << "Options:\n"
              << "  --variants <path>      Path to variants.json (default: search adjacent/source)\n"
              << "  --headless             Run without opening a display window\n"
              << "  --max-vblanks <n>      Stop execution after <n> VBlanks (0 = unlimited)\n"
              << "  --input-script <path>  Deterministic controller script (<vblank> <button names>)\n"
              << "  --save-dir <path>      Directory for sceUtility savedata slots (default: user data/savedata)\n"
              << "  --dialog-answer <ans>  Auto answer for Yes/No utility dialogs: yes or no (default: yes)\n"
              << "  --codec-plugin <path>  Path to optional codec plugin shared library\n"
              << "  --no-codec-plugin      Disable optional codec plugin (use silent/blank)\n"
              << "  --lift-check           Execute recompiled code up to the first firmware call\n"
              << "  --help, -h             Show this help message\n"
              << "  --version, -v          Show version\n";
}

#if REDMP1_HAVE_RECOMP
struct InputScriptEntry
{
    uint32_t vblank = 0;
    uint32_t buttons = 0;
};

bool ParseInputButtonToken(const std::string& raw, uint32_t& mask_out)
{
    std::string tok;
    tok.reserve(raw.size());
    for (unsigned char c : raw)
    {
        tok.push_back(static_cast<char>(std::toupper(c)));
    }
    if (tok == "NONE" || tok == "CLEAR" || tok == "0")
    {
        mask_out = 0u;
        return true;
    }
    if (tok == "SELECT") { mask_out = repsp::platform::kPspCtrlSelect; return true; }
    if (tok == "START") { mask_out = repsp::platform::kPspCtrlStart; return true; }
    if (tok == "UP") { mask_out = repsp::platform::kPspCtrlUp; return true; }
    if (tok == "RIGHT") { mask_out = repsp::platform::kPspCtrlRight; return true; }
    if (tok == "DOWN") { mask_out = repsp::platform::kPspCtrlDown; return true; }
    if (tok == "LEFT") { mask_out = repsp::platform::kPspCtrlLeft; return true; }
    if (tok == "L" || tok == "LTRIGGER" || tok == "L_TRIGGER") { mask_out = repsp::platform::kPspCtrlLTrigger; return true; }
    if (tok == "R" || tok == "RTRIGGER" || tok == "R_TRIGGER") { mask_out = repsp::platform::kPspCtrlRTrigger; return true; }
    if (tok == "TRIANGLE") { mask_out = repsp::platform::kPspCtrlTriangle; return true; }
    if (tok == "CIRCLE") { mask_out = repsp::platform::kPspCtrlCircle; return true; }
    if (tok == "CROSS") { mask_out = repsp::platform::kPspCtrlCross; return true; }
    if (tok == "SQUARE") { mask_out = repsp::platform::kPspCtrlSquare; return true; }
    return false;
}

bool LoadInputScript(const std::filesystem::path& path, std::vector<InputScriptEntry>& entries)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        std::cerr << "[boot] Error: failed to open input script: " << path << "\n";
        return false;
    }

    entries.clear();
    std::string line;
    size_t line_no = 0;
    while (std::getline(in, line))
    {
        ++line_no;
        const size_t hash_pos = line.find('#');
        if (hash_pos != std::string::npos)
        {
            line.erase(hash_pos);
        }
        std::istringstream iss(line);
        std::string vblank_tok;
        if (!(iss >> vblank_tok))
        {
            continue;
        }
        char* end_ptr = nullptr;
        unsigned long vblank_val = std::strtoul(vblank_tok.c_str(), &end_ptr, 10);
        if (vblank_tok[0] == '-' || end_ptr == nullptr || *end_ptr != '\0')
        {
            std::cerr << "[boot] Error: invalid vblank on script line " << line_no << ": " << vblank_tok << "\n";
            return false;
        }

        uint32_t mask = 0u;
        std::string btn_tok;
        while (iss >> btn_tok)
        {
            uint32_t single = 0u;
            if (!ParseInputButtonToken(btn_tok, single))
            {
                std::cerr << "[boot] Error: unknown button '" << btn_tok << "' on script line " << line_no << "\n";
                return false;
            }
            mask |= single;
        }
        entries.push_back(InputScriptEntry{static_cast<uint32_t>(vblank_val), mask});
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [](const InputScriptEntry& a, const InputScriptEntry& b) { return a.vblank < b.vblank; });

    // Coalesce duplicate vblank timestamps by keeping the last entry written for that vblank.
    std::vector<InputScriptEntry> dedup;
    for (const auto& entry : entries)
    {
        if (!dedup.empty() && dedup.back().vblank == entry.vblank)
        {
            dedup.back().buttons = entry.buttons;
        }
        else
        {
            dedup.push_back(entry);
        }
    }
    entries = std::move(dedup);
    return true;
}
std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return {};
    return buffer;
}

uint32_t ParseStoppedNidFromReason(const std::string& reason)
{
    if (reason.rfind("Unimplemented NID ", 0) != 0)
    {
        return 0;
    }
    auto hex_pos = reason.find("0x");
    if (hex_pos == std::string::npos)
    {
        hex_pos = reason.find("0X");
    }
    if (hex_pos == std::string::npos)
    {
        return 0;
    }
    try
    {
        return static_cast<uint32_t>(std::stoul(reason.substr(hex_pos + 2), nullptr, 16));
    }
    catch (...)
    {
        return 0;
    }
}

int RunLiftCheck(const std::filesystem::path& game_dir, const redmp1::IdentifyResult& idResult)
{
    std::cout << "[lift-check] Starting lift & link pre-flight and guest entry check...\n";

    uint32_t base = 0x08804000u;
    std::filesystem::path exe_path = game_dir / "PSP_GAME" / "SYSDIR" / idResult.executable_name;
    auto exe_bytes = ReadBinaryFile(exe_path);
    if (exe_bytes.empty())
    {
        std::cerr << "[lift-check] Error: failed to read executable: " << exe_path << "\n";
        return 1;
    }

    repsp::runtime::GuestMemory& memory = repsp::runtime::GetGuestMemory();
    if (!memory.Initialize())
    {
        std::cerr << "[lift-check] Error: failed to initialize guest memory\n";
        return 1;
    }

    std::string loadErr;
    auto loaded = repsp::runtime::LoadPrx(exe_bytes, base, memory, &loadErr);
    if (!loaded.has_value())
    {
        std::cerr << "[lift-check] Error: LoadPrx failed: " << loadErr << "\n";
        return 1;
    }

    // Pre-checks
    uint32_t expected_entry = base + 0x0128u;
    if (loaded->entry != expected_entry)
    {
        std::cerr << "[lift-check] Error: entry point mismatch! LoadPrx=0x" << std::hex
                  << loaded->entry << ", expected=0x" << expected_entry << std::dec << "\n";
        return 1;
    }
    std::cout << "[lift-check] Pre-check: entry point matches 0x" << std::hex << loaded->entry << std::dec << "\n";
    std::cout << "[lift-check] Pre-check: $gp is 0x" << std::hex << loaded->gp << std::dec << "\n";
    std::cout << "[lift-check] Pre-check: " << loaded->imports.size() << " import stubs verified\n";

    // Register ABI CPU ops and generated functions
    recomp_abi_init();
    psp_recomp_register();
    const repsp_guest_cpu_ops* ops = repsp_recomp_get_cpu_ops();
    if (!ops)
    {
        std::cerr << "[lift-check] Error: CPU ops not registered\n";
        return 1;
    }

    // Set initial register state
    ops->set_gpr(PSP_REG_GP, loaded->gp);
    uint32_t temp_sp = 0x09FFF000u - (64 * 1024);
    ops->set_gpr(PSP_REG_SP, temp_sp);
    ops->set_gpr(PSP_REG_A0, 0);
    ops->set_gpr(PSP_REG_A1, 0);
    ops->set_gpr(PSP_REG_RA, 0);

    // Setup stop on first unimplemented HLE call via repsp NidRegistry
    repsp::runtime::GetNidRegistry().SetStopOnUnimplemented(true);

    std::cout << "[lift-check] Running guest code in GuardedRun from entry 0x" << std::hex << loaded->entry << std::dec << "...\n";

    auto guard_res = repsp::platform::GuardedRun([&]() {
        ops->call_guest(loaded->entry);
    });

    repsp::runtime::GetNidRegistry().SetStopOnUnimplemented(false);

    if (!guard_res.stopped)
    {
        if (!guard_res.succeeded)
        {
            std::cerr << "[lift-check] Error: guest execution faulted: " << guard_res.description << "\n";
        }
        else
        {
            std::cerr << "[lift-check] Warning: guest returned without making any firmware calls.\n";
        }
        return 1;
    }

    uint32_t first_nid = ParseStoppedNidFromReason(guard_res.stop_reason);
    if (first_nid == 0)
    {
        std::cerr << "[lift-check] Error: guest stopped unexpectedly: " << guard_res.stop_reason << "\n";
        return 1;
    }

    std::string lib_name = "(unknown)";
    for (const auto& imp : loaded->imports)
    {
        if (imp.nid == first_nid)
        {
            lib_name = imp.library;
            break;
        }
    }

    const repsp::runtime::NidNameEntry* known_entry = repsp::runtime::LookupKnownNid(first_nid);

    std::cout << "\n[lift-check] === FIRST FIRMWARE CALL STOP REPORT ===\n"
              << "  Status:      STOPPED_ON_FIRST_HLE_CALL\n"
              << "  Library:     " << lib_name << "\n"
              << "  NID:         0x" << std::hex << first_nid << std::dec << "\n";
    if (known_entry)
    {
        std::cout << "  Name:        " << known_entry->name << " (source: SHA-1 PSPSDK NID table)\n";
    }
    else
    {
        std::cout << "  Name:        (unnamed/unknown)\n";
    }
    std::cout << "  Dispatches:  " << psp_dispatch_calls() << "\n"
              << "================================================\n";

    return 0;
}

bool IsKernelLibrary(const std::string& lib)
{
    static const char* const kKernelLibs[] = {
        "SysMemUserForUser",
        "ThreadManForUser",
        "IoFileMgrForUser",
        "ModuleMgrForUser",
        "LoadExecForUser",
        "UtilsForUser",
        "StdioForUser",
        "Kernel_Library",
        "scePower",
        "sceSuspendForUser",
        "sceUmdUser",
    };
    for (const char* k : kKernelLibs)
    {
        if (lib == k)
        {
            return true;
        }
    }
    return false;
}

int RunBoot(const std::filesystem::path& game_dir,
            const redmp1::IdentifyResult& idResult,
            bool headless,
            uint32_t max_vblanks,
            const std::filesystem::path& input_script_path,
            const std::filesystem::path& save_dir,
            repsp::hle::utility::DialogAnswer yes_no_answer,
            const std::filesystem::path& codec_plugin_path,
            bool no_codec_plugin)
{
    std::vector<InputScriptEntry> input_script;
    if (!input_script_path.empty())
    {
        if (!LoadInputScript(input_script_path, input_script))
        {
            return 1;
        }
    }

    uint32_t base = 0x08804000u;
    std::filesystem::path exe_path = game_dir / "PSP_GAME" / "SYSDIR" / idResult.executable_name;
    auto exe_bytes = ReadBinaryFile(exe_path);
    if (exe_bytes.empty())
    {
        std::cerr << "[boot] Error: failed to read executable: " << exe_path << "\n";
        return 1;
    }

    repsp::runtime::GuestMemory& memory = repsp::runtime::GetGuestMemory();
    if (!memory.Initialize())
    {
        std::cerr << "[boot] Error: failed to initialize guest memory\n";
        return 1;
    }

    std::string loadErr;
    auto loaded = repsp::runtime::LoadPrx(exe_bytes, base, memory, &loadErr);
    if (!loaded.has_value())
    {
        std::cerr << "[boot] Error: LoadPrx failed: " << loadErr << "\n";
        return 1;
    }

    recomp_abi_init();
    psp_recomp_register();
    if (!repsp_recomp_get_cpu_ops())
    {
        std::cerr << "[boot] Error: CPU ops not registered\n";
        return 1;
    }

    std::filesystem::path ms0_dir = repsp::platform::GetUserDataDirectory("redmp1") / "ms0";
    std::error_code ec;
    std::filesystem::create_directories(ms0_dir, ec);

    repsp::platform::PlatformConfig plat_cfg;
    plat_cfg.headless = headless;
    plat_cfg.display.width = 480 * 2;
    plat_cfg.display.height = 272 * 2;
    plat_cfg.display.title = "reDMP1 - DJMAX Portable";
    plat_cfg.display.dummy = headless;
    plat_cfg.audio.dummy = headless;
    if (!repsp::platform::Initialize(plat_cfg) && !headless)
    {
        plat_cfg.headless = true;
        plat_cfg.display.dummy = true;
        plat_cfg.audio.dummy = true;
        repsp::platform::Initialize(plat_cfg);
    }

    repsp::hle::kernel::KernelConfig cfg;
    cfg.mounts = {
        {"disc0", game_dir, true},
        {"umd0", game_dir, true},
        {"ms0", ms0_dir, false},
    };
    cfg.module_image_base = base;
    cfg.module_image_size = loaded->image_end - base;
    cfg.module_gp = loaded->gp;
    cfg.entry_address = loaded->entry;
    cfg.first_thread_name = "module_start";
    cfg.first_thread_priority = 32;
    cfg.first_thread_stack_size = 262144;
    cfg.first_thread_attr = 0;

    repsp::hle::kernel::Register(cfg);
    repsp::hle::ge::Register();

    repsp::hle::display::DisplayHleConfig disp_cfg;
    disp_cfg.headless = headless;
    disp_cfg.scale = 2;
    disp_cfg.window_title = "reDMP1 - DJMAX Portable";
    repsp::hle::display::Register(disp_cfg);
    repsp::hle::ctrl::Register();
    if (!input_script.empty())
    {
        repsp::hle::ctrl::SetInputSource([input_script](uint32_t vcount)
        {
            repsp::platform::ControllerState state{};
            for (const auto& entry : input_script)
            {
                if (entry.vblank <= vcount)
                {
                    state.buttons = entry.buttons;
                }
                else
                {
                    break;
                }
            }
            return state;
        });
    }
    repsp::hle::dmac::Register();
    repsp::hle::audio::Register();
    repsp::hle::sascore::Register();
    repsp::hle::atrac::Register();
    repsp::hle::mpeg::Register();

    std::unique_ptr<repsp::codec::LoadedCodecPlugin> codec_plugin;
    if (!no_codec_plugin)
    {
        const bool explicit_path = !codec_plugin_path.empty();
        const std::filesystem::path resolved_path = explicit_path
            ? codec_plugin_path
            : (repsp::platform::GetExecutableDirectory() / repsp::codec::DefaultCodecPluginFilename());
        std::error_code fs_ec;
        if (explicit_path || std::filesystem::exists(resolved_path, fs_ec))
        {
            std::string plugin_err;
            codec_plugin = repsp::codec::LoadCodecPlugin(resolved_path, &plugin_err);
            if (codec_plugin != nullptr && codec_plugin->IsLoaded())
            {
                std::cout << "[redmp1] Codec plugin loaded: " << codec_plugin->Name()
                          << " (" << codec_plugin->Description() << ")\n";
                if (auto* atrac_dec = codec_plugin->GetAtracDecoder())
                {
                    repsp::hle::atrac::SetDecoder(atrac_dec);
                    repsp::hle::mpeg::SetAtracDecoder(atrac_dec);
                }
                if (auto* avc_dec = codec_plugin->GetAvcDecoder())
                {
                    repsp::hle::mpeg::SetAvcDecoder(avc_dec);
                }
            }
            else
            {
                std::cout << "[redmp1] Codec plugin unavailable (" << plugin_err
                          << "); falling back to silent/blank\n";
            }
        }
        else
        {
            std::cout << "[redmp1] Codec plugin not found ("
                      << repsp::codec::DefaultCodecPluginFilename().string()
                      << "); using silent/blank fallback\n";
        }
    }

    repsp::hle::utility::Config utility_cfg;
    utility_cfg.save_root = save_dir.empty()
                                ? (repsp::platform::GetUserDataDirectory("redmp1") / "savedata")
                                : save_dir;
    repsp::hle::utility::Register(utility_cfg);
    repsp::hle::utility::SetDialogPolicy(
        [yes_no_answer](repsp::hle::utility::DialogKind,
                        const std::string&,
                        repsp::hle::utility::ChoiceKind choice)
        {
            if (choice == repsp::hle::utility::ChoiceKind::kYesNo)
            {
                return yes_no_answer;
            }
            return repsp::hle::utility::DialogAnswer::kConfirm;
        });

    repsp::runtime::Scheduler& sched = repsp::runtime::GetScheduler();
    int32_t entry_tid = sched.CreateThread(
        cfg.first_thread_name,
        cfg.entry_address,
        cfg.first_thread_priority,
        cfg.first_thread_stack_size,
        cfg.first_thread_attr,
        cfg.module_gp);
    if (entry_tid < 0)
    {
        std::cerr << "[boot] Error: CreateThread failed: 0x" << std::hex << entry_tid << std::dec << "\n";
        repsp::hle::utility::SetDialogPolicy(nullptr);
        repsp::hle::ctrl::SetInputSource(nullptr);
        repsp::hle::utility::Shutdown();
        repsp::hle::mpeg::Shutdown();
        repsp::hle::atrac::Shutdown();
        repsp::hle::sascore::Shutdown();
        repsp::hle::audio::Shutdown();
        repsp::hle::ctrl::Shutdown();
        repsp::hle::dmac::Shutdown();
        repsp::hle::display::Shutdown();
        repsp::platform::Shutdown();
        return 1;
    }

    std::atomic<bool> vblank_limit_triggered{false};
    uint32_t vblank_limit_sub_id = 0;
    if (max_vblanks > 0)
    {
        vblank_limit_sub_id = repsp::runtime::GetVblankClock().Subscribe(
            [&sched, &vblank_limit_triggered, max_vblanks](uint32_t vcount)
            {
                if (vcount >= max_vblanks && !vblank_limit_triggered.exchange(true))
                {
                    sched.RequestStop("Reached VBlank limit (--max-vblanks)");
                }
            });
    }

    repsp::runtime::GetNidRegistry().SetStopOnUnimplemented(true);

    std::cout << "[boot] Starting guest execution at 0x" << std::hex << loaded->entry
              << " ($gp=0x" << loaded->gp << ", tid=" << std::dec << entry_tid << ")...\n";

    auto guard_res = repsp::platform::GuardedRun([&]() {
        sched.Run(entry_tid);
    });

    repsp::runtime::GetNidRegistry().SetStopOnUnimplemented(false);
    if (vblank_limit_sub_id != 0)
    {
        repsp::runtime::GetVblankClock().Unsubscribe(vblank_limit_sub_id);
    }

    const std::string ge_stats = repsp::hle::ge::FormatStats();
    const std::string disp_stats = repsp::hle::display::FormatStats();
    const std::string ctrl_stats = repsp::hle::ctrl::FormatStats();
    const std::string audio_stats = repsp::hle::audio::FormatStats();
    const std::string sas_stats = repsp::hle::sascore::FormatStats();
    const std::string atrac_stats = repsp::hle::atrac::FormatStats();
    const std::string mpeg_stats = repsp::hle::mpeg::FormatStats();
    const std::string utility_stats = repsp::hle::utility::FormatStats();
    repsp::hle::utility::SetDialogPolicy(nullptr);
    repsp::hle::ctrl::SetInputSource(nullptr);
    repsp::hle::utility::Shutdown();
    repsp::hle::mpeg::Shutdown();
    repsp::hle::atrac::Shutdown();
    repsp::hle::sascore::Shutdown();
    repsp::hle::audio::Shutdown();
    repsp::hle::ctrl::Shutdown();
    repsp::hle::dmac::Shutdown();
    repsp::hle::display::Shutdown();
    repsp::platform::Shutdown();
    codec_plugin.reset();

    int exit_code = 0;
    std::cout << "\n[boot] === BOOT STOP REPORT ===\n";
    if (guard_res.stopped)
    {
        uint32_t stop_nid = ParseStoppedNidFromReason(guard_res.stop_reason);
        if (stop_nid != 0)
        {
            std::string lib_name = "(unknown)";
            for (const auto& imp : loaded->imports)
            {
                if (imp.nid == stop_nid)
                {
                    lib_name = imp.library;
                    break;
                }
            }
            const bool is_kernel = IsKernelLibrary(lib_name);
            const std::string formatted_nid = repsp::runtime::FormatKnownNid(stop_nid);
            std::cout << "  Class:       "
                      << (is_kernel ? "(b) Unimplemented kernel NID" : "(a) Unimplemented non-kernel NID") << "\n"
                      << "  Library:     " << lib_name << "\n"
                      << "  NID:         " << formatted_nid << "\n"
                      << "  Reason:      " << guard_res.stop_reason << "\n";
            exit_code = is_kernel ? 10 : 0;
        }
        else if (guard_res.stop_reason.find("Reached VBlank limit") != std::string::npos)
        {
            std::cout << "  Class:       (e) VBlank limit reached (--max-vblanks)\n"
                      << "  Reason:      " << guard_res.stop_reason << "\n";
            exit_code = 0;
        }
        else if (guard_res.stop_reason.find("sceKernelExitGame") != std::string::npos)
        {
            std::cout << "  Class:       (a) Guest exited via sceKernelExitGame\n"
                      << "  Reason:      " << guard_res.stop_reason << "\n";
            exit_code = 0;
        }
        else
        {
            std::cout << "  Class:       (b) Unexpected runtime/HLE stop\n"
                      << "  Reason:      " << guard_res.stop_reason << "\n";
            exit_code = 11;
        }
    }
    else if (!guard_res.succeeded)
    {
        std::cout << "  Class:       (c) CPU fault / host exception\n"
                  << "  Description: " << guard_res.description << "\n";
        exit_code = 12;
    }
    else
    {
        std::cout << "  Class:       (a) All guest threads exited normally\n";
        exit_code = 0;
    }
    std::cout << "  Dispatches:  " << psp_dispatch_calls() << "\n"
              << "  Exit code:   " << exit_code << "\n"
              << "  " << disp_stats
              << "  " << ctrl_stats
              << "  " << ge_stats
              << "  Audio:       " << audio_stats << "\n"
              << "  " << sas_stats << "\n"
              << "  " << atrac_stats << "\n"
              << "  " << mpeg_stats << "\n"
              << "  " << utility_stats << "\n"
              << "==============================\n";

    repsp::hle::kernel::PrintKernelSummary();
    return exit_code;
}
#endif

std::filesystem::path FindDefaultVariantsJson(const std::filesystem::path& app_dir, const std::filesystem::path& game_dir)
{
    std::vector<std::filesystem::path> candidates = {
        app_dir / "variants.json",
        app_dir / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        app_dir / ".." / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        app_dir / ".." / ".." / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        app_dir / ".." / ".." / ".." / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        game_dir / "variants.json",
        game_dir / ".." / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        std::filesystem::current_path() / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        std::filesystem::current_path() / "variants.json",
    };

    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }
    return "";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Error: No game directory specified.\n";
        PrintUsage(argv[0]);
        return 1;
    }

    std::string arg1 = argv[1];
    if (arg1 == "--help" || arg1 == "-h")
    {
        PrintUsage(argv[0]);
        return 0;
    }
    if (arg1 == "--version" || arg1 == "-v")
    {
        std::cout << "redmp1 version " << REDMP1_VERSION << " (portable native port of DJMAX Portable)\n";
        return 0;
    }

    std::filesystem::path game_dir = arg1;
    std::filesystem::path variants_path;
    std::filesystem::path input_script_path;
    std::filesystem::path save_dir;
    std::filesystem::path codec_plugin_path;
    bool no_codec_plugin = false;
    bool dialog_answer_yes = true;
    bool lift_check = false;
    bool headless = false;
    uint32_t max_vblanks = 0;

    for (int i = 2; i < argc; ++i)
    {
        std::string opt = argv[i];
        if (opt == "--variants" && i + 1 < argc)
        {
            variants_path = argv[++i];
        }
        else if (opt == "--input-script")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --input-script requires a file path.\n";
                return 1;
            }
            input_script_path = argv[++i];
        }
        else if (opt == "--save-dir")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --save-dir requires a directory path.\n";
                return 1;
            }
            save_dir = argv[++i];
        }
        else if (opt == "--dialog-answer")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --dialog-answer requires 'yes' or 'no'.\n";
                return 1;
            }
            std::string ans = argv[++i];
            if (ans == "yes")
            {
                dialog_answer_yes = true;
            }
            else if (ans == "no")
            {
                dialog_answer_yes = false;
            }
            else
            {
                std::cerr << "Error: Invalid --dialog-answer value: " << ans
                          << " (expected 'yes' or 'no')\n";
                return 1;
            }
        }
        else if (opt == "--codec-plugin")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --codec-plugin requires a file path.\n";
                return 1;
            }
            codec_plugin_path = argv[++i];
        }
        else if (opt == "--no-codec-plugin")
        {
            no_codec_plugin = true;
        }
        else if (opt == "--lift-check")
        {
            lift_check = true;
        }
        else if (opt == "--headless")
        {
            headless = true;
        }
        else if (opt == "--max-vblanks")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: --max-vblanks requires a numeric argument.\n";
                return 1;
            }
            const char* raw = argv[++i];
            char* end_ptr = nullptr;
            unsigned long parsed = std::strtoul(raw, &end_ptr, 10);
            if (raw[0] == '\0' || raw[0] == '-' || end_ptr == nullptr || *end_ptr != '\0')
            {
                std::cerr << "Error: Invalid --max-vblanks value: " << raw << "\n";
                return 1;
            }
            max_vblanks = static_cast<uint32_t>(parsed);
        }
    }

    if (variants_path.empty())
    {
        std::filesystem::path app_dir = std::filesystem::absolute(argv[0]).parent_path();
        variants_path = FindDefaultVariantsJson(app_dir, game_dir);
    }

    std::vector<redmp1::Variant> variants;
    if (!variants_path.empty() && std::filesystem::exists(variants_path))
    {
        redmp1::LoadVariants(variants_path, variants);
    }

    redmp1::IdentifyResult result = redmp1::IdentifyGameDirectory(game_dir, variants);

    if (result.status != redmp1::IdentifyStatus::Success)
    {
        std::cerr << "Identification failed: " << result.message << "\n";
        return static_cast<int>(result.status);
    }

    std::cout << "[redmp1] Game identified successfully:\n"
              << "  Title:       " << result.title << "\n"
              << "  Disc ID:     " << result.disc_id << "\n"
              << "  Version:     " << result.disc_version << "\n"
              << "  Executable:  " << result.executable_name << " (" << result.executable_size << " bytes)\n"
              << "  SHA-1:       " << result.executable_sha1 << "\n";

    if (lift_check)
    {
#if REDMP1_HAVE_RECOMP
        return RunLiftCheck(game_dir, result);
#else
        std::cerr << "Error: redmp1 was built without generated code (--lift-check unavailable).\n";
        return 1;
#endif
    }

#if REDMP1_HAVE_RECOMP
    return RunBoot(game_dir, result, headless, max_vblanks, input_script_path, save_dir,
                   dialog_answer_yes ? repsp::hle::utility::DialogAnswer::kYes
                                     : repsp::hle::utility::DialogAnswer::kNo,
                   codec_plugin_path, no_codec_plugin);
#else
    (void)headless;
    (void)max_vblanks;
    (void)input_script_path;
    (void)save_dir;
    (void)dialog_answer_yes;
    (void)codec_plugin_path;
    (void)no_codec_plugin;
    return 0;
#endif
}
