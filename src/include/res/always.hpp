#pragma once

#include "db/version.hpp"

namespace xlink2 {

template <mango::Game GAME>
struct ResAlwaysTrigger;

template <mango::Game GAME> requires (mango::GetResAlwaysTriggerLayout(GAME) == 0)
struct ResAlwaysTrigger<GAME> {
    std::uint32_t guid;
    mango::PtrT<GAME> assetCallTableOffset;
    std::uint16_t flags;
    std::uint16_t unknown;
    mango::PtrT<GAME> overwriteParamOffset;
};

template <mango::Game GAME> requires (mango::GetResAlwaysTriggerLayout(GAME) == 1)
struct ResAlwaysTrigger<GAME> {
    std::uint32_t guid;
    std::uint16_t flags;
    std::uint16_t unknown;
    mango::PtrT<GAME> assetCallTableOffset;
    mango::PtrT<GAME> overwriteParamOffset;
};
static_assert(sizeof(ResAlwaysTrigger<mango::Game::Park>) == 0x10);
static_assert(sizeof(ResAlwaysTrigger<mango::Game::EXKing>) == 0x18);

} // namespace xlink2