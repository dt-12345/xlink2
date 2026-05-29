#pragma once

#include <concepts>

namespace common {

template <typename T>
requires std::integral<T>
constexpr auto AlignUp(T value, T align) -> T {
    return (value + align - 1) / align * align;
}

} // namespace common