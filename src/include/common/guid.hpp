#pragma once

#include <cstdint>

namespace common {

[[nodiscard]] auto GenerateGUID() -> std::uint32_t;

} // namespace common