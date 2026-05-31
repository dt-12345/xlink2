#pragma once

#include "asset/handle.hpp"
#include "common/guid.hpp"
#include "condition/condition.hpp"

#include <cstdint>
#include <vector>

namespace mango {

enum class CallTableType {
    Switch,
    Random,
    RandomNoRepeat,
    Blend,
    BlendBy,
    Sequence,
    Grid,
    Jump,
    Asset,
};

class AssetCallTable {
public:
    virtual ~AssetCallTable() = 0;
    virtual auto getType() const -> CallTableType = 0;

    auto addChild(AssetCallTableHandle child) -> void;
    auto setParent(AssetCallTableHandle parent) -> void;
    auto setKeyName(std::string_view name) -> void;
    auto rollGUID() -> void { mGUID = common::GenerateGUID(); }
    auto setGUID(std::uint32_t guid) -> void { mGUID = guid; }
    auto setCondition(std::unique_ptr<ICondition> cond) -> void { mCondition = std::move(cond); }
    auto setEmitCount(std::int32_t count) -> void { mEmitCount = count; }
    auto setOneShot(bool value) -> void { mOneShot = value; }
    auto setNoPause(bool value) -> void { mNoPause = value; }
    auto setUserFlags(std::uint8_t value) -> void { mUserFlags = value; }

    [[nodiscard]] auto getChildren() -> std::vector<AssetCallTableHandle>& { return mChildren; }
    [[nodiscard]] auto getChildren() const -> const std::vector<AssetCallTableHandle>& { return mChildren; }
    [[nodiscard]] auto getParent() -> AssetCallTableHandle& { return mParent; }
    [[nodiscard]] auto getParent() const -> const AssetCallTableHandle& { return mParent; }
    [[nodiscard]] auto getKeyName() const -> std::string_view { return mKeyName; }
    [[nodiscard]] auto getGUID() const -> std::uint32_t { return mGUID; }
    [[nodiscard]] auto getCondition() const -> const std::unique_ptr<ICondition>& { return mCondition; }
    [[nodiscard]] auto isAsset() const -> bool { return getType() == CallTableType::Asset; }
    [[nodiscard]] auto isContainer() const -> bool { return !isAsset(); }
    [[nodiscard]] auto getEmitCount() const -> std::int32_t { return mEmitCount; }
    [[nodiscard]] auto getOneShot() const -> bool { return mOneShot; }
    [[nodiscard]] auto getNoPause() const -> bool { return mNoPause; }
    [[nodiscard]] auto getUserFlags() const -> std::uint8_t { return mUserFlags; }
    [[nodiscard]] auto getDepth() const -> std::uint32_t;

protected:
    std::vector<AssetCallTableHandle> mChildren = {};
    AssetCallTableHandle mParent                = { nullptr };
    std::string mKeyName                        = "";
    std::unique_ptr<ICondition> mCondition      = {};
    std::uint32_t mGUID                         = 0u;
    std::int32_t mEmitCount                     = 1;     // how many times to emit the effect/sound when the event is requested a single time
    bool mOneShot                               = false; // this event is only active when actively being requested
    bool mNoPause                               = false; // this event cannot be paused
    std::uint8_t mUserFlags                     = 0;
};

inline AssetCallTable::~AssetCallTable() = default;

} // namespace mango