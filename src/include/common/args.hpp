#pragma once

#include <cstdint>
#include <string>

namespace common {

class CommandLineArgs {
public:
    CommandLineArgs() = delete;
    CommandLineArgs(std::int32_t argc, const char** argv);

    auto pop(const char* purpose = "") -> std::string;
    auto finished() const -> bool { return mCurrent >= mArgCount; }

    static constexpr const auto cMaxArgSize = std::size_t(0x1000);

private:
    const char** mArgs;
    std::int32_t mArgCount;
    std::int32_t mCurrent;
};

} // namespace common