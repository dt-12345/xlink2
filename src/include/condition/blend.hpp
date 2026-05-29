#pragma once

#include "condition/condition.hpp"

namespace mango {

enum class BlendOp {
    None,
    Multiply,
    SquareRoot,
    Sin,
    Add,
    SetToOne,
};

class BlendByCondition : public ICondition {
public:
    BlendByCondition() = default;
    BlendByCondition(float min, float max, BlendOp opMin, BlendOp opMax)
        : mValueMin(min), mValueMax(max), mBlendOpMin(opMin), mBlendOpMax(opMax) {}

    ~BlendByCondition() override = default;

    auto getType() const -> ConditionType override { return ConditionType::BlendBy; }

    auto setValueMin(float value) -> void { mValueMin = value; }
    auto setValueMax(float value) -> void { mValueMax = value; }
    auto setBlendOpMin(BlendOp op) -> void { mBlendOpMin = op; }
    auto setBlendOpMax(BlendOp op) -> void { mBlendOpMax = op; }

    [[nodiscard]] auto getValueMin() const -> float { return mValueMin; }
    [[nodiscard]] auto getValueMax() const -> float { return mValueMax; }
    [[nodiscard]] auto getBlendOpMin() const -> BlendOp { return mBlendOpMin; }
    [[nodiscard]] auto getBlendOpMax() const -> BlendOp { return mBlendOpMax; }

private:
    float mValueMin                         = 0.f;
    float mValueMax                         = 1.f;
    BlendOp mBlendOpMin                     = BlendOp::None;
    BlendOp mBlendOpMax                     = BlendOp::None;
};

} // namespace mango