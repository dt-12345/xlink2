#include "common/error.hpp"

#include <cstdio>

#ifdef XLINK_DEBUG
#include <stacktrace>
#endif

namespace common::detail {

auto AbortWithDetailImpl(const char* msg) noexcept -> void {
    std::fputs( msg, stderr);
#ifdef XLINK_DEBUG
    std::fputs("\n", stderr);
    std::fputs(std::to_string(std::stacktrace::current(1)).c_str(), stderr);
#endif
    std::abort();
}

} // namespace common