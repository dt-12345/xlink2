#pragma once

#include "param/arrange.hpp"
#include "param/curve.hpp"
#include "param/random.hpp"
#include "param/types.hpp"

#include <memory>
#include <variant>

namespace mango {

using ParamValue = std::variant<
    // ints
    std::int32_t,
    BitfieldType,
    // floats
    float,
    std::unique_ptr<Curve>,
    std::unique_ptr<RandomTable>,
    // bools
    bool,
    // enums
    EnumType,
    // strings
    std::unique_ptr<std::string>,
    // arrange params
    std::unique_ptr<ArrangeParam>
>;
static_assert(std::variant_size_v<ParamValue> == static_cast<std::size_t>(ValueType::Max));

[[nodiscard]] static constexpr auto EnumValue(std::uint32_t value) -> ParamValue {
    return ParamValue{ std::in_place_index<static_cast<std::size_t>(ValueType::Enum)>, value };
}

[[nodiscard]] static constexpr auto BitfieldValue(std::uint32_t value) -> ParamValue {
    return ParamValue{ std::in_place_index<static_cast<std::size_t>(ValueType::Bitfield)>, value };
}

class Param {
public:
    Param() = default;
    Param(std::string_view name, ParamType type, ParamValue&& value);

    auto setName(std::string_view name) -> void;
    auto setType(ParamType type) -> void;
    auto setValue(ParamValue&& value) -> void;
    auto setTypeAndValue(ParamType type, ParamValue&& value) -> void;

    [[nodiscard]] auto getName() const -> std::string_view { return mName; }
    [[nodiscard]] auto getParamType() const -> ParamType { return mType; }
    [[nodiscard]] auto getValueType() const -> ValueType { return static_cast<ValueType>(mValue.index()); }
    [[nodiscard]] auto getValue() const -> const ParamValue& { return mValue; }
    template <ValueType VT>
    [[nodiscard]] auto getValue() const -> const auto& { return std::get<static_cast<std::size_t>(VT)>(mValue); }
    template <ValueType VT>
    [[nodiscard]] auto getValue() -> auto& { return std::get<static_cast<std::size_t>(VT)>(mValue); }

private:
    std::string mName = "";
    ParamType mType   = ParamType::S32;
    ParamValue mValue = 0;
};

}; // namespace mango