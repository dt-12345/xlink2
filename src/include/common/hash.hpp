#pragma once

#include <cstdint>
#include <string_view>

namespace common {

[[nodiscard]] auto CalcCRC32(const std::uint8_t* data, size_t size, std::uint32_t seed = 0xffff'ffffu) -> std::uint32_t;
[[nodiscard]] auto CalcCRC32(const char* str) -> std::uint32_t;
[[nodiscard]] auto CalcCRC32(const std::string_view str) -> std::uint32_t;

struct CRC32Context {
    std::uint32_t value = 0xffff'ffffu;

    auto update(const std::uint8_t* data, size_t size) -> void;
    [[nodiscard]] constexpr auto get() const noexcept -> std::uint32_t { return ~value; }

    [[nodiscard]] constexpr operator std::uint32_t() const noexcept { return get(); }
};

} // namespace common