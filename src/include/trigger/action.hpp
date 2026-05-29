#pragma once

#include "asset/handle.hpp"
#include "common/guid.hpp"
#include "param/param.hpp"

namespace mango {

enum class ActionTriggerType {
    FrameWindow,    // trigger when the action occurs and is within the specified frame window
    Always,         // always trigger whenever the action occurs
    OnLeave,        // trigger when the action ends
    Previous,       // trigger when the action occurs if the previous action matches the condition
};

class ActionTrigger {
public:
    ActionTrigger() = default;

    ActionTrigger(const ActionTrigger&) = delete;
    ActionTrigger(ActionTrigger&&) = default;

    auto operator=(const ActionTrigger&) -> ActionTrigger& = delete;
    auto operator=(ActionTrigger&&) -> ActionTrigger& = default;

    auto rollGUID() -> void { mGUID = common::GenerateGUID(); };
    auto setGUID(std::uint32_t guid) -> void { mGUID = guid; }
    auto setType(ActionTriggerType type) -> void { mType = type; }
    auto setStartFrame(std::int32_t frame) -> void { mStartFrame = frame; }
    auto setEndFrame(std::int32_t frame) -> void { mEndFrame = frame; }
    auto setPreviousAction(std::string_view action) -> void { mPreviousAction = action; }
    auto setUnknown1(std::uint32_t value) -> void { mUnknown1 = value; }
    auto setUnknown2(std::uint16_t value) -> void { mUnknown2 = value; }
    auto setOneShot(bool value) -> void { mOneShot = value; }
    auto setAssetCallTable(AssetCallTableHandle act) -> void;
    auto addOverwriteParam() -> Param&;

    [[nodiscard]] auto getGUID() const -> std::uint32_t { return mGUID; }
    [[nodiscard]] auto getType() const -> ActionTriggerType { return mType; }
    [[nodiscard]] auto getStartFrame() const -> std::int32_t { return mStartFrame; }
    [[nodiscard]] auto getEndFrame() const -> std::int32_t { return mEndFrame; }
    [[nodiscard]] auto getPreviousAction() const -> std::string_view { return mPreviousAction; }
    [[nodiscard]] auto getUnknown1() const -> std::uint32_t { return mUnknown1; }
    [[nodiscard]] auto getUnknown2() const -> std::uint16_t { return mUnknown2; }
    [[nodiscard]] auto getOneShot() const -> bool { return mOneShot; }
    [[nodiscard]] auto getAssetCallTable() const -> const AssetCallTable* { return mAssetCallTable.get(); }
    [[nodiscard]] auto getOverwriteParams() -> std::vector<Param>& { return mTriggerOverwriteParam; }
    [[nodiscard]] auto getOverwriteParams() const -> const std::vector<Param>& { return mTriggerOverwriteParam; }

private:
    std::uint32_t mGUID                         = 0u;
    ActionTriggerType mType                     = ActionTriggerType::FrameWindow;
    // these correspond to the start/end frames of the action, not the emitted asset
    std::int32_t mStartFrame                    = 0;            // if -1, event will be emitted when the action ends and is not followed by other actions
    std::int32_t mEndFrame                      = 0x7fffffff;
    std::string mPreviousAction                 = "";           // previous action condition
    std::uint32_t mUnknown1                     = 0u;
    std::uint16_t mUnknown2                     = 0u;
    bool mOneShot                               = false;        // triggered only once when the action is active (action must change for re-emission)
    AssetCallTableHandle mAssetCallTable        = { nullptr };
    std::vector<Param> mTriggerOverwriteParam   = {};
};

} // namespace mango