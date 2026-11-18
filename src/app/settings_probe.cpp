#include "settings.h"

#include "repsp/platform/post_shader_catalog.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#define PROBE_ASSERT(cond)                                                              \
    do                                                                                  \
    {                                                                                   \
        if (!(cond))                                                                    \
        {                                                                               \
            std::fprintf(stderr, "PROBE_ASSERT failed at %s:%d: %s\n",                  \
                         __FILE__, __LINE__, #cond);                                    \
            std::fflush(stderr);                                                        \
            std::exit(1);                                                               \
        }                                                                               \
    } while (0)

int main()
{
    std::error_code ec;
    const auto base = std::filesystem::temp_directory_path(ec) / "redmp1_settings_probe";
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base, ec);

    const auto primary = base / "primary" / "redmp1.ini";
    const auto fallback = base / "fallback" / "redmp1.ini";

    // 1. Round-trip save and load
    {
        redmp1::SettingsFile file(primary, fallback);
        redmp1::Settings s;
        s.window_scale = 4;
        s.fullscreen = true;
        s.vsync = repsp::platform::VsyncMode::kAdaptive;
        s.aspect = repsp::platform::AspectMode::kInteger;
        s.output_filter = repsp::platform::OutputFilter::kLinear;
        s.render_scale = 6;
        s.msaa = 4;
        s.texture_filter = repsp::gpu::TextureFilterMode::kAniso8x;
        s.post_shader = "crt";
        s.show_fps = true;
        s.show_memory = true;
        s.show_gpu_memory = true;
        s.stats_x = 0.85f;
        s.stats_y = 0.12f;
        PROBE_ASSERT(file.Save(s));

        const redmp1::Settings loaded = file.Load();
        PROBE_ASSERT(loaded.window_scale == 4);
        PROBE_ASSERT(loaded.fullscreen == true);
        PROBE_ASSERT(loaded.vsync == repsp::platform::VsyncMode::kAdaptive);
        PROBE_ASSERT(loaded.aspect == repsp::platform::AspectMode::kInteger);
        PROBE_ASSERT(loaded.output_filter == repsp::platform::OutputFilter::kLinear);
        PROBE_ASSERT(loaded.render_scale == 6);
        PROBE_ASSERT(loaded.msaa == 4);
        PROBE_ASSERT(loaded.texture_filter == repsp::gpu::TextureFilterMode::kAniso8x);
        PROBE_ASSERT(loaded.post_shader == "crt");
        PROBE_ASSERT(loaded.show_fps == true);
        PROBE_ASSERT(loaded.show_memory == true);
        PROBE_ASSERT(loaded.show_gpu_memory == true);
        PROBE_ASSERT(loaded.stats_x > 0.84f && loaded.stats_x < 0.86f);
        PROBE_ASSERT(loaded.stats_y > 0.11f && loaded.stats_y < 0.13f);
    }

    // 2. Unknown keys and malformed values preserve defaults
    {
        const auto bad_ini = base / "bad" / "redmp1.ini";
        std::filesystem::create_directories(bad_ini.parent_path(), ec);
        {
            std::ofstream out(bad_ini, std::ios::binary | std::ios::trunc);
            out << "[UnknownSection]\n"
                << "WindowScale=5\n"
                << "[Graphics]\n"
                << "UnknownKey=hello\n"
                << "WindowScale=99\n"
                << "RenderScale=-3\n"
                << "MSAA=3\n"
                << "VSync=bogus\n"
                << "Aspect=4:3\n"
                << "OutputFilter=cubic\n"
                << "TextureFilter=invalid\n"
                << "StatsX=2.5\n"
                << "Fullscreen=yes\n";
        }
        redmp1::SettingsFile file(bad_ini, fallback);
        const redmp1::Settings loaded = file.Load();
        PROBE_ASSERT(loaded.window_scale == 2);
        PROBE_ASSERT(loaded.render_scale == 2);
        PROBE_ASSERT(loaded.msaa == 0);
        PROBE_ASSERT(loaded.vsync == repsp::platform::VsyncMode::kOff);
        PROBE_ASSERT(loaded.aspect == repsp::platform::AspectMode::kOriginal);
        PROBE_ASSERT(loaded.output_filter == repsp::platform::OutputFilter::kNearest);
        PROBE_ASSERT(loaded.texture_filter == repsp::gpu::TextureFilterMode::kOriginal);
        PROBE_ASSERT(loaded.stats_x < 0.0f);
        PROBE_ASSERT(loaded.fullscreen == true);
    }

    // 3. Fallback path when primary cannot be written (primary path is an existing directory)
    {
        const auto blocked_primary = base / "blocked_dir";
        std::filesystem::create_directories(blocked_primary, ec);
        const auto fb_ini = base / "fb2" / "redmp1.ini";
        redmp1::SettingsFile file(blocked_primary, fb_ini);
        redmp1::Settings s;
        s.window_scale = 3;
        PROBE_ASSERT(file.Save(s));
        PROBE_ASSERT(file.path() == fb_ini);
        PROBE_ASSERT(std::filesystem::exists(fb_ini, ec));
        const redmp1::Settings loaded = file.Load();
        PROBE_ASSERT(loaded.window_scale == 3);
    }

    // 4. Empty PostShader= preserves default "none" and ChoosePostShader precedence
    {
        const auto empty_shader_ini = base / "empty_shader" / "redmp1.ini";
        std::filesystem::create_directories(empty_shader_ini.parent_path(), ec);
        {
            std::ofstream out(empty_shader_ini, std::ios::binary | std::ios::trunc);
            out << "[Graphics]\nPostShader=\n";
        }
        redmp1::SettingsFile file(empty_shader_ini, fallback);
        const redmp1::Settings loaded = file.Load();
        PROBE_ASSERT(loaded.post_shader == "none");

        const std::string cli = "fsr";
        const std::string ini = "crt";
        const std::string empty;
        PROBE_ASSERT(repsp::platform::ChoosePostShader(&cli, "scanline", &ini) == "fsr");
        PROBE_ASSERT(repsp::platform::ChoosePostShader(nullptr, "scanline", &ini) == "scanline");
        PROBE_ASSERT(repsp::platform::ChoosePostShader(&empty, "scanline", &ini) == "scanline");
        PROBE_ASSERT(repsp::platform::ChoosePostShader(nullptr, "", &ini) == "crt");
        PROBE_ASSERT(repsp::platform::ChoosePostShader(nullptr, nullptr, &ini) == "crt");
        PROBE_ASSERT(repsp::platform::ChoosePostShader(nullptr, nullptr, &empty) == "none");
        PROBE_ASSERT(repsp::platform::ChoosePostShader(nullptr, nullptr, nullptr) == "none");
    }

    std::filesystem::remove_all(base, ec);
    std::printf("redmp1_settings_probe: PASS\n");
    return 0;
}
