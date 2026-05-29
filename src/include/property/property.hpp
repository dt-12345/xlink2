#pragma once

#include "property/common.hpp"
#include "trigger/property.hpp"

namespace mango {

class Property {
public:
    Property() = default;

    auto setInfo(std::string_view name, PropertyScope scope) -> void;
    auto setInfo(const PropertyInfo& info) -> void;
    auto setName(std::string_view name) -> void;
    auto setScope(PropertyScope scope) -> void { mInfo.scope = scope; }
    auto addTrigger() -> PropertyTrigger&;

    [[nodiscard]] auto getInfo() const -> const PropertyInfo& { return mInfo; }
    [[nodiscard]] auto getName() const -> std::string_view { return mInfo.name; }
    [[nodiscard]] auto getScope() const -> PropertyScope { return mInfo.scope; }
    [[nodiscard]] auto getTriggers() -> std::vector<PropertyTrigger>& { return mTriggers; }
    [[nodiscard]] auto getTriggers() const -> const std::vector<PropertyTrigger>& { return mTriggers; }

private:
    PropertyInfo mInfo                      = {};
    std::vector<PropertyTrigger> mTriggers  = {};
};

} // namespace mango