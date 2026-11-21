#include "settings_ui.h"

#include "repsp/hle/atrac.h"
#include "repsp/platform/overlay.h"
#include "repsp/platform/post_shader.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace redmp1
{

namespace
{

constexpr int kPspWidth = 480;
constexpr int kPspHeight = 272;
constexpr double kBinSeconds = 0.5;
constexpr size_t kBins = 120;
constexpr float kFpsAxisMax = 70.0f;
constexpr double kMib = 1024.0 * 1024.0;

template <typename T>
bool Follow(T actual, T* seen, T* expected, T* stored)
{
    if (actual == *seen)
    {
        return false;
    }
    *seen = actual;
    if (actual == *expected)
    {
        return false;
    }
    *expected = actual;
    *stored = actual;
    return true;
}

void Row(const char* label, const std::string& value)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
}

bool BeginInfoTable(const char* id)
{
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
    {
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 140.0f * ImGui::GetStyle().FontScaleMain);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

std::string FormatMib(uint64_t bytes)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f MiB", static_cast<double>(bytes) / kMib);
    return buffer;
}

float MemoryAxisMax(float peak)
{
    const float target = std::max(peak * 1.25f, 16.0f);
    float step = 16.0f;
    while (step * 8.0f < target)
    {
        step *= 2.0f;
    }
    return std::ceil(target / step) * step;
}

struct Tick
{
    float value;
    std::string label;
    ImU32 color;
    float thickness;
};

void DrawGraph(const std::vector<float>& values, float axis_max, const std::vector<Tick>& ticks,
               ImU32 line_color, float label_width)
{
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float graph_width = 240.0f * scale;
    const float graph_height = 64.0f * scale;
    const float pad = ImGui::GetFontSize() * 0.5f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(graph_width + label_width, graph_height + pad * 2.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float top = origin.y + pad;
    const auto y_of = [&](float value)
    {
        return top + graph_height * (1.0f - std::clamp(value, 0.0f, axis_max) / axis_max);
    };
    draw->AddRectFilled(ImVec2(origin.x, top), ImVec2(origin.x + graph_width, top + graph_height),
                        IM_COL32(0, 0, 0, 110));
    for (const Tick& tick : ticks)
    {
        const float y = y_of(tick.value);
        draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + graph_width, y), tick.color, tick.thickness);
        draw->AddText(ImVec2(origin.x + graph_width + 6.0f * scale, y - ImGui::GetFontSize() * 0.5f),
                      tick.color, tick.label.c_str());
    }
    std::vector<ImVec2> points;
    const size_t first_slot = kBins - values.size();
    for (size_t i = 0; i < values.size(); ++i)
    {
        const float x = origin.x + graph_width * (static_cast<float>(first_slot + i) + 0.5f) / kBins;
        points.push_back(ImVec2(x, y_of(values[i])));
    }
    if (points.size() >= 2)
    {
        draw->AddPolyline(points.data(), static_cast<int>(points.size()), line_color, ImDrawFlags_None, 1.5f * scale);
    }
}

uint64_t EffectiveGpuBytes(const repsp::gpu::GpuMemoryInfo& gpu)
{
    if (gpu.driver_reported && gpu.total_bytes >= gpu.available_bytes)
    {
        return gpu.total_bytes - gpu.available_bytes;
    }
    return gpu.hle_textures_bytes + gpu.hle_targets_bytes;
}

} // namespace

SettingsUi::SettingsUi(BuildInfo build, Settings settings, SettingsFile* file, app::Autoplay* autoplay)
    : build_(std::move(build)), settings_(settings), file_(file), autoplay_(autoplay)
{
}

void SettingsUi::Save()
{
    if (file_ != nullptr)
    {
        file_->Save(settings_);
    }
}

void SettingsUi::ObserveActualState()
{
    const int scale = repsp::gpu::GetInternalScale();
    const bool fullscreen = repsp::platform::IsFullscreen();
    const auto aspect = repsp::platform::GetAspectMode();
    const auto out_filter = repsp::platform::GetOutputFilter();

    if (!observed_)
    {
        observed_ = true;
        render_scale_ = {scale, scale};
        fullscreen_ = {fullscreen, fullscreen};
        aspect_ = {aspect, aspect};
        output_filter_ = {out_filter, out_filter};
        return;
    }
    bool changed = Follow(scale, &render_scale_.seen, &render_scale_.expected, &settings_.render_scale);
    changed |= Follow(fullscreen, &fullscreen_.seen, &fullscreen_.expected, &settings_.fullscreen);
    changed |= Follow(aspect, &aspect_.seen, &aspect_.expected, &settings_.aspect);
    changed |= Follow(out_filter, &output_filter_.seen, &output_filter_.expected, &settings_.output_filter);
    if (changed)
    {
        Save();
    }
}

void SettingsUi::TakeMemorySnapshot()
{
    process_memory_bytes_ = repsp::platform::GetProcessMemoryBytes();
    gpu_memory_ = repsp::gpu::GetGpuMemoryInfo();
}

void SettingsUi::Record(double now)
{
    const int64_t bin = static_cast<int64_t>(std::floor(now / kBinSeconds));
    if (bin != bin_)
    {
        if (bin_ >= 0)
        {
            TakeMemorySnapshot();
            Sample sample;
            sample.fps = bin_time_ > 0.0 ? static_cast<float>(bin_frames_ / bin_time_) : 0.0f;
            sample.memory_mib = static_cast<float>(static_cast<double>(process_memory_bytes_) / kMib);
            sample.gpu_mib = static_cast<float>(static_cast<double>(EffectiveGpuBytes(gpu_memory_)) / kMib);
            history_.push_back(sample);
            for (int64_t missing = bin_ + 1; missing < bin && history_.size() < 2 * kBins; ++missing)
            {
                Sample stalled = sample;
                stalled.fps = 0.0f;
                history_.push_back(stalled);
            }
            while (history_.size() > kBins)
            {
                history_.pop_front();
            }
        }
        bin_ = bin;
        bin_frames_ = 0;
        bin_time_ = 0.0;
    }
    if (last_present_ >= 0.0)
    {
        ++bin_frames_;
        bin_time_ += now - last_present_;
    }
    last_present_ = now;
}

void SettingsUi::Build()
{
    Record(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count());
    ObserveActualState();
    const bool menu = repsp::platform::OverlayVisible();
    if (menu && !menu_was_open_)
    {
        tab_height_ = 0.0f;
        menu_frames_ = 0;
    }
    menu_was_open_ = menu;
    if (menu)
    {
        DrawMenu();
        ++menu_frames_;
    }
    DrawStatsWindow();
    DrawAutoplayBadge();
}

void SettingsUi::DrawMenu()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    // The menu is 560 (scaled) wide, or narrower when the window is (a 1x
    // window is 480 wide); its height fits the tallest tab, up to the
    // window's height, beyond which it scrolls.
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float margin = 8.0f * scale;
    const float width = std::max(1.0f, std::min(560.0f * scale, viewport->WorkSize.x - 2.0f * margin));
    const float max_height = std::max(1.0f, viewport->WorkSize.y - 2.0f * margin);
    menu_width_ = width;
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, max_height));
    bool open = true;
    if (ImGui::Begin("reDMP1 Settings", &open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Tab (LT+RT+Y on a gamepad) closes this menu. The game keeps running without input.");
        ImGui::PopStyleColor();
        if (ImGui::BeginTabBar("tabs"))
        {
            if (ImGui::BeginTabItem("Info"))
            {
                DrawTab(&SettingsUi::DrawInfoTab);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Graphics"))
            {
                DrawTab(&SettingsUi::DrawGraphicsTab);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio"))
            {
                DrawTab(&SettingsUi::DrawAudioTab);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Input"))
            {
                DrawTab(&SettingsUi::DrawInputTab);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (!open)
    {
        repsp::platform::SetOverlayVisible(false);
    }
}

// Every tab is as tall as the tallest seen since the menu opened, so the
// menu keeps one size whichever tab is open (as in reCT3). Heights are kept
// divided by the OSD scale, so they still fit after the window changes size.
// A window's first frames are laid out before it has its width (wrapped text
// then runs very tall), so they are not measured, nor is any frame narrower
// than the menu's width.
void SettingsUi::DrawTab(void (SettingsUi::*draw)())
{
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float top = ImGui::GetCursorPosY();
    (this->*draw)();
    const float height = ImGui::GetCursorPosY() - top;
    if (menu_frames_ >= 2 && ImGui::GetWindowWidth() >= menu_width_ - 1.0f)
    {
        tab_height_ = std::max(tab_height_, height / scale);
    }
    if (height < tab_height_ * scale)
    {
        ImGui::Dummy(ImVec2(0.0f, tab_height_ * scale - height));
    }
}

void SettingsUi::DrawInfoTab()
{
    if (menu_frames_ == 0)
    {
        TakeMemorySnapshot();
    }
    const auto gl_info = repsp::platform::GetDisplayGlInfo();

    ImGui::SeparatorText("Build");
    if (BeginInfoTable("build_info"))
    {
        Row("Version", build_.version);
        Row("Built", build_.build_time);
        Row("Target", build_.identity);
        Row("Commit", build_.commit);
        Row("Game", build_.game_id + " - " + build_.game_title);
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Graphics");
    if (BeginInfoTable("gfx_info"))
    {
        char api_buf[64];
        if (gl_info.valid)
        {
            std::snprintf(api_buf, sizeof(api_buf), "OpenGL %d.%d", gl_info.major, gl_info.minor);
        }
        else
        {
            std::snprintf(api_buf, sizeof(api_buf), "OpenGL");
        }
        Row("Renderer", api_buf);
        Row("GPU", gl_info.renderer);
        Row("Vendor", gl_info.vendor);
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Memory");
    if (BeginInfoTable("mem_info"))
    {
        Row("Process", FormatMib(process_memory_bytes_));
        if (gpu_memory_.driver_reported)
        {
            const uint64_t used = gpu_memory_.total_bytes >= gpu_memory_.available_bytes
                                      ? (gpu_memory_.total_bytes - gpu_memory_.available_bytes)
                                      : 0;
            Row("GPU (Driver)", FormatMib(used) + " / " + FormatMib(gpu_memory_.total_bytes));
        }
        else
        {
            const uint64_t hle_total = gpu_memory_.hle_textures_bytes + gpu_memory_.hle_targets_bytes;
            Row("GPU (HLE est.)",
                FormatMib(hle_total) + " (tex " + FormatMib(gpu_memory_.hle_textures_bytes) + ", FBO " +
                    FormatMib(gpu_memory_.hle_targets_bytes) + ")");
        }
        ImGui::EndTable();
    }
}

void SettingsUi::DrawGraphicsTab()
{
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float combo_width = 240.0f * scale;

    // Render Scale (1..8)
    char scale_preview[48];
    const int cur_render = repsp::gpu::GetInternalScale();
    if (render_scale_.expected != cur_render)
    {
        std::snprintf(scale_preview, sizeof(scale_preview), "%dx (%dx%d) - applying",
                      render_scale_.expected, kPspWidth * render_scale_.expected, kPspHeight * render_scale_.expected);
    }
    else
    {
        std::snprintf(scale_preview, sizeof(scale_preview), "%dx (%dx%d)",
                      settings_.render_scale, kPspWidth * settings_.render_scale, kPspHeight * settings_.render_scale);
    }
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("Render Scale (Ctrl+1..8)", scale_preview))
    {
        for (int s = 1; s <= 8; ++s)
        {
            char item[48];
            std::snprintf(item, sizeof(item), "%dx (%dx%d)", s, kPspWidth * s, kPspHeight * s);
            const bool selected = (settings_.render_scale == s);
            if (ImGui::Selectable(item, selected))
            {
                settings_.render_scale = s;
                render_scale_.expected = s;
                repsp::gpu::SetInternalScale(s);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Window Scale (1..6)
    char win_preview[48];
    std::snprintf(win_preview, sizeof(win_preview), "%dx (%dx%d)",
                  settings_.window_scale, kPspWidth * settings_.window_scale, kPspHeight * settings_.window_scale);
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("Window Scale (Alt+1..6)", win_preview))
    {
        for (int s = 1; s <= 6; ++s)
        {
            char item[48];
            std::snprintf(item, sizeof(item), "%dx (%dx%d)", s, kPspWidth * s, kPspHeight * s);
            const bool selected = (settings_.window_scale == s);
            if (ImGui::Selectable(item, selected))
            {
                settings_.window_scale = s;
                settings_.fullscreen = false;
                fullscreen_.expected = false;
                repsp::platform::SetWindowScale(s);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Fullscreen
    bool fs = settings_.fullscreen;
    if (ImGui::Checkbox("Fullscreen (Alt+Enter)", &fs))
    {
        settings_.fullscreen = fs;
        fullscreen_.expected = fs;
        repsp::platform::SetFullscreen(fs);
        Save();
    }

    // VSync
    static constexpr const char* kVsyncLabels[] = {"Off", "On", "Adaptive"};
    const int vsync_idx = std::clamp(static_cast<int>(settings_.vsync), 0, 2);
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("VSync", kVsyncLabels[vsync_idx]))
    {
        for (int i = 0; i < 3; ++i)
        {
            const bool selected = (vsync_idx == i);
            if (ImGui::Selectable(kVsyncLabels[i], selected))
            {
                settings_.vsync = static_cast<repsp::platform::VsyncMode>(i);
                repsp::platform::SetVsyncMode(settings_.vsync);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Aspect Ratio
    static constexpr const char* kAspectLabels[] = {"Original (16:9)", "Stretch to Window", "Integer Scale Only"};
    const int aspect_idx = std::clamp(static_cast<int>(settings_.aspect), 0, 2);
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("Aspect Ratio", kAspectLabels[aspect_idx]))
    {
        for (int i = 0; i < 3; ++i)
        {
            const bool selected = (aspect_idx == i);
            if (ImGui::Selectable(kAspectLabels[i], selected))
            {
                settings_.aspect = static_cast<repsp::platform::AspectMode>(i);
                aspect_.expected = settings_.aspect;
                repsp::platform::SetAspectMode(settings_.aspect);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Output Filter
    static constexpr const char* kOutputLabels[] = {"Nearest", "Linear"};
    const int out_idx = std::clamp(static_cast<int>(settings_.output_filter), 0, 1);
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("Output Filter", kOutputLabels[out_idx]))
    {
        for (int i = 0; i < 2; ++i)
        {
            const bool selected = (out_idx == i);
            if (ImGui::Selectable(kOutputLabels[i], selected))
            {
                settings_.output_filter = static_cast<repsp::platform::OutputFilter>(i);
                output_filter_.expected = settings_.output_filter;
                repsp::platform::SetOutputFilter(settings_.output_filter);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // MSAA
    static constexpr int kMsaaValues[] = {0, 2, 4, 8};
    static constexpr const char* kMsaaLabels[] = {"Off", "2x", "4x", "8x"};
    int msaa_idx = 0;
    for (int i = 0; i < 4; ++i)
    {
        if (kMsaaValues[i] == settings_.msaa)
        {
            msaa_idx = i;
            break;
        }
    }
    const int eff_msaa = repsp::gpu::GetEffectiveMsaaSamples();
    char msaa_preview[48];
    if (settings_.msaa > 0 && eff_msaa != settings_.msaa)
    {
        std::snprintf(msaa_preview, sizeof(msaa_preview), "%s (using %dx)", kMsaaLabels[msaa_idx], eff_msaa);
    }
    else
    {
        std::snprintf(msaa_preview, sizeof(msaa_preview), "%s", kMsaaLabels[msaa_idx]);
    }
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("MSAA", msaa_preview))
    {
        for (int i = 0; i < 4; ++i)
        {
            const bool selected = (msaa_idx == i);
            if (ImGui::Selectable(kMsaaLabels[i], selected))
            {
                settings_.msaa = kMsaaValues[i];
                repsp::gpu::SetMsaaSamples(settings_.msaa);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Texture Filtering
    static constexpr const char* kTexLabels[] = {
        "Original (as the game sets it)", "Linear", "Anisotropic 2x", "Anisotropic 4x", "Anisotropic 8x", "Anisotropic 16x"
    };
    const int tex_idx = std::clamp(static_cast<int>(settings_.texture_filter), 0, 5);
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("Texture Filtering", kTexLabels[tex_idx]))
    {
        for (int i = 0; i < 6; ++i)
        {
            const bool selected = (tex_idx == i);
            if (ImGui::Selectable(kTexLabels[i], selected))
            {
                settings_.texture_filter = static_cast<repsp::gpu::TextureFilterMode>(i);
                repsp::gpu::SetTextureFilterMode(settings_.texture_filter);
                Save();
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    DrawPostShaderSection(combo_width);

    ImGui::SeparatorText("Stats Overlay");
    if (ImGui::Checkbox("Show FPS", &settings_.show_fps))
    {
        Save();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Show Memory", &settings_.show_memory))
    {
        Save();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Show GPU Memory", &settings_.show_gpu_memory))
    {
        Save();
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped(
        "Shortcuts: Ctrl+1..8 resolution, Alt+Enter or double click fullscreen, "
        "Alt+1..6 window size. Gamepad: LT+RT+Y menu, LT+RT+L3+R3 held 1 s quits.");
    ImGui::PopStyleColor();
}

void SettingsUi::DrawPostShaderSection(float combo_width)
{
    ImGui::SeparatorText("Screen shader");
    repsp::platform::PostShaderControl* ctrl = repsp::platform::GetPostShaderControl();
    if (ctrl == nullptr)
    {
        ImGui::BeginDisabled(true);
        ImGui::SetNextItemWidth(combo_width);
        if (ImGui::BeginCombo("Shader", repsp::platform::kPostShaderNoneId))
        {
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Unavailable: OpenGL renderer required.");
        return;
    }

    const std::string& active = ctrl->active_id();
    ImGui::SetNextItemWidth(combo_width);
    if (ImGui::BeginCombo("Shader", active.c_str()))
    {
        const bool none_selected = (active == repsp::platform::kPostShaderNoneId);
        if (ImGui::Selectable(repsp::platform::kPostShaderNoneId, none_selected))
        {
            if (ctrl->Select(repsp::platform::kPostShaderNoneId))
            {
                settings_.post_shader = repsp::platform::kPostShaderNoneId;
                Save();
            }
        }
        if (none_selected)
        {
            ImGui::SetItemDefaultFocus();
        }
        for (const auto& entry : ctrl->catalog())
        {
            const bool selected = (active == entry.id);
            if (ImGui::Selectable(entry.id.c_str(), selected))
            {
                if (ctrl->Select(entry.id))
                {
                    settings_.post_shader = entry.id;
                    Save();
                }
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        ctrl->Reload();
    }

    if (!ctrl->last_error().empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", ctrl->last_error().c_str());
    }

    auto& params = ctrl->parameters();
    if (!params.empty())
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            auto& param = params[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(combo_width);
            const char* label = param.description.empty() ? param.name.c_str() : param.description.c_str();
            ImGui::SliderFloat(label, &param.value, param.minimum, param.maximum, "%.3f");
            ImGui::PopID();
        }
        if (ImGui::Button("Reset defaults"))
        {
            for (auto& param : params)
            {
                param.value = param.initial;
            }
        }
    }

    if (!ctrl->shader_directory().empty())
    {
        ImGui::TextDisabled("Files: %s (*.glsl / *.glslp)", ctrl->shader_directory().c_str());
    }
}

void SettingsUi::DrawAudioTab()
{
    ImGui::SeparatorText("Music Timing");
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float slider_width = std::min(280.0f * scale, ImGui::GetContentRegionAvail().x * 0.65f);
    ImGui::SetNextItemWidth(slider_width);
    int offset_ms = settings_.music_offset_ms;
    if (ImGui::SliderInt("Music offset (ms)", &offset_ms, -300, 300, "%d ms"))
    {
        offset_ms = (offset_ms >= 0) ? ((offset_ms + 2) / 5) * 5 : ((offset_ms - 2) / 5) * 5;
        offset_ms = std::clamp(offset_ms, -300, 300);
        if (offset_ms != settings_.music_offset_ms)
        {
            settings_.music_offset_ms = offset_ms;
            repsp::hle::atrac::SetStartOffsetMs(static_cast<int32_t>(offset_ms));
            Save();
        }
    }
    ImGui::TextWrapped("Positive values advance music; negative values delay it. Takes effect on the next track.");
}

void SettingsUi::DrawInputTab()
{
    if (autoplay_ != nullptr)
    {
        ImGui::SeparatorText("Autoplay");
        bool enabled = autoplay_->IsEnabled();
        if (ImGui::Checkbox("Autoplay", &enabled))
        {
            autoplay_->SetEnabled(enabled);
        }
        ImGui::TextDisabled("Plays notes automatically. High scores are saved as usual.");
    }

    ImGui::SeparatorText("Menu & Window Shortcuts");
    if (BeginInfoTable("shortcut_table"))
    {
        Row("Tab / LT+RT+Y", "Open / close settings menu");
        Row("LT+RT+L3+R3 (hold 1 s)", "Quit");
        Row("Alt+1..6", "Resize window to 1x..6x (480x272)");
        Row("Alt+Enter / Double Click", "Toggle borderless fullscreen");
        Row("Ctrl+1..8", "Set GL render scale (1x..8x)");
        ImGui::EndTable();
    }

    ImGui::SeparatorText("PSP Controller Mapping");
    if (ImGui::BeginTable("input_map", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("PSP");
        ImGui::TableSetupColumn("Keyboard");
        ImGui::TableSetupColumn("Gamepad");
        ImGui::TableHeadersRow();

        auto map_row = [](const char* psp, const char* kbd, const char* pad)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(psp);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(kbd);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(pad);
        };
        map_row("D-Pad", "Arrow Keys", "D-Pad");
        map_row("Analog Stick", "I / J / K / L", "Left Stick");
        map_row("Cross", "Z", "A (South)");
        map_row("Circle", "X", "B (East)");
        map_row("Square", "A", "X (West)");
        map_row("Triangle", "S", "Y (North)");
        map_row("L Trigger", "Q", "LB");
        map_row("R Trigger", "W", "RB");
        map_row("Start", "Enter", "Start");
        map_row("Select", "Backspace", "Back");
        ImGui::EndTable();
    }
}

void SettingsUi::DrawStatsWindow()
{
    if (!settings_.show_fps && !settings_.show_memory && !settings_.show_gpu_memory)
    {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float margin = 10.0f * ImGui::GetStyle().FontScaleMain;
    if (!stats_moved_)
    {
        if (settings_.stats_x >= 0.0f && settings_.stats_y >= 0.0f &&
            viewport->WorkSize.x > 0.0f && viewport->WorkSize.y > 0.0f)
        {
            const float px = viewport->WorkPos.x + settings_.stats_x * viewport->WorkSize.x;
            const float py = viewport->WorkPos.y + settings_.stats_y * viewport->WorkSize.y;
            ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        }
        else
        {
            ImGui::SetNextWindowPos(
                ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - margin, viewport->WorkPos.y + margin),
                ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        }
    }

    ImGui::SetNextWindowBgAlpha(0.75f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        (repsp::platform::OverlayVisible() ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoInputs);

    if (ImGui::Begin("##stats_overlay", nullptr, flags))
    {
        if (repsp::platform::OverlayVisible())
        {
            if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                stats_moved_ = true;
                const ImVec2 pos = ImGui::GetWindowPos();
                const ImVec2 size = ImGui::GetWindowSize();
                if (viewport->WorkSize.x > 0.0f && viewport->WorkSize.y > 0.0f)
                {
                    stats_drag_x_ = std::clamp((pos.x + size.x - viewport->WorkPos.x) / viewport->WorkSize.x, 0.05f, 1.0f);
                    stats_drag_y_ = std::clamp((pos.y - viewport->WorkPos.y) / viewport->WorkSize.y, 0.0f, 0.95f);
                }
            }
            if (stats_moved_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                stats_moved_ = false;
                settings_.stats_x = stats_drag_x_;
                settings_.stats_y = stats_drag_y_;
                Save();
            }
        }

        const float scale = ImGui::GetStyle().FontScaleMain;
        const float label_width = 74.0f * scale;

        if (settings_.show_fps)
        {
            const double cur_fps = history_.empty() ? repsp::platform::GetDisplayFps() : static_cast<double>(history_.back().fps);
            ImGui::Text("FPS: %.1f", cur_fps);
            std::vector<float> vals;
            vals.reserve(history_.size());
            for (const Sample& s : history_)
            {
                vals.push_back(s.fps);
            }
            const std::vector<Tick> ticks = {
                {60.0f, "60", IM_COL32(140, 220, 140, 180), 1.0f},
                {30.0f, "30", IM_COL32(220, 200, 120, 140), 1.0f},
            };
            DrawGraph(vals, kFpsAxisMax, ticks, IM_COL32(100, 220, 255, 255), label_width);
        }

        if (settings_.show_memory)
        {
            ImGui::Text("Memory: %s", FormatMib(process_memory_bytes_).c_str());
            std::vector<float> vals;
            float peak = 0.0f;
            for (const Sample& s : history_)
            {
                vals.push_back(s.memory_mib);
                peak = std::max(peak, s.memory_mib);
            }
            const float axis_max = MemoryAxisMax(peak);
            char top_lbl[24];
            std::snprintf(top_lbl, sizeof(top_lbl), "%.0fM", axis_max);
            const std::vector<Tick> ticks = {{axis_max * 0.5f, "", IM_COL32(180, 180, 180, 90), 1.0f},
                                             {axis_max, top_lbl, IM_COL32(200, 200, 200, 160), 1.0f}};
            DrawGraph(vals, axis_max, ticks, IM_COL32(120, 255, 160, 255), label_width);
        }

        if (settings_.show_gpu_memory)
        {
            const uint64_t gpu_bytes = EffectiveGpuBytes(gpu_memory_);
            ImGui::Text("GPU Mem: %s%s", FormatMib(gpu_bytes).c_str(),
                        gpu_memory_.driver_reported ? "" : " (HLE)");
            std::vector<float> vals;
            float peak = 0.0f;
            for (const Sample& s : history_)
            {
                vals.push_back(s.gpu_mib);
                peak = std::max(peak, s.gpu_mib);
            }
            const float axis_max = MemoryAxisMax(peak);
            char top_lbl[24];
            std::snprintf(top_lbl, sizeof(top_lbl), "%.0fM", axis_max);
            const std::vector<Tick> ticks = {{axis_max * 0.5f, "", IM_COL32(180, 180, 180, 90), 1.0f},
                                             {axis_max, top_lbl, IM_COL32(200, 200, 200, 160), 1.0f}};
            DrawGraph(vals, axis_max, ticks, IM_COL32(255, 190, 100, 255), label_width);
        }
    }
    ImGui::End();
}

void SettingsUi::DrawAutoplayBadge()
{
    if (autoplay_ == nullptr || !autoplay_->IsEnabled() || !autoplay_->IsInGameplay())
    {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = ImGui::GetStyle().FontScaleMain;
    const float pad = 8.0f * scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - pad,
                                   viewport->WorkPos.y + pad),
                            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##autoplay_badge", nullptr, kFlags))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.35f, 1.0f), "AUTOPLAY");
    }
    ImGui::End();
}

} // namespace redmp1
