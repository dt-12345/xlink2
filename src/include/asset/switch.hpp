#pragma once

#include "asset/calltable.hpp"
#include "asset/handle.hpp"

#include <string>

namespace mango {

class AssetCallTable;

enum class SwitchType {
    Null,
    LocalProperty,
    GlobalProperty,
    ActionSlot,
};

class Switch : public AssetCallTable {
public:
    Switch() = default;

    ~Switch() override = default;

    auto getType() const -> CallTableType override { return CallTableType::Switch; }

    auto setSwitchVariable(std::string_view var) -> void;
    auto setSwitchType(SwitchType type) -> void { mType = type; }
    auto setUnknown(std::int32_t value) -> void { mUnknown = value; }

    [[nodiscard]] auto getSwitchVariable() const -> std::string_view { return mSwitchVariable; }
    [[nodiscard]] auto getSwitchType() const -> SwitchType { return mType; }
    [[nodiscard]] auto getUnknown() const -> std::int32_t { return mUnknown; }

private:
    std::string mSwitchVariable                 = "";
    SwitchType mType                            = SwitchType::Null;
    std::int32_t mUnknown                       = -1;
};

} // namespace mango