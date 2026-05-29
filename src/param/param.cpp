#include "common/error.hpp"
#include "param/param.hpp"

#include <utility>

namespace mango {

static constexpr auto ValueToParamType(ValueType vt) -> ParamType {
    switch (vt) {
        case ValueType::S32:
        case ValueType::Bitfield:
            return ParamType::S32;
        case ValueType::F32:
        case ValueType::Curve:
        case ValueType::Random:
            return ParamType::F32;
        case ValueType::Bool:
            return ParamType::Bool;
        case ValueType::Enum:
            return ParamType::Enum;
        case ValueType::String:
            return ParamType::String;
        case ValueType::ArrangeParam:
            return ParamType::Custom;
        default:
            return ParamType::S32;
    }
}

template <ValueType VT, typename... Ts>
static auto SetValue(ParamValue& param, Ts&&... args) -> void {
    param.emplace<static_cast<std::size_t>(VT)>(std::forward<Ts>(args)...);
}

Param::Param(std::string_view name, ParamType type, ParamValue&& value) : mName(name), mType(type), mValue(std::move(value)) {}

auto Param::setName(std::string_view name) -> void {
    mName = name;
}

auto Param::setType(ParamType type) -> void {
    switch (type) {
        case ParamType::S32:
            SetValue<ValueType::S32>(mValue, 0);
            break;
        case ParamType::F32:
            SetValue<ValueType::F32>(mValue, 0.f);
            break;
        case ParamType::Bool:
            SetValue<ValueType::Bool>(mValue, false);
            break;
        case ParamType::Enum:
            SetValue<ValueType::Enum>(mValue, 0u);
            break;
        case ParamType::String:
            SetValue<ValueType::String>(mValue, std::make_unique<std::string>());
            break;
        case ParamType::Custom:
            SetValue<ValueType::ArrangeParam>(mValue, std::make_unique<ArrangeParam>());
            break;
        default:
            common::AbortWithDetail("Invalid param type! {:#x}", std::to_underlying(type));
    }
    mType = type; // do this last so if the input value is not a valid type we don't end up in an invalid state
}

auto Param::setValue(ParamValue&& value) -> void {
    if (ValueToParamType(getValueType()) != mType) {
        common::AbortWithDetail("Value does not match current param type!");
    }

    mValue = std::move(value);
}

auto Param::setTypeAndValue(ParamType type, ParamValue&& value) -> void {
    if (ValueToParamType(static_cast<ValueType>(value.index())) != type) {
        common::AbortWithDetail("Mismatching param and value types!");
    }

    mType = type;
    mValue = std::move(value);
}

} // namespace mango