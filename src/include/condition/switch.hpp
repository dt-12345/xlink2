#pragma once

#include "asset/switch.hpp"
#include "condition/condition.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace mango {

enum class CompareType {
    Equal,
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual,
    NotEqual,
    Default,
};

class SwitchCondition : public ICondition {
public:
    using ValueType = std::variant<
        std::string, // for action and enums
        std::int32_t,
        float,
        bool
    >;

    SwitchCondition() = default;
    SwitchCondition(CompareType type, ValueType&& value) : mType(type), mValue(value) {}
    
    ~SwitchCondition() override = default;

    auto getType() const -> ConditionType override { return ConditionType::Switch; }

    auto setParentType(SwitchType type) -> void { mParentType = type; }
    auto setCompareType(CompareType type) -> void { mType = type; }
    auto setValue(ValueType&& value) -> void { mValue = std::move(value); }

    [[nodiscard]] auto getParentType() const -> SwitchType { return mParentType; }
    [[nodiscard]] auto getCompareType() const -> CompareType { return mType; }
    [[nodiscard]] auto getValue() const -> const ValueType& { return mValue; }

private:
    SwitchType mParentType                  = SwitchType::Null;
    CompareType mType                       = CompareType::Default;
    ValueType mValue                        = 0;
};

} // namespace mango