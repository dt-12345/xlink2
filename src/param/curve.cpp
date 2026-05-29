#include "param/curve.hpp"

namespace mango {

auto Curve::addPoint(const CurvePoint& point) -> CurvePoint& {
    return mPoints.emplace_back(point);
}

auto Curve::addPoint(float x, float y) -> CurvePoint& {
    return mPoints.emplace_back(CurvePoint{ x, y });
}

auto Curve::setPropertyName(std::string_view name) -> void {
    mProperty.name = name;
}

auto Curve::setPropertyScope(PropertyScope scope) -> void {
    mProperty.scope = scope;
}

auto Curve::setProperty(const PropertyInfo& prop) -> void {
    mProperty = prop;
}


} // namespace mango