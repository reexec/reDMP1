#include "redmp1/dmp1_profile.h"

#include <repsp/module/param_sfo.h>
#include <repsp/runtime/sha1.h>

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace redmp1
{

namespace
{

std::string ToHex(const uint8_t* data, size_t length)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i)
    {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string Trim(const std::string& str)
{
    size_t first = 0;
    while (first < str.size() && (std::isspace(static_cast<unsigned char>(str[first])) != 0))
    {
        ++first;
    }
    if (first == str.size())
    {
        return "";
    }
    size_t last = str.size() - 1;
    while (last > first && (std::isspace(static_cast<unsigned char>(str[last])) != 0))
    {
        --last;
    }
    return str.substr(first, last - first + 1);
}

// Simple JSON string unescaper
std::string ParseJsonString(const std::string& input, size_t& pos)
{
    if (pos >= input.size() || input[pos] != '"')
    {
        return "";
    }
    ++pos; // skip opening quote
    std::string result;
    while (pos < input.size())
    {
        char c = input[pos++];
        if (c == '"')
        {
            return result;
        }
        if (c == '\\' && pos < input.size())
        {
            char esc = input[pos++];
            if (esc == '"' || esc == '\\' || esc == '/')
            {
                result.push_back(esc);
            }
            else if (esc == 'n')
            {
                result.push_back('\n');
            }
            else if (esc == 'r')
            {
                result.push_back('\r');
            }
            else if (esc == 't')
            {
                result.push_back('\t');
            }
            else
            {
                result.push_back(esc);
            }
        }
        else
        {
            result.push_back(c);
        }
    }
    return result;
}

void SkipWhitespace(const std::string& input, size_t& pos)
{
    while (pos < input.size() &&
           (std::isspace(static_cast<unsigned char>(input[pos])) != 0 || input[pos] == ',' || input[pos] == ':'))
    {
        ++pos;
    }
}

} // namespace

bool LoadVariantsFromString(const std::string& json_str, std::vector<Variant>& out_variants)
{
    out_variants.clear();
    size_t pos = 0;

    // Search for array opening '['
    while (pos < json_str.size() && json_str[pos] != '[')
    {
        ++pos;
    }
    if (pos >= json_str.size())
    {
        return false;
    }
    ++pos; // skip '['

    while (pos < json_str.size())
    {
        SkipWhitespace(json_str, pos);
        if (pos >= json_str.size() || json_str[pos] == ']')
        {
            break;
        }

        if (json_str[pos] == '{')
        {
            ++pos; // skip '{'
            Variant v;
            while (pos < json_str.size())
            {
                SkipWhitespace(json_str, pos);
                if (pos >= json_str.size() || json_str[pos] == '}')
                {
                    if (pos < json_str.size())
                    {
                        ++pos;
                    }
                    break;
                }

                if (json_str[pos] == '"')
                {
                    std::string key = ParseJsonString(json_str, pos);
                    SkipWhitespace(json_str, pos);

                    if (pos < json_str.size() && json_str[pos] == '"')
                    {
                        std::string val = ParseJsonString(json_str, pos);
                        if (key == "disc_id")
                        {
                            v.disc_id = val;
                        }
                        else if (key == "disc_version")
                        {
                            v.disc_version = val;
                        }
                        else if (key == "title")
                        {
                            v.title = val;
                        }
                        else if (key == "executable")
                        {
                            v.executable = val;
                        }
                        else if (key == "sha1")
                        {
                            v.sha1 = val;
                        }
                    }
                    else if (pos < json_str.size() && (std::isdigit(static_cast<unsigned char>(json_str[pos])) != 0))
                    {
                        size_t num_start = pos;
                        while (pos < json_str.size() && (std::isdigit(static_cast<unsigned char>(json_str[pos])) != 0))
                        {
                            ++pos;
                        }
                        size_t num = std::stoull(json_str.substr(num_start, pos - num_start));
                        if (key == "size")
                        {
                            v.size = num;
                        }
                    }
                }
                else
                {
                    ++pos;
                }
            }
            if (!v.disc_id.empty() && !v.sha1.empty())
            {
                out_variants.push_back(v);
            }
        }
        else
        {
            ++pos;
        }
    }

    return true;
}

bool LoadVariants(const std::filesystem::path& json_path, std::vector<Variant>& out_variants)
{
    if (!std::filesystem::exists(json_path))
    {
        return false;
    }
    std::ifstream file(json_path, std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return LoadVariantsFromString(content, out_variants);
}

const Variant* FindVariant(const std::vector<Variant>& variants,
                           const std::string& disc_id,
                           const std::string& disc_version,
                           const std::string& sha1)
{
    // Disc ID in PARAM.SFO may omit hyphen (e.g. ULKS46050 vs ULKS-46050)
    std::string norm_id;
    for (char c : disc_id)
    {
        if (c != '-')
        {
            norm_id.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }

    for (const auto& v : variants)
    {
        std::string v_norm_id;
        for (char c : v.disc_id)
        {
            if (c != '-')
            {
                v_norm_id.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        }

        if (norm_id == v_norm_id &&
            disc_version == v.disc_version &&
            sha1 == v.sha1)
        {
            return &v;
        }
    }
    return nullptr;
}

IdentifyResult IdentifyGameDirectory(const std::filesystem::path& game_dir,
                                     const std::vector<Variant>& variants)
{
    IdentifyResult res;

    if (!std::filesystem::exists(game_dir) || !std::filesystem::is_directory(game_dir))
    {
        res.status = IdentifyStatus::MissingDirectory;
        res.message = "Game directory does not exist or is not a directory: " + game_dir.string();
        return res;
    }

    std::filesystem::path sfo_path = game_dir / "PSP_GAME" / "PARAM.SFO";
    if (!std::filesystem::exists(sfo_path))
    {
        res.status = IdentifyStatus::MissingParamSfo;
        res.message = "PARAM.SFO not found at: " + sfo_path.string();
        return res;
    }

    std::ifstream sfo_file(sfo_path, std::ios::binary);
    if (!sfo_file.is_open())
    {
        res.status = IdentifyStatus::InvalidParamSfo;
        res.message = "Failed to open PARAM.SFO at: " + sfo_path.string();
        return res;
    }
    std::vector<uint8_t> sfo_bytes((std::istreambuf_iterator<char>(sfo_file)),
                                   std::istreambuf_iterator<char>());
    auto sfo_opt = repsp::module::ParamSfo::Parse(sfo_bytes.data(), sfo_bytes.size());
    if (!sfo_opt.has_value())
    {
        res.status = IdentifyStatus::InvalidParamSfo;
        res.message = "Failed to parse PARAM.SFO (invalid SFO format)";
        return res;
    }

    res.disc_id = sfo_opt->GetString("DISC_ID").value_or("");
    res.disc_version = sfo_opt->GetString("DISC_VERSION").value_or("");
    res.title = sfo_opt->GetString("TITLE").value_or("");

    // Identify executable candidate: prefer BOOT.BIN, fallback to EBOOT.BIN
    std::filesystem::path boot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "BOOT.BIN";
    std::filesystem::path eboot_bin = game_dir / "PSP_GAME" / "SYSDIR" / "EBOOT.BIN";
    std::filesystem::path chosen_exe;

    if (std::filesystem::exists(boot_bin))
    {
        chosen_exe = boot_bin;
        res.executable_name = "BOOT.BIN";
    }
    else if (std::filesystem::exists(eboot_bin))
    {
        chosen_exe = eboot_bin;
        res.executable_name = "EBOOT.BIN";
    }
    else
    {
        res.status = IdentifyStatus::MissingExecutable;
        res.message = "Neither BOOT.BIN nor EBOOT.BIN found in PSP_GAME/SYSDIR";
        return res;
    }

    std::ifstream exe_file(chosen_exe, std::ios::binary);
    if (!exe_file.is_open())
    {
        res.status = IdentifyStatus::MissingExecutable;
        res.message = "Failed to open executable: " + chosen_exe.string();
        return res;
    }
    std::vector<uint8_t> exe_bytes((std::istreambuf_iterator<char>(exe_file)),
                                   std::istreambuf_iterator<char>());
    res.executable_size = exe_bytes.size();

    uint8_t digest[20];
    repsp::runtime::ComputeSha1(exe_bytes.data(), exe_bytes.size(), digest);
    res.executable_sha1 = ToHex(digest, sizeof(digest));

    const Variant* matched = FindVariant(variants, res.disc_id, res.disc_version, res.executable_sha1);
    if (matched != nullptr)
    {
        res.status = IdentifyStatus::Success;
        res.matched_variant = matched;
        res.message = "Successfully identified: " + res.title + " (" + res.disc_id + " v" + res.disc_version + ")";
    }
    else
    {
        res.status = IdentifyStatus::UnregisteredVariant;
        res.message = "Unregistered variant: DISC_ID=" + res.disc_id +
                      ", DISC_VERSION=" + res.disc_version +
                      ", SHA1=" + res.executable_sha1;
    }

    return res;
}

} // namespace redmp1
