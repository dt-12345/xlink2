#include "property/property.hpp"

namespace mango {

auto Property::setInfo(std::string_view name, PropertyScope scope) -> void {
    mInfo.name = name;
    mInfo.scope = scope;
}

auto Property::setInfo(const PropertyInfo& info) -> void {
    mInfo = info;
}

auto Property::setName(std::string_view name) -> void {
    mInfo.name = name;
}

auto Property::addTrigger() -> PropertyTrigger& {
    return mTriggers.emplace_back();
}

} // namespace mango