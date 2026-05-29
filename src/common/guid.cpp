#include "common/guid.hpp"

#include <random>

namespace common {

auto GenerateGUID() -> std::uint32_t {
    auto rng = std::mt19937(std::random_device{}());
    auto dist = std::uniform_int_distribution<std::uint32_t>(0u, 0xfffffffu);
    return dist(rng);
}

} // namespace common