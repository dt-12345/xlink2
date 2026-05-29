#pragma once

#include <type_traits>

namespace common {

template <typename T, typename StorageT = std::underlying_type_t<T>> requires std::is_enum_v<T>
class SizedEnum {
public:
    constexpr SizedEnum() noexcept = default;
    constexpr SizedEnum(T value) noexcept : mValue(static_cast<StorageT>(value)) {}
    constexpr explicit SizedEnum(StorageT value) noexcept : mValue(value) {}

    constexpr operator T() const noexcept { return static_cast<T>(mValue); }

private:
    StorageT mValue;
};

} // namespace common