#include "db/username.hpp"
#include "common/error.hpp"
#include "common/hash.hpp"

#include <fstream>

namespace mango {

UsernameDatabase::UsernameDatabase(std::string_view path) : mUsernames() {
    loadFromFile(path);
}

auto UsernameDatabase::loadFromFile(std::string_view path) -> void {
    auto file = std::ifstream{ std::string(path) };
    if (!file) {
        return;
    }

    for (std::string line; std::getline(file, line);) {
        if (line.empty()) {
            continue;
        }

        const auto hash = common::CalcCRC32(line);
        if (const auto it = mUsernames.find(hash); it != mUsernames.end()) {
            common::AbortWithDetail("Hash collision between \"{}\" and \"{}\"", it->second, line);
        }
        mUsernames.emplace(common::CalcCRC32(line), line);
    }
}

auto UsernameDatabase::add(std::string_view name) -> void {
    const auto hash = common::CalcCRC32(name);
    if (const auto it = mUsernames.find(hash); it != mUsernames.end()) {
        common::AbortWithDetail("Hash collision between \"{}\" and \"{}\"", it->second, name);
    }
    mUsernames.emplace(common::CalcCRC32(name), name);
}

auto UsernameDatabase::query(std::uint32_t hash) const -> std::optional<std::string_view> {
    const auto res = mUsernames.find(hash);
    return res != mUsernames.end() ? std::make_optional<std::string_view>(res->second) : std::nullopt;
}

} // namespace mango