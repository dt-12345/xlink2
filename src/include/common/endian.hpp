#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace common {

inline constexpr auto IsBigEndian() -> bool {
    return std::endian::native == std::endian::big;
}

inline constexpr auto IsLittleEndian() -> bool {
    return std::endian::native == std::endian::little;
}

inline constexpr auto IsNative(std::uint16_t bom) -> bool {
    return bom == 0xfeff;
}

template <typename T>
requires std::integral<T> || std::floating_point<T>
constexpr auto ByteSwap(T value) -> T;

template <typename T>
requires std::is_enum_v<T>
constexpr auto ByteSwap(T value) -> T {
    return static_cast<T>(std::byteswap(static_cast<std::underlying_type_t<T>>(value)));
}

template <>
constexpr auto ByteSwap(float value) -> float {
    return std::bit_cast<float>(std::byteswap(std::bit_cast<std::uint32_t>(value)));
}

template <>
constexpr auto ByteSwap(double value) -> double {
    return std::bit_cast<double>(std::byteswap(std::bit_cast<std::uint64_t>(value)));
}

template <std::integral T>
constexpr auto ByteSwap(T value) -> T {
    return std::byteswap(value);
}

template <typename T>
constexpr auto ByteSwapIfNeeded(T value, std::endian endian) -> T {
    return endian == std::endian::native ? value : ByteSwap(value);
}

template <typename T>
requires std::integral<T> || std::floating_point<T>
constexpr auto InplaceByteSwap(T& value) -> void;

template <typename T>
requires std::is_enum_v<T>
constexpr auto InplaceByteSwap(T& value) -> void {
    value = static_cast<T>(std::byteswap(static_cast<std::underlying_type_t<T>>(value)));
}

template <>
constexpr auto InplaceByteSwap(float& value) -> void {
    value = std::bit_cast<float>(std::byteswap(std::bit_cast<std::uint32_t>(value)));
}

template <>
constexpr auto InplaceByteSwap(double& value) -> void {
    value = std::bit_cast<double>(std::byteswap(std::bit_cast<std::uint64_t>(value)));
}

template <std::integral T>
constexpr auto InplaceByteSwap(T& value) -> void {
    value = std::byteswap(value);
}

template <typename T>
constexpr auto InplaceByteSwapIfNeeded(T& value, std::endian endian) -> void {
    if (endian != std::endian::native) {
        InplaceByteSwap(value);
    }
}

} // namespace common