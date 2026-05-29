#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mango {

enum class LimitType {
    None, // limits by whatever order the events happen to be in (emit order)
    // these orderings refer to which assets have the highest priority (so limited last)
    PriorityThenOldest,
    PriorityThenNewest,
    OldestThenPriority,
    NewestThenPriority,
    SpatialPriorityThenOldest,
    SpatialPriorityThenNewest,
    OldestThenSpatialPriority,
    NewestThenSpatialPriority,
};

class ArrangeGroup {
public:
    ArrangeGroup() = default;

    auto setName(std::string_view name) -> void;
    auto setLimitType(LimitType type) -> void { mLimitType = type; }
    auto setThreshold(std::int8_t threshold) -> void { mThreshold = threshold; }
    auto setIncludeFading(bool value) -> void { mIncludeFading = value;  }

    [[nodiscard]] auto getName() const -> std::string_view { return mName; }
    [[nodiscard]] auto getLimitType() const -> LimitType { return mLimitType; }
    [[nodiscard]] auto getThreshold() const -> std::int8_t { return mThreshold; }
    [[nodiscard]] auto getIncludeFading() const -> bool { return mIncludeFading; }

private:
    std::string mName = "";
    LimitType mLimitType = LimitType::PriorityThenOldest;   // how to choose which events are limited first
    std::int8_t mThreshold = -1;                            // how many events need to be active before some start to be limited
    bool mIncludeFading = false;                            // include fading events in count
};

using ArrangeParam = std::vector<ArrangeGroup>;

} // namespace mango