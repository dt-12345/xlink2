#pragma once

#include "condition/condition.hpp"

#include <cstdint>

namespace mango {

class SequenceCondition : public ICondition {
public:
    ~SequenceCondition() override = default;

    auto getType() const -> ConditionType override { return ConditionType::Sequence; }

    auto setForceContinue(std::uint32_t value) -> void { mForceContinue = value; }

    [[nodiscard]] auto getForceContinue() const -> std::uint32_t { return mForceContinue; }

private:
    std::uint32_t mForceContinue = 0u;
};

} // namespace mango