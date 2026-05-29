#include "asset/blend.hpp"

namespace mango {

auto BlendBy::setBlendProperty(std::string_view name, PropertyScope scope) -> void {
    mBlendProperty.name = name;
    mBlendProperty.scope = scope;
}

auto BlendBy::setBlendProperty(const PropertyInfo& property) -> void {
    mBlendProperty = property;
}

auto BlendBy::setBlendPropertyName(std::string_view name) -> void {
    mBlendProperty.name = name;
}

} // namespace mango