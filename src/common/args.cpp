#include "common/args.hpp"
#include "common/error.hpp"

namespace common {

CommandLineArgs::CommandLineArgs(std::int32_t argc, const char** argv)
    : mArgs(argv), mArgCount(argc), mCurrent(1) {}

auto CommandLineArgs::pop(const char* purpose) -> std::string {
    if (mCurrent >= mArgCount) {
        common::AbortWithDetail("No more arguments left! {}", purpose);
    }

    if (mCurrent < 0) {
        mCurrent = 0;
    }

    const auto arg = mArgs[mCurrent++];
    const auto len = strnlen(arg, cMaxArgSize);
    return std::string{ arg, len };
}

} // namespace common