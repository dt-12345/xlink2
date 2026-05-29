#pragma once

namespace mango {

enum class ConditionType {
    Switch,
    Random,
    RandomNoRepeat,
    Blend,              // no corresponding condition
    BlendBy,
    Sequence,
    Grid,               // no corresponding condition
    Jump,               // no corresponding condition
};

class ICondition {
public:
    virtual ~ICondition() = 0;
    virtual auto getType() const -> ConditionType = 0;
};

inline ICondition::~ICondition() = default;

} // namespace mango