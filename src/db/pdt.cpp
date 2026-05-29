#include "common/error.hpp"
#include "db/pdt.hpp"

#include <utility>

namespace mango {

auto ParamDefineTable::reset() -> void {
    mSystemUserParams.clear();
    mCustomUserParams.clear();
    mSystemAssetParams.clear();
    mCustomAssetParams.clear();
    mTriggerParams.clear();
}

auto ParamDefineTable::addSystemUserParam() -> Param& {
    return mSystemUserParams.emplace_back();
}

auto ParamDefineTable::addCustomUserParam() -> Param& {
    return mCustomUserParams.emplace_back();
}

auto ParamDefineTable::addSystemAssetParam() -> Param& {
    return mSystemAssetParams.emplace_back();
}

auto ParamDefineTable::addCustomAssetParam() -> Param& {
    return mCustomAssetParams.emplace_back();
}

auto ParamDefineTable::addTriggerParam() -> Param& {
    return mTriggerParams.emplace_back();
}

auto ParamDefineTable::getSystemUserParam(size_t index) -> Param& {
    return mSystemUserParams.at(index);
}

auto ParamDefineTable::getSystemUserParam(size_t index) const -> const Param& {
    return mSystemUserParams.at(index);
}

auto ParamDefineTable::getCustomUserParam(size_t index) -> Param& {
    return mCustomUserParams.at(index);
}

auto ParamDefineTable::getCustomUserParam(size_t index) const -> const Param& {
    return mCustomUserParams.at(index);
}

auto ParamDefineTable::getUserParam(size_t index) -> Param& {
    return index >= getNumSystemUserParams() ? getCustomUserParam(index - getNumSystemUserParams()) : getSystemUserParam(index);
}

auto ParamDefineTable::getUserParam(size_t index) const -> const Param& {
    return index >= getNumSystemUserParams() ? getCustomUserParam(index - getNumSystemUserParams()) : getSystemUserParam(index);
}

auto ParamDefineTable::getSystemAssetParam(size_t index) -> Param& {
    return mSystemAssetParams.at(index);
}

auto ParamDefineTable::getSystemAssetParam(size_t index) const -> const Param& {
    return mSystemAssetParams.at(index);
}

auto ParamDefineTable::getCustomAssetParam(size_t index) -> Param& {
    return mCustomAssetParams.at(index);
}

auto ParamDefineTable::getCustomAssetParam(size_t index) const -> const Param& {
    return mCustomAssetParams.at(index);
}


auto ParamDefineTable::getAssetParam(size_t index) -> Param& {
    return index >= getNumSystemAssetParams() ? getCustomAssetParam(index - getNumSystemAssetParams()) : getSystemAssetParam(index);
}

auto ParamDefineTable::getAssetParam(size_t index) const -> const Param& {
    return index >= getNumSystemAssetParams() ? getCustomAssetParam(index - getNumSystemAssetParams()) : getSystemAssetParam(index);
}

auto ParamDefineTable::getTriggerParam(size_t index) -> Param& {
    return mTriggerParams.at(index);
}

auto ParamDefineTable::getTriggerParam(size_t index) const -> const Param& {
    return mTriggerParams.at(index);
}

auto ParamDefineTable::getParamDefine(const Param& param, ParamDefineType type) -> const Param& {
    switch (type) {
        case ParamDefineType::User:
            for (const auto& def : mSystemUserParams) {
                if (def.getName() == param.getName()) {
                    return def;
                }
            }
            for (const auto& def : mCustomUserParams) {
                if (def.getName() == param.getName()) {
                    return def;
                }
            }
            break;
        case ParamDefineType::Asset:
            for (const auto& def : mSystemAssetParams) {
                if (def.getName() == param.getName()) {
                    return def;
                }
            }
            for (const auto& def : mCustomAssetParams) {
                if (def.getName() == param.getName()) {
                    return def;
                }
            }
            break;
        case ParamDefineType::Trigger:
            for (const auto& def : mTriggerParams) {
                if (def.getName() == param.getName()) {
                    return def;
                }
            }
            break;
    }

    common::AbortWithDetail("Could not find matching param define for {} (type {:#x})", param.getName(), std::to_underlying(type));
}

} // namespace mango