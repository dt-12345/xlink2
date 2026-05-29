#pragma once

#include "asset/calltable.hpp"
#include "property/common.hpp"

namespace mango {

class AssetCallTable;

class Blend : public AssetCallTable {
public:
    Blend() = default;

    ~Blend() override = default;

    auto getType() const -> CallTableType override { return CallTableType::Blend; }
};

class BlendBy : public AssetCallTable{
public:
    BlendBy() = default;

    ~BlendBy() override = default;

    auto getType() const -> CallTableType override { return CallTableType::BlendBy; }

    auto setBlendProperty(std::string_view name, PropertyScope scope) -> void;
    auto setBlendProperty(const PropertyInfo& property) -> void;
    auto setBlendPropertyName(std::string_view name) -> void;
    auto setBlendPropertyScope(PropertyScope scope) -> void { mBlendProperty.scope = scope; }
    auto setUnknown(std::int32_t value) -> void { mUnknown = value; }

    [[nodiscard]] auto getBlendProperty() const -> const PropertyInfo& { return mBlendProperty; }
    [[nodiscard]] auto getBlendPropertyName() const -> std::string_view { return mBlendProperty.name; }
    [[nodiscard]] auto getBlendPropertyScope() const -> PropertyScope { return mBlendProperty.scope; }
    [[nodiscard]] auto getUnknown() const -> std::int32_t { return mUnknown; }

private:
    PropertyInfo mBlendProperty                     = {};
    std::int32_t mUnknown                           = -1;
};

} // namespace mango