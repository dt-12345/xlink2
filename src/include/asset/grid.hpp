#pragma once

#include "asset/calltable.hpp"
#include "asset/handle.hpp"
#include "property/common.hpp"

namespace mango {

class AssetCallTable;

class Grid : public AssetCallTable {
public:
    Grid() = default;
    Grid(
        const PropertyInfo& prop1,
        const PropertyInfo& prop2,
        std::vector<std::string>&& values1,
        std::vector<std::string>&& values2,
        std::vector<AssetCallTableHandle>&& assetCallTables
    );

    ~Grid() override = default;

    auto getType() const -> CallTableType override { return CallTableType::Grid; }

    auto ensureValidSize() const -> void;

    auto setProperty1(std::string_view name, PropertyScope scope) -> void;
    auto setProperty1(const PropertyInfo& prop) -> void;
    auto setProperty1Name(std::string_view name) -> void;
    auto setProperty1Scope(PropertyScope scope) -> void { mProperty1.scope = scope; }
    auto setProperty2(std::string_view name, PropertyScope scope) -> void;
    auto setProperty2(const PropertyInfo& prop) -> void;
    auto setProperty2Name(std::string_view name) -> void;
    auto setProperty2Scope(PropertyScope scope) -> void { mProperty2.scope = scope; }
    auto setValuesAndAssetCallTables(
        std::vector<std::string>&& values1,
        std::vector<std::string>&& values2,
        std::vector<AssetCallTableHandle>&& assetCallTables
    ) -> void;

    [[nodiscard]] auto getProperty1() const -> const PropertyInfo { return mProperty1; }
    [[nodiscard]] auto getProperty1Name() const -> std::string_view { return mProperty1.name; }
    [[nodiscard]] auto getProperty1Scope() const -> PropertyScope { return mProperty1.scope; }
    [[nodiscard]] auto getProperty2() const -> const PropertyInfo { return mProperty2; }
    [[nodiscard]] auto getProperty2Name() const -> std::string_view { return mProperty2.name; }
    [[nodiscard]] auto getProperty2Scope() const -> PropertyScope { return mProperty2.scope; }
    [[nodiscard]] auto getValues1() -> std::vector<std::string>& { return mValues1; }
    [[nodiscard]] auto getValues1() const -> const std::vector<std::string>& { return mValues1; }
    [[nodiscard]] auto getValues2() -> std::vector<std::string>& { return mValues2; }
    [[nodiscard]] auto getValues2() const -> const std::vector<std::string>& { return mValues2; }
    [[nodiscard]] auto getAssetCallTables() -> std::vector<AssetCallTableHandle>& { return mAssetCallTables; }
    [[nodiscard]] auto getAssetCallTables() const -> const std::vector<AssetCallTableHandle>& { return mAssetCallTables; }

private:
    PropertyInfo mProperty1                             = {};
    PropertyInfo mProperty2                             = {};
    std::vector<std::string> mValues1                   = {};
    std::vector<std::string> mValues2                   = {};
    std::vector<AssetCallTableHandle> mAssetCallTables  = {};
};

} // namespace banan