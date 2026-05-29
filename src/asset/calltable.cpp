#include "asset/calltable.hpp"

namespace mango {

auto AssetCallTable::addChild(AssetCallTableHandle child) -> void {
    mChildren.emplace_back(std::move(child));
}

auto AssetCallTable::setParent(AssetCallTableHandle parent) -> void {
    mParent = std::move(parent);
}

auto AssetCallTable::setKeyName(std::string_view name) -> void {
    mKeyName = name;
}

auto AssetCallTable::getDepth() const -> std::uint32_t {
    return 1 + (getParent() ? getParent()->getDepth() : 0);
}

} // namespace mango