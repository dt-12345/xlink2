#pragma once

#include "condition/condition.hpp"

#include <cstdint>

namespace mango {

class SequenceCondition : public ICondition {
public:
    ~SequenceCondition() override = default;

    auto getType() const -> ConditionType override { return ConditionType::Sequence; }

    auto setContinueOnFade(std::uint32_t value) -> void { mContinueOnFade = value; }

    [[nodiscard]] auto getContinueOnFade() const -> std::uint32_t { return mContinueOnFade; }

private:
    std::uint32_t mContinueOnFade = 0u;
};

} // namespace mango