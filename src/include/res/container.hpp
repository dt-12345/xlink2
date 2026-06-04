#pragma once

#include "common/sizedenum.hpp"
#include "db/version.hpp"

namespace xlink2 {

enum class ContainerType {
    Switch      = 0,
    Random      = 1,
    Random2     = 2,
    Blend       = 3,
    Sequence    = 4,
    Grid        = 5,
    Jump        = 6,
};

template <mango::Game GAME>
struct ResContainerParam;

template <mango::Game GAME> requires (mango::GetResContainerParamLayout(GAME) == 0)
struct alignas(mango::PtrT<GAME>) ResContainerParam<GAME> {
    common::SizedEnum<ContainerType, std::uint32_t> containerType;
    std::int32_t childStartIndex;
    std::int32_t childEndIndex;
};

template <mango::Game GAME> requires (mango::GetResContainerParamLayout(GAME) == 1)
struct alignas(mango::PtrT<GAME>) ResContainerParam<GAME> {
    common::SizedEnum<ContainerType, std::uint8_t> containerType;
    bool isBlendByProperty;
    bool maxCallStackDepthExceeded; // for jump containers: if call is >=32 deep, then this is set to true and the jump container does nothing
    std::int32_t childStartIndex;
    std::int32_t childEndIndex;
};
static_assert(sizeof(ResContainerParam<mango::Game::Park>) == 0xc);
static_assert(sizeof(ResContainerParam<mango::Game::EXKing>) == 0x10);

// also used by blend-by containers
template <mango::Game GAME>
struct ResSwitchContainerParam : public ResContainerParam<GAME> {
    mango::PtrT<GAME> switchVariableNameOffset;
    std::uint32_t unknown;
    std::int16_t propertyIndex;
    bool isGlobal;
    bool isAction; // padding on unsupported versions
};
static_assert(sizeof(ResSwitchContainerParam<mango::Game::Park>) == 0x18);
static_assert(sizeof(ResSwitchContainerParam<mango::Game::EXKing>) == 0x20);

template <mango::Game GAME>
struct ResGridContainerParam : public ResContainerParam<GAME> {
    mango::PtrT<GAME> property1NameOffset;
    mango::PtrT<GAME> property2NameOffset;
    std::int16_t property1Index;
    std::int16_t property2Index;
    std::uint16_t flags;
    std::uint8_t property1ValueCount;
    std::uint8_t property2ValueCount;
};
static_assert(sizeof(ResGridContainerParam<mango::Game::EXKing>) == 0x28);

} // namespace xlink2