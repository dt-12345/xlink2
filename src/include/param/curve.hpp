#pragma once

#include "property/common.hpp"

#include <cstdint>
#include <vector>

namespace mango {

enum class CurveType {
    Standard,
    Constant,
};

enum class CurveUpdateMode {
    LocalVolatile,
    LocalStable,
    Global,
};

struct CurvePoint {
    float x, y;
};

class Curve {
public:
    Curve() = default;

    auto addPoint(const CurvePoint& point) -> CurvePoint&;
    auto addPoint(float x, float y) -> CurvePoint&;
    auto setPropertyName(std::string_view name) -> void;
    auto setPropertyScope(PropertyScope scope) -> void;
    auto setProperty(const PropertyInfo& prop) -> void;
    auto setType(CurveType type) -> void { mType = type; }
    auto setUnknown(std::int32_t value) -> void { mUnknown = value; }
    auto setUpdateType(CurveUpdateMode value) -> void { mUpdateType = value; }

    [[nodiscard]] auto getPoints() -> std::vector<CurvePoint>& { return mPoints; }
    [[nodiscard]] auto getPoints() const -> const std::vector<CurvePoint>& { return mPoints; }
    [[nodiscard]] auto getPoint(size_t index) const -> CurvePoint { return mPoints.at(index); }
    [[nodiscard]] auto getProperty() -> PropertyInfo& { return mProperty; }
    [[nodiscard]] auto getProperty() const -> const PropertyInfo& { return mProperty; }
    [[nodiscard]] auto getPropertyName() const -> std::string_view { return mProperty.name; }
    [[nodiscard]] auto getPropertyScope() const -> PropertyScope { return mProperty.scope; }
    [[nodiscard]] auto getType() const -> CurveType { return mType; }
    [[nodiscard]] auto getUnknown() const -> std::int32_t { return mUnknown; }
    [[nodiscard]] auto getUpdateType() const -> CurveUpdateMode { return mUpdateType; }

private:
    std::vector<CurvePoint> mPoints = {};
    PropertyInfo mProperty          = {};
    CurveType mType                 = CurveType::Standard;
    std::int32_t mUnknown           = 0; // if this is < 0, the curve returns infinity
    CurveUpdateMode mUpdateType     = CurveUpdateMode::LocalVolatile;
};

/*

curves will work with non-float properties, but will always output a float

auto getCurvePoint(const ResCurve* curve, const ResCurvePoint* pointTable, float value) -> float {
    if (pointTable == nullptr) {
        return INFINITY;
    }

    const auto points = pointTable + curve->pointStartIndex;
    const auto numPoints = curve->numPoints;
    auto pointValue = (points + numPoints - 1)->y;

    if (numPoints == 0 || curve->type != CurveType::Standard) {
        return pointValue;
    }

    for (s32 i = 0; i < numPoints; ++i) {
        const auto point = points + i;
        if (point->x == value) {
            if (i + 1 < numPoints && (point + 1)->x == value) {
                if ((point + 1)->y != INFINITY) {
                    pointValue = (point + 1)->y;
                }
            } else {
                if (return point->y != INFINITY) {
                    pointValue = point->y;
                }
            }
            break;
        } else if (point->x > value) {
            if (i == 0) {
                if (point->y != INFINITY) {
                    pointValue = point->y;
                }
            } else {
                const auto lastPoint = point - 1;
                const auto val = lastPoint->y + (value - lastPoint->x) * (point->y - lastPoint->y) / (point->x - lastPoint->x);
                if (val != INFINITY) {
                    pointValue = val;
                }
            }
            break;
        }
    }

    return pointValue;
}

*/

} // namespace mango