#pragma once

#include "action/action.hpp"
#include "asset/handle.hpp"
#include "param/param.hpp"
#include "property/property.hpp"
#include "trigger/always.hpp"

#include <set>

namespace mango {

class User {
public:
    using KeyType = std::variant<std::uint32_t, std::string>;

    User() = default;
    explicit User(std::uint32_t hash) : mKey(hash) {}
    explicit User(std::string_view name) : mKey(std::string(name)) {}

    auto setHash(std::uint32_t hash) -> void;
    auto setName(std::string_view name) -> void;
    template <typename... Ts>
    auto addParam(Ts&&... args) -> Param& { return mParams.emplace_back(std::forward<Ts>(args)...); }
    template <typename... Ts>
    auto addLocalProperty(Ts&&... args) -> void { mLocalProperties.emplace(std::forward<Ts>(args)...); }
    template <typename... Ts>
    auto addActionSlot(Ts&&... args) -> ActionSlot& { return mActionSlots.emplace_back(std::forward<Ts>(args)...); }
    template <typename... Ts>
    auto addProperty(Ts&&... args) -> Property& { return mProperties.emplace_back(std::forward<Ts>(args)...); }
    template <typename... Ts>
    auto addAlwaysTrigger(Ts&&... args) -> AlwaysTrigger& { return mAlwaysTriggers.emplace_back(std::forward<Ts>(args)...); }
    template <typename... Ts>
    auto addAssetCallTable(Ts&&... args) -> AssetCallTableHandle& { return mAssetCallTables.emplace_back(std::forward<Ts>(args)...); }
    auto setUnknown(std::int16_t value) -> void { mUnknown = value; }

    [[nodiscard]] auto hasKnownName() const -> bool { return mKey.index() == 1; }
    [[nodiscard]] auto getKey() const -> const KeyType& { return mKey; }
    [[nodiscard]] auto getHash() const -> std::uint32_t;
    [[nodiscard]] auto getName() const -> std::string_view;
    [[nodiscard]] auto getParams() -> std::vector<Param>& { return mParams; }
    [[nodiscard]] auto getParams() const -> const std::vector<Param>& { return mParams; }
    [[nodiscard]] auto getLocalProperties() -> std::set<std::string>& { return mLocalProperties; }
    [[nodiscard]] auto getLocalProperties() const -> const std::set<std::string>& { return mLocalProperties; }
    [[nodiscard]] auto getActionSlots() -> std::vector<ActionSlot>& { return mActionSlots; }
    [[nodiscard]] auto getActionSlots() const -> const std::vector<ActionSlot>& { return mActionSlots; }
    [[nodiscard]] auto getProperties() -> std::vector<Property>& { return mProperties; }
    [[nodiscard]] auto getProperties() const -> const std::vector<Property>& { return mProperties; }
    [[nodiscard]] auto getAlwaysTriggers() -> std::vector<AlwaysTrigger>& { return mAlwaysTriggers; }
    [[nodiscard]] auto getAlwaysTriggers() const -> const std::vector<AlwaysTrigger>& { return mAlwaysTriggers; }
    [[nodiscard]] auto getAssetCallTables() -> std::vector<AssetCallTableHandle>& { return mAssetCallTables; }
    [[nodiscard]] auto getAssetCallTables() const -> const std::vector<AssetCallTableHandle>& { return mAssetCallTables; }
    [[nodiscard]] auto getUnknown() const -> std::int16_t { return mUnknown; }

private:
    KeyType mKey                                        = 0u;
    std::vector<Param> mParams                          = {};
    std::set<std::string> mLocalProperties              = {};
    std::vector<ActionSlot> mActionSlots                = {};
    std::vector<Property> mProperties                   = {};
    std::vector<AlwaysTrigger> mAlwaysTriggers          = {};
    std::vector<AssetCallTableHandle> mAssetCallTables  = {};
    std::int16_t mUnknown                               = -1;
};

} // namespace mango