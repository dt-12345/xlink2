#include "common/error.hpp"
#include "common/file.hpp"

#include <filesystem>
#include <fstream>

namespace common {

auto ReadFile(std::string_view path) -> std::vector<std::uint8_t> {
    const auto fpath = std::filesystem::path(path);
    auto file = std::ifstream{ fpath, std::ios::binary };
    if (!file) {
        common::AbortWithDetail("{} does not exist!", path);
    }

    const auto size = std::filesystem::file_size(fpath);
    auto data = std::vector<std::uint8_t>(size);
    file.read(reinterpret_cast<char*>(data.data()), data.size());

    return data;
}

auto WriteFile(std::string_view path, const std::span<const std::uint8_t> data) -> void {
    const auto fpath = std::filesystem::path(path);
    if (fpath.has_parent_path()) {
        std::filesystem::create_directories(fpath.parent_path());
    }
    auto file = std::ofstream{ fpath, std::ios::binary };
    if (!file) {
        common::AbortWithDetail("Failed to open {}!", path);
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace common