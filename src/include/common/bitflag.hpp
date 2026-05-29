#pragma once

#include <bit>
#include <climits>
#include <type_traits>

namespace common {

template <typename T, typename StorageT = std::underlying_type_t<T>>
requires std::is_enum_v<T>
class BitFlag {
    static constexpr const std::size_t cSize = sizeof(StorageT);
    static constexpr const std::size_t cBitSize = cSize * CHAR_BIT;
public:
    constexpr BitFlag() noexcept = default;

    template <typename ...Ts> requires std::is_same_v<Ts..., T>
    constexpr explicit BitFlag(Ts&&... values) noexcept : mStorage(StorageT(0) | MakeMask(values...)) {}

    constexpr explicit BitFlag(StorageT val) noexcept : mStorage(val) {}

    [[nodiscard]] static constexpr auto MakeMask(T val) -> StorageT { return 1 << static_cast<StorageT>(val); }

    [[nodiscard]] constexpr auto isOn(T val) const -> bool { return (mStorage & MakeMask(val)) != 0; }
    [[nodiscard]] constexpr auto isOff(T val) const -> bool { return !isOn(val); }

    constexpr auto set(T val, bool on) -> void {
        if (on) {
            mStorage |= MakeMask(val);
        } else {
            mStorage &= ~MakeMask(val);
        }
    }
    constexpr auto setOn(T val) -> void { set(val, true); }
    constexpr auto setOff(T val) -> void { set(val, false); }

    [[nodiscard]] constexpr auto getNumOn() const -> std::size_t { return std::popcount(mStorage); }
    [[nodiscard]] constexpr auto getNumOff() const -> std::size_t { return cBitSize - getNumOn(); }

    constexpr operator StorageT() noexcept { return mStorage; }

private:
    StorageT mStorage;
};

} // namespace common