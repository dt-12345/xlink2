#include "asset/grid.hpp"
#include "common/error.hpp"

namespace mango {

Grid::Grid(
    const PropertyInfo& prop1,
    const PropertyInfo& prop2,
    std::vector<std::string>&& values1,
    std::vector<std::string>&& values2,
    std::vector<AssetCallTableHandle>&& assetCallTables
) : mProperty1(prop1), mProperty2(prop2) {
    setValuesAndAssetCallTables(std::move(values1), std::move(values2), std::move(assetCallTables));
}

auto Grid::ensureValidSize() const -> void {
    if (mValues1.size() * mValues2.size() != mAssetCallTables.size()) {
        common::AbortWithDetail(
            "Grid must contain a case for each combination of values!\n"
            "Num Value 1: {}\n"
            "Num Value 2: {}\n"
            "Num Call Tables: {}\n",
            mValues1.size(), mValues2.size(), mAssetCallTables.size()
        );
    }
    if (mValues1.size() > 0xff || mValues2.size() > 0xff) {
        common::AbortWithDetail(
            "Grid has too many values! (max: 255)\n"
            "Num Value 1: {}\n"
            "Num Value 2: {}\n"
            "Num Call Tables: {}\n",
            mValues1.size(), mValues2.size(), mAssetCallTables.size()
        );
    }
}

auto Grid::setProperty1(std::string_view name, PropertyScope scope) -> void {
    mProperty1.name = name;
    mProperty2.scope = scope;
}

auto Grid::setProperty1(const PropertyInfo& prop) -> void {
    mProperty1 = prop;
}

auto Grid::setProperty1Name(std::string_view name) -> void {
    mProperty1.name = name;
}

auto Grid::setProperty2(std::string_view name, PropertyScope scope) -> void {
    mProperty2.name = name;
    mProperty2.scope = scope;
}

auto Grid::setProperty2(const PropertyInfo& prop) -> void {
    mProperty2 = prop;
}

auto Grid::setProperty2Name(std::string_view name) -> void {
    mProperty2.name = name;
}

auto Grid::setValuesAndAssetCallTables(
    std::vector<std::string>&& values1,
    std::vector<std::string>&& values2,
    std::vector<AssetCallTableHandle>&& assetCallTables
) -> void {
    mValues1 = std::move(values1);
    mValues2 = std::move(values2);
    mAssetCallTables = std::move(assetCallTables);
}

} // namespace mango