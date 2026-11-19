// Application settings and redmp1.ini persistence.

#ifndef REDMP1_APP_SETTINGS_H
#define REDMP1_APP_SETTINGS_H

#include "repsp/gpu/render_backend.h"
#include "repsp/platform/display.h"

#include <filesystem>
#include <string>

namespace redmp1
{

struct Settings
{
    int window_scale = 2; // 1..6
    bool fullscreen = false;
    repsp::platform::VsyncMode vsync = repsp::platform::VsyncMode::kOff;
    repsp::platform::AspectMode aspect = repsp::platform::AspectMode::kOriginal;
    repsp::platform::OutputFilter output_filter = repsp::platform::OutputFilter::kNearest;
    int render_scale = 2; // 1..8
    int msaa = 0;         // 0, 2, 4, 8
    repsp::gpu::TextureFilterMode texture_filter = repsp::gpu::TextureFilterMode::kOriginal;
    std::string post_shader = "none";

    bool show_fps = false;
    bool show_memory = false;
    bool show_gpu_memory = false;
    float stats_x = -1.0f;
    float stats_y = -1.0f;

    int music_offset_ms = 0; // -300..300
};

// Reads and writes redmp1.ini next to the executable, falling back to the
// user's data directory if the executable directory is not writable or if a
// file already exists only in the fallback location.
class SettingsFile
{
public:
    SettingsFile();
    SettingsFile(std::filesystem::path primary_path, std::filesystem::path fallback_path);

    Settings Load();
    bool Save(const Settings& settings);

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    std::filesystem::path fallback_path_;
};

} // namespace redmp1

#endif // REDMP1_APP_SETTINGS_H
