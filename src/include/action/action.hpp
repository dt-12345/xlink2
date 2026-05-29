#pragma once

#include "trigger/action.hpp"

#include <string>
#include <vector>

namespace mango {

class Action {
public:
    Action() = default;
    explicit Action(std::string_view name) : mName(name), mTriggers() {}

    auto setName(std::string_view name) -> void { mName = name; }
    auto addTrigger() -> ActionTrigger& { return mTriggers.emplace_back(); }
    auto setIsPrefix(bool value) -> void { mIsPrefix = value; }

    auto getName() const -> std::string_view { return mName; }
    auto getTriggers() -> std::vector<ActionTrigger>& { return mTriggers; }
    auto getTriggers() const -> const std::vector<ActionTrigger>& { return mTriggers; }
    auto getIsPrefix() const -> bool { return mIsPrefix; }

private:
    std::string mName                       = "";
    std::vector<ActionTrigger> mTriggers    = {};
    bool mIsPrefix                          = false;
};

class ActionSlot {
public:
    ActionSlot() = default;
    explicit ActionSlot(std::string_view name) : mName(name), mActions() {}

    auto setName(std::string_view name) -> void { mName = name; }
    auto addAction() -> Action& { return mActions.emplace_back(); }

    auto getName() const -> std::string_view { return mName; }
    auto getActions() -> std::vector<Action>& { return mActions; }
    auto getActions() const -> const std::vector<Action>& { return mActions; }

private:
    std::string mName               = "";
    std::vector<Action> mActions    = {};
};

} // namespace mango