#include "settings.h"

#include "repsp/platform/path.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace redmp1
{

namespace
{

constexpr const char* kFileName = "redmp1.ini";

std::string Trim(const std::string& text)
{
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); });
    return first < last.base() ? std::string(first, last.base()) : std::string();
}

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool ParseBool(const std::string& value, bool* out)
{
    const std::string v = Lower(value);
    if (v == "true" || v == "1" || v == "yes" || v == "on")
    {
        *out = true;
        return true;
    }
    if (v == "false" || v == "0" || v == "no" || v == "off")
    {
        *out = false;
        return true;
    }
    return false;
}

bool ParseIntRange(const std::string& value, int min_v, int max_v, int* out)
{
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (value.empty() || end == value.c_str() || *end != '\0')
    {
        return false;
    }
    if (parsed < min_v || parsed > max_v)
    {
        return false;
    }
    *out = static_cast<int>(parsed);
    return true;
}

bool Exists(const std::filesystem::path& path)
{
    std::error_code ec;
    return !path.empty() && std::filesystem::exists(path, ec);
}

bool WriteAtomic(const std::filesystem::path& target, const std::string& text)
{
    if (target.empty())
    {
        return false;
    }
    std::error_code ec;
    if (target.has_parent_path())
    {
        std::filesystem::create_directories(target.parent_path(), ec);
    }
    std::filesystem::path temp = target;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }
        out << text;
        if (!out.flush())
        {
            out.close();
            std::filesystem::remove(temp, ec);
            return false;
        }
    }
    std::filesystem::rename(temp, target, ec);
    if (ec)
    {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

const char* VsyncToString(repsp::platform::VsyncMode m)
{
    switch (m)
    {
    case repsp::platform::VsyncMode::kOn:
        return "on";
    case repsp::platform::VsyncMode::kAdaptive:
        return "adaptive";
    default:
        return "off";
    }
}

const char* AspectToString(repsp::platform::AspectMode m)
{
    switch (m)
    {
    case repsp::platform::AspectMode::kStretch:
        return "stretch";
    case repsp::platform::AspectMode::kInteger:
        return "integer";
    default:
        return "original";
    }
}

const char* OutputFilterToString(repsp::platform::OutputFilter f)
{
    return f == repsp::platform::OutputFilter::kLinear ? "linear" : "nearest";
}

const char* TexFilterToString(repsp::gpu::TextureFilterMode f)
{
    switch (f)
    {
    case repsp::gpu::TextureFilterMode::kLinear:
        return "linear";
    case repsp::gpu::TextureFilterMode::kAniso2x:
        return "aniso2x";
    case repsp::gpu::TextureFilterMode::kAniso4x:
        return "aniso4x";
    case repsp::gpu::TextureFilterMode::kAniso8x:
        return "aniso8x";
    case repsp::gpu::TextureFilterMode::kAniso16x:
        return "aniso16x";
    default:
        return "original";
    }
}

} // namespace

SettingsFile::SettingsFile()
    : SettingsFile(
          []()
          {
              const auto exe_dir = repsp::platform::GetExecutableDirectory();
              return exe_dir.empty() ? std::filesystem::path(kFileName) : (exe_dir / kFileName);
          }(),
          repsp::platform::GetUserDataDirectory("redmp1") / kFileName)
{
}

SettingsFile::SettingsFile(std::filesystem::path primary_path, std::filesystem::path fallback_path)
    : path_(std::move(primary_path)), fallback_path_(std::move(fallback_path))
{
    if (path_.empty() || (!Exists(path_) && Exists(fallback_path_)))
    {
        path_ = fallback_path_;
    }
    if (path_.empty())
    {
        path_ = kFileName;
    }
}

Settings SettingsFile::Load()
{
    Settings s;
    std::ifstream in(path_, std::ios::binary);
    if (!in)
    {
        return s;
    }
    std::string section;
    std::string line;
    while (std::getline(in, line))
    {
        const size_t comment = line.find_first_of(";#");
        line = Trim(comment == std::string::npos ? line : line.substr(0, comment));
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            section = Lower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos || section != "graphics")
        {
            continue;
        }
        const std::string key = Lower(Trim(line.substr(0, eq)));
        const std::string val = Trim(line.substr(eq + 1));
        const std::string lval = Lower(val);

        if (key == "windowscale")
        {
            ParseIntRange(val, 1, 6, &s.window_scale);
        }
        else if (key == "fullscreen")
        {
            ParseBool(val, &s.fullscreen);
        }
        else if (key == "vsync")
        {
            if (lval == "off" || lval == "0" || lval == "false")
            {
                s.vsync = repsp::platform::VsyncMode::kOff;
            }
            else if (lval == "on" || lval == "1" || lval == "true")
            {
                s.vsync = repsp::platform::VsyncMode::kOn;
            }
            else if (lval == "adaptive")
            {
                s.vsync = repsp::platform::VsyncMode::kAdaptive;
            }
        }
        else if (key == "aspect")
        {
            if (lval == "original")
            {
                s.aspect = repsp::platform::AspectMode::kOriginal;
            }
            else if (lval == "stretch")
            {
                s.aspect = repsp::platform::AspectMode::kStretch;
            }
            else if (lval == "integer")
            {
                s.aspect = repsp::platform::AspectMode::kInteger;
            }
        }
        else if (key == "outputfilter")
        {
            if (lval == "nearest")
            {
                s.output_filter = repsp::platform::OutputFilter::kNearest;
            }
            else if (lval == "linear")
            {
                s.output_filter = repsp::platform::OutputFilter::kLinear;
            }
        }
        else if (key == "renderscale")
        {
            ParseIntRange(val, 1, 8, &s.render_scale);
        }
        else if (key == "msaa")
        {
            int count = 0;
            if (ParseIntRange(val, 0, 8, &count) && (count == 0 || count == 2 || count == 4 || count == 8))
            {
                s.msaa = count;
            }
        }
        else if (key == "texturefilter")
        {
            if (lval == "original")
            {
                s.texture_filter = repsp::gpu::TextureFilterMode::kOriginal;
            }
            else if (lval == "linear")
            {
                s.texture_filter = repsp::gpu::TextureFilterMode::kLinear;
            }
            else if (lval == "aniso2x")
            {
                s.texture_filter = repsp::gpu::TextureFilterMode::kAniso2x;
            }
            else if (lval == "aniso4x")
            {
                s.texture_filter = repsp::gpu::TextureFilterMode::kAniso4x;
            }
            else if (lval == "aniso8x")
            {
                s.texture_filter = repsp::gpu::TextureFilterMode::kAniso8x;
            }
            else if (lval == "aniso16x")
            {
                s.texture_filter = repsp::gpu::TextureFilterMode::kAniso16x;
            }
        }
        else if (key == "showfps")
        {
            ParseBool(val, &s.show_fps);
        }
        else if (key == "showmemory")
        {
            ParseBool(val, &s.show_memory);
        }
        else if (key == "showgpumemory")
        {
            ParseBool(val, &s.show_gpu_memory);
        }
        else if (key == "statsx" || key == "statsy")
        {
            char* end = nullptr;
            const float frac = std::strtof(val.c_str(), &end);
            if (!val.empty() && end != val.c_str() && *end == '\0' && frac >= 0.0f && frac <= 1.0f)
            {
                (key == "statsx" ? s.stats_x : s.stats_y) = frac;
            }
        }
    }
    return s;
}

bool SettingsFile::Save(const Settings& settings)
{
    std::ostringstream out;
    out << "[Graphics]\n";
    out << "WindowScale=" << settings.window_scale << "\n";
    out << "Fullscreen=" << (settings.fullscreen ? "true" : "false") << "\n";
    out << "VSync=" << VsyncToString(settings.vsync) << "\n";
    out << "Aspect=" << AspectToString(settings.aspect) << "\n";
    out << "OutputFilter=" << OutputFilterToString(settings.output_filter) << "\n";
    out << "RenderScale=" << settings.render_scale << "\n";
    out << "MSAA=" << settings.msaa << "\n";
    out << "TextureFilter=" << TexFilterToString(settings.texture_filter) << "\n";
    out << "ShowFPS=" << (settings.show_fps ? "true" : "false") << "\n";
    out << "ShowMemory=" << (settings.show_memory ? "true" : "false") << "\n";
    out << "ShowGPUMemory=" << (settings.show_gpu_memory ? "true" : "false") << "\n";
    if (settings.stats_x >= 0.0f && settings.stats_y >= 0.0f)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "StatsX=%.4f\nStatsY=%.4f\n", settings.stats_x, settings.stats_y);
        out << buf;
    }

    const std::string text = out.str();
    if (WriteAtomic(path_, text))
    {
        return true;
    }
    if (!fallback_path_.empty() && fallback_path_ != path_ && WriteAtomic(fallback_path_, text))
    {
        path_ = fallback_path_;
        return true;
    }
    return false;
}

} // namespace redmp1
