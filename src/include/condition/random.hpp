#pragma once

#include "condition/condition.hpp"

namespace mango {

class RandomCondition : public ICondition {
public:
    RandomCondition() = default;
    explicit RandomCondition(float weight) : mWeight(weight) {}

    ~RandomCondition() override = default;

    auto getType() const -> ConditionType override { return ConditionType::Random; }

    auto setWeight(float weight) -> void { mWeight = weight; }

    [[nodiscard]] auto getWeight() const -> float { return mWeight; }

private:
    float mWeight = 1.f;
};

class RandomNoRepeatCondition : public ICondition {
public:
    RandomNoRepeatCondition() = default;
    explicit RandomNoRepeatCondition(float weight) : mWeight(weight) {}

    ~RandomNoRepeatCondition() override = default;

    auto getType() const -> ConditionType override { return ConditionType::RandomNoRepeat; }

    auto setWeight(float weight) -> void { mWeight = weight; }

    [[nodiscard]] auto getWeight() const -> float { return mWeight; }

private:
    float mWeight = 1.f;
};

} // namespace mango