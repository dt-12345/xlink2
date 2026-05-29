#pragma once

#include "db/version.hpp"

namespace xlink2 {

template <mango::Game GAME>
struct ResUserHeader;

template <mango::Game GAME> requires (mango::GetResUserHeaderLayout(GAME) == 0)
struct ResUserHeader<GAME> {
    std::uint32_t isSetup;
    std::int32_t numLocalProperty;
    std::uint32_t numCallTable;
    std::uint32_t numAsset;
    std::uint32_t numRandomContainer;
    std::uint32_t numResActionSlot;
    std::uint32_t numResAction;
    std::uint32_t numResActionTrigger;
    std::uint32_t numResProperty;
    std::uint32_t numResPropertyTrigger;
    std::uint32_t numResAlwaysTrigger;
    mango::PtrT<GAME> triggerTablePos;
};

template <mango::Game GAME> requires (mango::GetResUserHeaderLayout(GAME) == 1)
struct ResUserHeader<GAME> {
    std::uint32_t isSetup;
    std::int16_t numLocalProperty;
    std::uint16_t unknown;
    std::uint32_t numCallTable;
    std::uint32_t numAsset;
    std::uint32_t numRandomContainer;
    std::uint32_t numResActionSlot;
    std::uint32_t numResAction;
    std::uint32_t numResActionTrigger;
    std::uint32_t numResProperty;
    std::uint32_t numResPropertyTrigger;
    std::uint32_t numResAlwaysTrigger;
    mango::PtrT<GAME> triggerTablePos;
};

static_assert(sizeof(xlink2::ResUserHeader<mango::Game::Park>) == 0x30);
static_assert(sizeof(xlink2::ResUserHeader<mango::Game::EXKing>) == 0x38);

} // namespace xlink2