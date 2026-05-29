#pragma once

#include <array>

namespace mango {

enum class RandomType {
    Linear,
    InflectedPolynomial,
    IncreasingPolynomial,
    DecreasingPolynomial,
};

/*

these are implemented as follows

auto getRandomLinear(float min, float max) -> float {
    min = min < max ? min : max;
    return min + absf(max - min) * random(0.f, 1.f);
}

auto getRandomInflectedPolynomial(float min, float max, float power) -> float {
    const auto range = absf(max - min) / 2.f;
    const auto value = random(-1.f, 1.f);

    return value < 0.f
        ? min + range + range * powf(absf(value), power)
        : min + range - range * powf(absf(value), power);
}

auto getRandomIncreasingPolynomial(float min, float max, float power) -> float {
    return min + absf(max - min) * powf(random(0.f, 1.f), power);
}

auto getRandomDecreasingPolynomial(float min, float max, float power) -> float {
    return min + absf(max - min) * powf(1.f - random(0.f, 1.f), power);
}

*/

class RandomTable {
public:
    RandomTable() = default;
    
    RandomTable(RandomType type, float min, float max)
        : mType(type), mMinValue(min), mMaxValue(max) {}

    RandomTable(RandomType type, float power, float min, float max)
        : mType(type), mPower(power), mMinValue(min), mMaxValue(max) {}

    auto setType(RandomType type) -> void { mType = type; }
    auto setPower(float value) -> void { if (IsValidPower(value)) { mPower = value; } }
    auto setMin(float value) -> void { mMinValue = value; }
    auto setMax(float value) -> void { mMaxValue = value; }

    [[nodiscard]] auto getType() const -> RandomType { return mType; }
    [[nodiscard]] auto getPower() const -> float { return mPower; }
    [[nodiscard]] auto getMin() const -> float { return mMinValue; }
    [[nodiscard]] auto getMax() const -> float { return mMaxValue; }

private:
    static constexpr const auto cValidPowers = std::array{ 2.f, 3.f, 4.f, 1.5f };
    [[nodiscard]] static auto IsValidPower(float value) -> bool;

    RandomType mType = RandomType::Linear;
    float mPower    = 2.f;
    float mMinValue  = 0.f;
    float mMaxValue  = 1.f;
};

} // namespace mango