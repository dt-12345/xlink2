#pragma once

#include "db/version.hpp"

namespace xlink2 {

template <mango::Game GAME>
struct ResourceHeader {
    char magic[4];
    std::uint32_t dataSize;
    std::uint32_t version;
    std::uint32_t numResParam;
    std::uint32_t numResAssetParam;
    std::uint32_t numResTriggerOverwriteParam;
    mango::PtrT<GAME> triggerOverwriteParamTablePos;
    mango::PtrT<GAME> localPropertyNameRefTablePos;
    std::uint32_t numLocalPropertyNameRefTable;
    std::uint32_t numLocalPropertyEnumNameRefTable;
    std::uint32_t numDirectValueTable;
    std::uint32_t numRandomTable;
    std::uint32_t numCurveTable;
    std::uint32_t numCurvePointTable;
    mango::PtrT<GAME> exRegionPos;
    std::uint32_t numUser;
    mango::PtrT<GAME> conditionTablePos;
    mango::PtrT<GAME> nameTablePos;
};
static_assert(sizeof(ResourceHeader<mango::Game::Park>) == 0x48);
static_assert(sizeof(ResourceHeader<mango::Game::EXKing>) == 0x60);
    
} // namespace xlink2