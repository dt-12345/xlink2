#pragma once

#include "db/version.hpp"

namespace xlink2 {

template <mango::Game GAME>
struct ResRandom {
    float min;
    float max;
};
static_assert(sizeof(ResRandom<mango::Game::EXKing>) == 8);

template <mango::Game GAME>
struct ResCurvePoint {
    float x;
    float y;
};
static_assert(sizeof(ResCurvePoint<mango::Game::EXKing>) == 8);

enum class CurveType : std::uint16_t {
    Standard,
    Constant,
};

enum class CurveUpdateType : std::int16_t {
    Update      = 0,
    NoUpdate    = 1,
};

template <mango::Game GAME>
struct ResCurve {
    std::uint16_t pointStartIndex;
    std::uint16_t numPoints;
    CurveType type;
    std::uint16_t isGlobalProp;
    mango::PtrT<GAME> propNameOffset;
    std::int32_t unknown;
    std::int16_t propIndex;
    CurveUpdateType updateType;
};
static_assert(sizeof(ResCurve<mango::Game::Park>) == 0x14);
static_assert(sizeof(ResCurve<mango::Game::EXKing>) == 0x18);

enum class ReferenceType {
    Direct                              = 0x0,
    String                              = 0x1,
    Curve                               = 0x2,
    RandomLinear                        = 0x3,
    ArrangeParam                        = 0x4,
    Bitfield                            = 0x5,
    RandomInflectedPolynomial2          = 0x6,
    RandomInflectedPolynomial3          = 0x7,
    RandomInflectedPolynomial4          = 0x8,
    RandomInflectedPolynomial1Point5    = 0x9,
    RandomIncreasingPolynomial2         = 0xa,
    RandomIncreasingPolynomial3         = 0xb,
    RandomIncreasingPolynomial4         = 0xc,
    RandomIncreasingPolynomial1Point5   = 0xd,
    RandomDecreasingPolynomial2         = 0xe,
    RandomDecreasingPolynomial3         = 0xf,
    RandomDecreasingPolynomial4         = 0x10,
    RandomDecreasingPolynomial1Point5   = 0x11,
};

enum class LimitType : std::uint8_t {
    None                        = 0,
    PriorityThenOldest          = 1,
    PriorityThenNewest          = 2,
    OldestThenPriority          = 3,
    NewestThenPriority          = 4,
    SpatialPriorityThenOldest   = 5,
    SpatialPriorityThenNewest   = 6,
    OldestThenSpatialPriority   = 7,
    NewestThenSpatialPriority   = 8,
};

template <mango::Game GAME>
struct ResArrangeGroup {
    mango::PtrT<GAME> nameOffset;
    LimitType limitType;
    std::int8_t threshold;
    bool includeFading;
};
static_assert(sizeof(ResArrangeGroup<mango::Game::Park>) == 8);
static_assert(sizeof(ResArrangeGroup<mango::Game::EXKing>) == 0x10);

template <mango::Game GAME>
struct ResParam {
    std::uint32_t packed;
};
static_assert(sizeof(ResParam<mango::Game::EXKing>) == 4);

} // namespace xlink2