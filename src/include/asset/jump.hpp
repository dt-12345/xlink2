#pragma once

#include "asset/calltable.hpp"

namespace mango {

class Jump : public AssetCallTable {
public:
    Jump() = default;

    ~Jump() override = default;
    
    auto getType() const -> CallTableType override { return CallTableType::Jump; }
};

} // namespace mango