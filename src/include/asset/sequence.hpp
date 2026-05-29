#pragma once

#include "asset/calltable.hpp"

namespace mango {

class AssetCallTable;

class Sequence : public AssetCallTable {
public:
    Sequence() = default;

    ~Sequence() override = default;

    auto getType() const -> CallTableType override { return CallTableType::Sequence; }
};

} // namespace mango