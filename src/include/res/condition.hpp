#pragma once

#include "common/sizedenum.hpp"
#include "db/version.hpp"
#include "res/container.hpp"

namespace xlink2 {

enum class PropertyType {
    Enum    = 0,
    S32     = 1,
    F32     = 2,
    Bool    = 3,
    _04     = 4, // S32
    _05     = 5, // F32
};

enum class CompareType {
    Equal               = 0,
    GreaterThan         = 1,
    GreaterThanOrEqual  = 2,
    LessThan            = 3,
    LessThanOrEqual     = 4,
    NotEqual            = 5,
};

enum class BlendType : std::uint8_t {
    None        = 0,
    Multiply    = 1,
    SquareRoot  = 2,
    Sin         = 3,
    Add         = 4,
    SetToOne    = 5,
};

template <mango::Game GAME>
struct ResCondition {
    common::SizedEnum<ContainerType, std::uint32_t> parentContainerType;
};
static_assert(sizeof(ResCondition<mango::Game::EXKing>) == 4);

template <mango::Game GAME>
struct ResSwitchCondition;

template <mango::Game GAME> requires (mango::GetResSwitchConditionLayout(GAME) == 0)
struct ResSwitchCondition<GAME> : public ResCondition<GAME> {
    common::SizedEnum<PropertyType, std::uint32_t> propType;
    common::SizedEnum<CompareType, std::uint32_t> compareType;
    union {
        mango::PtrT<GAME> globalPropertyEnumNameOffset;
        std::int32_t valueS32;
        float valueF32;
        bool valueBool;
    };
    std::int16_t localPropertyEnumNameIndex;
    bool isSolved;
    bool isGlobal;
};

template <mango::Game GAME> requires (mango::GetResSwitchConditionLayout(GAME) == 1)
struct ResSwitchCondition<GAME> : public ResCondition<GAME> {
    common::SizedEnum<PropertyType, std::uint8_t> propType;
    common::SizedEnum<CompareType, std::uint8_t> compareType;
    bool isSolved;
    bool isGlobal;
    union {
        mango::PtrT<GAME> globalPropertyEnumNameOffset;
        std::int32_t valueS32;
        float valueF32;
        bool valueBool;
    };
    union {
        std::uint32_t actionHash;
        std::int32_t localPropertyEnumNameIndex;
    };
};

template <mango::Game GAME> requires (mango::GetResSwitchConditionLayout(GAME) == 2)
struct ResSwitchCondition<GAME> : public ResCondition<GAME> {
    common::SizedEnum<PropertyType, std::uint8_t> propType;
    common::SizedEnum<CompareType, std::uint8_t> compareType;
    bool isSolved;
    bool isGlobal;
    union {
        std::uint32_t actionHash;
        std::int32_t localPropertyEnumNameIndex;
    };
    union {
        std::int32_t valueS32;
        float valueF32;
        bool valueBool;
    };
    // only present for enums
    // union {
    //     mango::PtrT<GAME> actionNameOffset;
    //     mango::PtrT<GAME> globalPropertyEnumNameOffset;
    //     mango::PtrT<GAME> localPropertyEnumNameOffset;
    // };
};

static_assert(sizeof(ResSwitchCondition<mango::Game::Blitz>) == 0x14);
static_assert(sizeof(ResSwitchCondition<mango::Game::Park>) == 0x10);
static_assert(sizeof(ResSwitchCondition<mango::Game::EXKing>) == 0x10);

template <mango::Game GAME>
struct ResRandomCondition : public ResCondition<GAME> {
    float weight;
};
static_assert(sizeof(ResRandomCondition<mango::Game::EXKing>) == 8);

template <mango::Game GAME>
using ResRandom2Condition = ResRandomCondition<GAME>;
static_assert(sizeof(ResRandom2Condition<mango::Game::EXKing>) == 8);

template <mango::Game GAME>
struct ResBlendByCondition : public ResCondition<GAME> {
    float min;
    float max;
    BlendType minOp;
    BlendType maxOp;
};
static_assert(sizeof(ResBlendByCondition<mango::Game::EXKing>) == 0x10);

template <mango::Game GAME>
struct ResSequenceCondition : public ResCondition<GAME> {
    std::uint32_t forceContinue;
};
static_assert(sizeof(ResSequenceCondition<mango::Game::EXKing>) == 8);

} // namespace xlink2