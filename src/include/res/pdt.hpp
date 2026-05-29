#pragma once

#include "db/version.hpp"

namespace xlink2 {

enum class ParamType : std::uint32_t {
    Int     = 0,
    Float   = 1,
    Bool    = 2,
    Enum    = 3,
    String  = 4,
    Custom  = 5,
};

template <mango::Game GAME>
struct ResParamDefine {
    mango::PtrT<GAME> nameOffset;
    ParamType type;
    mango::PtrT<GAME> value;
};
static_assert(sizeof(ResParamDefine<mango::Game::Park>) == 0xc);
static_assert(sizeof(ResParamDefine<mango::Game::EXKing>) == 0x18);

template <mango::Game GAME>
struct alignas(mango::PtrT<GAME>) ParamDefineTableHeader {
    std::uint32_t totalSize;
    std::int32_t userParamCount;
    std::int32_t assetParamCount;
    std::int32_t customAssetParamCount;
    std::int32_t triggerParamCount;
};
static_assert(sizeof(ParamDefineTableHeader<mango::Game::Park>) == 0x14);
static_assert(sizeof(ParamDefineTableHeader<mango::Game::EXKing>) == 0x18);

}