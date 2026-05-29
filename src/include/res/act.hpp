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
    _08             = 8,
    _09             = 9, // TODO: this is checked in a replaceAssetInfo callback but there doesn't seem to be any call tables that use it
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