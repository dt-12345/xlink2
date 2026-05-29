#pragma once

#include "db/version.hpp"

namespace xlink2 {

template <mango::Game GAME>
struct ResProperty {
    mango::PtrT<GAME> nameOffset;
    std::uint32_t isGlobal;
    std::int32_t triggerStartIndex;
    std::int32_t triggerEndIndex;
};
static_assert(sizeof(ResProperty<mango::Game::Park>) == 0x10);
static_assert(sizeof(ResProperty<mango::Game::EXKing>) == 0x18);

template <mango::Game GAME>
struct ResPropertyTrigger;

template <mango::Game GAME> requires (mango::GetResPropertyTriggerLayout(GAME) == 0)
struct ResPropertyTrigger<GAME> {
    std::uint32_t guid;
    mango::PtrT<GAME> assetCallTableOffset;
    mango::PtrT<GAME> conditionOffset;
    std::uint16_t flags;
    std::uint16_t unknown;
    mango::PtrT<GAME> overwriteParam;
};

// this re-ordering began with the shift to 64-bit so it's likely just bc it packs better
template <mango::Game GAME> requires (mango::GetResPropertyTriggerLayout(GAME) == 1)
struct ResPropertyTrigger<GAME> {
    std::uint32_t guid;
    std::uint16_t flags;
    std::uint16_t unknown;
    mango::PtrT<GAME> assetCallTableOffset;
    mango::PtrT<GAME> conditionOffset;
    mango::PtrT<GAME> overwriteParam;
};
static_assert(sizeof(ResPropertyTrigger<mango::Game::Park>) == 0x14);
static_assert(sizeof(ResPropertyTrigger<mango::Game::EXKing>) == 0x20);

} // namespace xlink2