#pragma once

#include <cstdint>
#include <string_view>
#include <span>
#include <vector>

namespace common {

[[nodiscard]] auto ReadFile(std::string_view path) -> std::vector<std::uint8_t>;
auto WriteFile(std::string_view path, const std::span<const std::uint8_t> data) -> void;

} // namespace common