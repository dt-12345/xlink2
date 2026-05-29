#pragma once

#include <cstdint>

namespace mango {

// keep this in sync with the variant below
enum class ValueType {
    S32 = 0,
    Bitfield,
    F32,
    Curve,
    Random,
    Bool,
    Enum,
    String,
    ArrangeParam,

    Max,
};

enum class ParamType {
    S32,
    F32,
    Bool,
    Enum,
    String,
    Custom, // used for arrange params
};

using EnumType = std::uint32_t;
using BitfieldType = std::uint32_t;

} // namespace mango