#pragma once

#include "asset/handle.hpp"
#include "condition/switch.hpp"
#include "common/guid.hpp"
#include "param/param.hpp"

namespace mango {

class PropertyTrigger {
public:
    PropertyTrigger() = default;

    PropertyTrigger(const PropertyTrigger&) = delete;
    PropertyTrigger(PropertyTrigger&&) = default;

    auto operator=(const PropertyTrigger&) -> PropertyTrigger& = delete;
    auto operator=(PropertyTrigger&&) -> PropertyTrigger& = default;

    auto rollGUID() -> void { mGUID = common::GenerateGUID(); };
    auto setGUID(std::uint32_t guid) -> void { mGUID = guid; }
    auto setLazy(bool value) -> void { mLazy = value; }
    auto setUnknown(std::uint16_t value) -> void { mUnknown = value; }
    auto setCondition(SwitchCondition&& condition) -> void;
    auto setCondition(CompareType type, SwitchCondition::ValueType&& value) -> void;
    auto setAssetCallTable(AssetCallTableHandle act) -> void;
    auto addOverwriteParam() -> Param&;

    [[nodiscard]] auto getGUID() const -> std::uint32_t { return mGUID; }
    [[nodiscard]] auto getLazy() const -> bool { return mLazy; }
    [[nodiscard]] auto getUnknown() const -> std::uint16_t { return mUnknown; }
    [[nodiscard]] auto getCondition() -> SwitchCondition& { return mCondition; }
    [[nodiscard]] auto getCondition() const -> const SwitchCondition& { return mCondition; }
    [[nodiscard]] auto getAssetCallTable() const -> const AssetCallTable* { return mAssetCallTable.get(); }
    [[nodiscard]] auto getOverwriteParams() -> std::vector<Param>& { return mTriggerOverwriteParam; }
    [[nodiscard]] auto getOverwriteParams() const -> const std::vector<Param>& { return mTriggerOverwriteParam; }

private:
    std::uint32_t mGUID                         = 0u;
    bool mLazy                                  = false; // property must change to target before triggering so this will not trigger if the property starts out at the target
    std::uint16_t mUnknown                      = 0u;
    SwitchCondition mCondition                  = {};
    AssetCallTableHandle mAssetCallTable        = { nullptr };
    std::vector<Param> mTriggerOverwriteParam   = {};
};

} // namespace mango