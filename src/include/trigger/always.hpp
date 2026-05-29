#pragma once

#include "asset/handle.hpp"
#include "common/guid.hpp"
#include "param/param.hpp"

namespace mango {

class AssetCallTable;

class AlwaysTrigger {
public:
    AlwaysTrigger() = default;

    AlwaysTrigger(const AlwaysTrigger&) = delete;
    AlwaysTrigger(AlwaysTrigger&&) = default;

    auto operator=(const AlwaysTrigger&) -> AlwaysTrigger& = delete;
    auto operator=(AlwaysTrigger&&) -> AlwaysTrigger& = default;

    auto rollGUID() -> void { mGUID = common::GenerateGUID(); };
    auto setGUID(std::uint32_t guid) -> void { mGUID = guid; }
    auto setFlags(std::uint16_t flags) -> void { mFlags = flags; }
    auto setUnknown(std::uint16_t value) -> void { mUnknown = value; }
    auto setAssetCallTable(AssetCallTableHandle act) -> void;
    auto addOverwriteParam() -> Param&;

    [[nodiscard]] auto getGUID() const -> std::uint32_t { return mGUID; }
    [[nodiscard]] auto getFlags() const -> std::uint16_t { return mFlags; }
    [[nodiscard]] auto getUnknown() const -> std::uint16_t { return mUnknown; }
    [[nodiscard]] auto getAssetCallTable() const -> const AssetCallTable* { return mAssetCallTable.get(); }
    [[nodiscard]] auto getOverwriteParams() -> std::vector<Param>& { return mTriggerOverwriteParam; }
    [[nodiscard]] auto getOverwriteParams() const -> const std::vector<Param>& { return mTriggerOverwriteParam; }

private:
    std::uint32_t mGUID                         = 0u;
    std::uint16_t mFlags                        = 0u; // TODO: figure what these do (only non-zero in two games)
    std::uint16_t mUnknown                      = 0u;
    AssetCallTableHandle mAssetCallTable        = { nullptr };
    std::vector<Param> mTriggerOverwriteParam   = {};
};

} // namespace mango