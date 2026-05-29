#pragma once

#include <fmt/base.h>

#include <string>

namespace common {

namespace detail {
[[noreturn]] auto AbortWithDetailImpl(const char* msg) noexcept -> void;
} // namespace detail

template <typename... Ts>
[[noreturn]] auto AbortWithDetail(const fmt::format_string<Ts...>& fmt, Ts&&... args) noexcept -> void {
    auto out = std::string{};
    fmt::format_to(std::back_inserter(out), fmt, std::forward<Ts>(args)...);
    detail::AbortWithDetailImpl(out.c_str());
}

[[noreturn]] inline auto AbortWithDetail(const char* msg) noexcept -> void {
    detail::AbortWithDetailImpl(msg);
}

} // namespace common