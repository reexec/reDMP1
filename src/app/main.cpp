#include "redmp1/dmp1_profile.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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
              << "  --help, -h             Show this help message\n"
              << "  --version, -v          Show version\n";
}

std::filesystem::path FindDefaultVariantsJson(const std::filesystem::path& app_dir)
{
    std::vector<std::filesystem::path> candidates = {
        app_dir / "variants.json",
        app_dir / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        app_dir / ".." / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        app_dir / ".." / ".." / "src" / "game" / "dmp1" / "recomp" / "variants.json",
        app_dir / ".." / ".." / ".." / "src" / "game" / "dmp1" / "recomp" / "variants.json",
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

    for (int i = 2; i < argc; ++i)
    {
        std::string opt = argv[i];
        if (opt == "--variants" && i + 1 < argc)
        {
            variants_path = argv[++i];
        }
    }

    if (variants_path.empty())
    {
        std::filesystem::path app_dir = std::filesystem::absolute(argv[0]).parent_path();
        variants_path = FindDefaultVariantsJson(app_dir);
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

    return 0;
}
