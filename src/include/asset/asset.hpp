#pragma once

#include "asset/calltable.hpp"
#include "param/param.hpp"

namespace mango {

class Asset : public AssetCallTable {
public:
    ~Asset() override = default;
    auto getType() const -> CallTableType override { return CallTableType::Asset; }

    auto getParams() -> std::vector<Param>& { return mParams; }
    auto getParams() const -> const std::vector<Param>& { return mParams; }

private:
    std::vector<Param> mParams;
};

} // namespace mango