#pragma once

#include "common/error.hpp"

#include <meta>
#include <type_traits>
#include <utility>

namespace common {

template <typename T> requires std::is_enum_v<T>
constexpr auto ToString(T value) -> std::string_view {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
        if (value == [:e:]) {
            return std::meta::identifier_of(e);
        }
    }

    common::AbortWithDetail("Unknown value for {}: {}", std::meta::identifier_of(^^T), std::to_underlying(value));
}

template <typename T> requires std::is_enum_v<T>
constexpr auto FromString(std::string_view value) -> T {
    template for (constexpr auto e :std::define_static_array(std::meta::enumerators_of(^^T))) {
        if (value == std::meta::identifier_of(e)) {
            return [:e:];
        }
    }

    common::AbortWithDetail("Unknown value for {}: {}", std::meta::identifier_of(^^T), value);
}

} // namespace common