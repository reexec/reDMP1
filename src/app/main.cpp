#include "redmp1/dmp1_profile.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if REDMP1_HAVE_RECOMP
#include "recomp_abi.h"
#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <repsp/platform/guarded_run.h>
#include <repsp/recomp/guest_cpu.h>
#include <repsp/runtime/guest_memory.h>
#include <repsp/runtime/prx_loader.h>
#include <setjmp.h>

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
              << "  --lift-check           Execute recompiled code up to the first firmware call\n"
              << "  --help, -h             Show this help message\n"
              << "  --version, -v          Show version\n";
}

#if REDMP1_HAVE_RECOMP
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

const char* LookupPublicNidName(const std::string& lib, uint32_t nid)
{
    if (lib == "ThreadManForUser")
    {
        if (nid == 0x446D8DE6) return "sceKernelCreateThread";
        if (nid == 0xF475845D) return "sceKernelStartThread";
    }
    return nullptr;
}

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#endif
static void InvokeGuestWithSetjmp(const repsp_guest_cpu_ops* ops, uint32_t entry, jmp_buf* jmp)
{
    if (setjmp(*jmp) == 0)
    {
        ops->call_guest(entry);
    }
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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

    // Setup stop on first HLE call
    recomp_abi_set_stop_on_hle(1);
    jmp_buf* jmp = static_cast<jmp_buf*>(recomp_abi_get_hle_jmp_buf());

    std::cout << "[lift-check] Running guest code in GuardedRun from entry 0x" << std::hex << loaded->entry << std::dec << "...\n";

    auto guard_res = repsp::platform::GuardedRun([&]() {
        InvokeGuestWithSetjmp(ops, loaded->entry, jmp);
    });

    if (!guard_res.succeeded && guard_res.native_exception_code != 0)
    {
        std::cerr << "[lift-check] Error: guest execution faulted: " << guard_res.description << "\n";
        return 1;
    }

    uint32_t first_nid = recomp_abi_get_first_hle_nid();
    if (first_nid == 0)
    {
        std::cerr << "[lift-check] Warning: guest returned without making any firmware calls.\n";
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

    const char* func_name = LookupPublicNidName(lib_name, first_nid);

    std::cout << "\n[lift-check] === FIRST FIRMWARE CALL STOP REPORT ===\n"
              << "  Status:      STOPPED_ON_FIRST_HLE_CALL\n"
              << "  Library:     " << lib_name << "\n"
              << "  NID:         0x" << std::hex << first_nid << std::dec << "\n";
    if (func_name)
    {
        std::cout << "  Name:        " << func_name << " (source: public PSP SDK NID database)\n";
    }
    else
    {
        std::cout << "  Name:        (unnamed/unknown)\n";
    }
    std::cout << "  Dispatches:  " << psp_dispatch_calls() << "\n"
              << "================================================\n";

    return 0;
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
    bool lift_check = false;

    for (int i = 2; i < argc; ++i)
    {
        std::string opt = argv[i];
        if (opt == "--variants" && i + 1 < argc)
        {
            variants_path = argv[++i];
        }
        else if (opt == "--lift-check")
        {
            lift_check = true;
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

    return 0;
}
