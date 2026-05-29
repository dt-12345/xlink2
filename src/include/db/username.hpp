#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace mango {

class UsernameDatabase {
public:
    UsernameDatabase() = default;
    explicit UsernameDatabase(std::string_view path);

    auto loadFromFile(std::string_view path) -> void;
    auto add(std::string_view name, bool ignoreCollisions = false) -> void;

    auto query(std::uint32_t hash) const -> std::optional<std::string_view>;
    auto getUsernames() const -> const std::unordered_map<std::uint32_t, std::string>& { return mUsernames; }

private:
    std::unordered_map<std::uint32_t, std::string> mUsernames = {};
};

} // namespace mango