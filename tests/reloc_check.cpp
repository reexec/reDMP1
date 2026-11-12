#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <repsp/module/prx.h>

static std::vector<uint8_t> ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        return {};
    }
    return buffer;
}

static uint32_t Read32Le(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: reloc_check <input_prx> <allegrex_reloc_bin> [base_hex]\n";
        return 1;
    }

    std::string prxPath = argv[1];
    std::string allegrexPath = argv[2];
    uint32_t base = 0x08804000u;
    if (argc > 3)
    {
        base = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 0));
    }

    std::vector<uint8_t> prxData = ReadFile(prxPath);
    if (prxData.empty())
    {
        std::cerr << "reloc_check error: cannot read PRX file " << prxPath << "\n";
        return 1;
    }

    std::string err;
    auto repspReloc = repsp::module::RelocateToBuffer(prxData, base, &err);
    if (!repspReloc.has_value())
    {
        std::cerr << "reloc_check error: repsp::module::RelocateToBuffer failed: " << err << "\n";
        return 1;
    }

    std::vector<uint8_t> allegrexReloc = ReadFile(allegrexPath);
    if (allegrexReloc.empty())
    {
        std::cerr << "reloc_check error: cannot read allegrexrecomp output " << allegrexPath << "\n";
        return 1;
    }

    if (repspReloc->size() != allegrexReloc.size())
    {
        std::cerr << "reloc_check MISMATCH: sizes differ! repsp=" << repspReloc->size()
                  << " bytes, allegrex=" << allegrexReloc.size() << " bytes\n";
        return 1;
    }

    size_t mismatchCount = 0;
    size_t firstMismatch = static_cast<size_t>(-1);

    for (size_t i = 0; i < repspReloc->size(); ++i)
    {
        if ((*repspReloc)[i] != allegrexReloc[i])
        {
            if (firstMismatch == static_cast<size_t>(-1))
            {
                firstMismatch = i;
            }
            mismatchCount++;
        }
    }

    if (mismatchCount > 0)
    {
        size_t wordOffset = firstMismatch & ~size_t{3};
        uint32_t wRepsp = Read32Le(repspReloc->data() + wordOffset);
        uint32_t wAllegrex = Read32Le(allegrexReloc.data() + wordOffset);

        std::cerr << "reloc_check MISMATCH: " << mismatchCount << " bytes differ.\n";
        std::cerr << "First mismatch at offset 0x" << std::hex << firstMismatch
                  << " (address 0x" << (base + firstMismatch) << "):\n"
                  << "  repsp byte:          0x" << static_cast<int>((*repspReloc)[firstMismatch]) << "\n"
                  << "  allegrexrecomp byte: 0x" << static_cast<int>(allegrexReloc[firstMismatch]) << "\n"
                  << "At aligned word offset 0x" << wordOffset
                  << " (address 0x" << (base + wordOffset) << "):\n"
                  << "  repsp word:          0x" << wRepsp << "\n"
                  << "  allegrexrecomp word: 0x" << wAllegrex << "\n"
                  << std::dec;
        return 1;
    }

    std::cout << "reloc_check: PASSED (all " << repspReloc->size()
              << " bytes match byte-for-byte at base 0x" << std::hex << base << std::dec << ")\n";
    return 0;
}
