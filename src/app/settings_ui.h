// The settings menu and the stats window drawn with Dear ImGui.

#ifndef REDMP1_APP_SETTINGS_UI_H
#define REDMP1_APP_SETTINGS_UI_H

#include "settings.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>

namespace redmp1
{

struct BuildInfo
{
    std::string version;
    std::string build_time;
    std::string identity;
    std::string commit;
    std::string game_id;
    std::string game_title;
};

class SettingsUi
{
public:
    SettingsUi(BuildInfo build, Settings settings, SettingsFile* file);

    // Overlay builder callback: runs on every present pass on the UI thread.
    void Build();

private:
    template <typename T>
    struct Tracked
    {
        T seen{};
        T expected{};
    };

    struct Sample
    {
        float fps = 0.0f;
        float memory_mib = 0.0f;
        float gpu_mib = 0.0f;
    };

    void ObserveActualState();
    void Save();
    void Record(double now);
    void TakeMemorySnapshot();
    void DrawMenu();
    void DrawTab(void (SettingsUi::*draw)());
    void DrawInfoTab();
    void DrawGraphicsTab();
    void DrawInputTab();
    void DrawStatsWindow();

    BuildInfo build_;
    Settings settings_;
    SettingsFile* file_ = nullptr;
    bool observed_ = false;
    Tracked<int> render_scale_;
    Tracked<bool> fullscreen_;
    Tracked<repsp::platform::AspectMode> aspect_;
    Tracked<repsp::platform::OutputFilter> output_filter_;
    float tab_height_ = 0.0f;  // the tallest tab's content, divided by the OSD scale
    float menu_width_ = 0.0f;  // this frame's menu width
    bool menu_was_open_ = false;
    int menu_frames_ = 0;

    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    double last_present_ = -1.0;
    int64_t bin_ = -1;
    int bin_frames_ = 0;
    double bin_time_ = 0.0;
    std::deque<Sample> history_;

    uint64_t process_memory_bytes_ = 0;
    repsp::gpu::GpuMemoryInfo gpu_memory_;

    bool stats_moved_ = false;
    float stats_drag_x_ = 0.0f;
    float stats_drag_y_ = 0.0f;
};

} // namespace redmp1

#endif // REDMP1_APP_SETTINGS_UI_H
