#pragma once

#include "asset/calltable.hpp"

namespace mango {

class Random : public AssetCallTable {
public:
    Random() = default;

    ~Random() override = default;

    auto getType() const -> CallTableType override { return CallTableType::Random; }
};

class RandomNoRepeat : public AssetCallTable {
public:
    RandomNoRepeat() = default;

    ~RandomNoRepeat() override = default;

    auto getType() const -> CallTableType override { return CallTableType::RandomNoRepeat; }
};

} // namespace