#pragma once

#include "common/bitflag.hpp"
#include "db/version.hpp"

namespace xlink2 {

enum class CallTableFlags : std::uint16_t {
    IsContainer     = 0,
    _01             = 1,
    _02             = 2,
    OneShot         = 3,
    NoPause         = 4,
    _05             = 5,
    _06             = 6,
    _07             = 7,
    UserFlag0       = 8,
    UserFlag1       = 9,    // in totk, this is used for conversions for gloom version of things
    UserFlag2       = 10,
    UserFlag3       = 11,
    UserFlag4       = 12,
    UserFlag5       = 13,
    UserFlag6       = 14,
    UserFlag7       = 15,
};

template <mango::Game GAME>
struct ResAssetCallTable {
    mango::PtrT<GAME> nameOffset;
    std::int16_t assetIndex;
    common::BitFlag<CallTableFlags, std::uint16_t> flags;
    std::int32_t emitCount;
    std::int32_t parentIndex;
    std::uint32_t guid;
    std::uint32_t keyHash;
    mango::PtrT<GAME> assetParamOrContainerOffset;
    mango::PtrT<GAME> conditionOffset;
};
static_assert(sizeof(ResAssetCallTable<mango::Game::Park>) == 0x20);
static_assert(sizeof(ResAssetCallTable<mango::Game::EXKing>) == 0x30);

} // namespace xlink2