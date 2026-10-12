#include "EntropySurroundCore.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) return 2;
    for (int argument = 1; argument < argc; ++argument)
    {
        const std::filesystem::path path(argv[argument]);
        std::ifstream input(path, std::ios::binary);
        std::vector<std::uint8_t> bytes(
            std::istreambuf_iterator<char>(input), {});
        std::map<entropy::surround::Codec, std::uint64_t> counts;
        for (std::size_t offset = 0; offset + 8 <= bytes.size(); ++offset)
        {
            const auto status = entropy::surround::inspectIec61937(
                bytes.data() + offset, 8);
            if (!status.found) continue;
            ++counts[status.codec];
            offset += 7;
        }
        std::wcout << path.filename().wstring() << L": ";
        for (const auto& [codec, count] : counts)
            std::cout << entropy::surround::codecName(codec)
                      << "=" << count << " ";
        std::cout << "\n";
    }
}
