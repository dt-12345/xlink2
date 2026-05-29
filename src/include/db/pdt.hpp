#pragma once

#include "param/param.hpp"

#include <vector>

namespace mango {

struct LoadContext;

enum class ParamDefineType {
    User,
    Asset,
    Trigger,
};

class ParamDefineTable {
public:
    ParamDefineTable() = default;

    auto reset() -> void;

    auto load(LoadContext& ctx) -> void;

    auto addSystemUserParam() -> Param&;
    auto addCustomUserParam() -> Param&;
    auto addSystemAssetParam() -> Param&;
    auto addCustomAssetParam() -> Param&;
    auto addTriggerParam() -> Param&;

    [[nodiscard]] auto getSystemUserParams() -> std::vector<Param>& { return mSystemUserParams; }
    [[nodiscard]] auto getSystemUserParams() const -> const std::vector<Param>& { return mSystemUserParams; }
    [[nodiscard]] auto getCustomUserParams() -> std::vector<Param>& { return mCustomUserParams; }
    [[nodiscard]] auto getCustomUserParams() const -> const std::vector<Param>& { return mCustomUserParams; }
    [[nodiscard]] auto getSystemAssetParams() -> std::vector<Param>& { return mSystemAssetParams; }
    [[nodiscard]] auto getSystemAssetParams() const -> const std::vector<Param>& { return mSystemAssetParams; }
    [[nodiscard]] auto getCustomAssetParams() -> std::vector<Param>& { return mCustomAssetParams; }
    [[nodiscard]] auto getCustomAssetParams() const -> const std::vector<Param>& { return mCustomAssetParams; }
    [[nodiscard]] auto getTriggerParams() -> std::vector<Param>& { return mTriggerParams; }
    [[nodiscard]] auto getTriggerParams() const -> const std::vector<Param>& { return mTriggerParams; }

    [[nodiscard]] auto getSystemUserParam(size_t index) -> Param&;
    [[nodiscard]] auto getSystemUserParam(size_t index) const -> const Param&;
    [[nodiscard]] auto getCustomUserParam(size_t index) -> Param&;
    [[nodiscard]] auto getCustomUserParam(size_t index) const -> const Param&;
    [[nodiscard]] auto getUserParam(size_t index) -> Param&;
    [[nodiscard]] auto getUserParam(size_t index) const -> const Param&;
    [[nodiscard]] auto getSystemAssetParam(size_t index) -> Param&;
    [[nodiscard]] auto getSystemAssetParam(size_t index) const -> const Param&;
    [[nodiscard]] auto getCustomAssetParam(size_t index) -> Param&;
    [[nodiscard]] auto getCustomAssetParam(size_t index) const -> const Param&;
    [[nodiscard]] auto getAssetParam(size_t index) -> Param&;
    [[nodiscard]] auto getAssetParam(size_t index) const -> const Param&;
    [[nodiscard]] auto getTriggerParam(size_t index) -> Param&;
    [[nodiscard]] auto getTriggerParam(size_t index) const -> const Param&;

    [[nodiscard]] auto getNumSystemUserParams() const -> std::size_t { return mSystemUserParams.size(); }
    [[nodiscard]] auto getNumCustomUserParams() const -> std::size_t { return mCustomUserParams.size(); }
    [[nodiscard]] auto getNumUserParams() const -> std::size_t { return getNumSystemUserParams() + getNumCustomUserParams(); }
    [[nodiscard]] auto getNumSystemAssetParams() const -> std::size_t { return mSystemAssetParams.size(); }
    [[nodiscard]] auto getNumCustomAssetParams() const -> std::size_t { return mCustomAssetParams.size(); }
    [[nodiscard]] auto getNumAssetParams() const -> std::size_t { return getNumSystemAssetParams() + getNumCustomAssetParams(); }
    [[nodiscard]] auto getNumTriggerParams() const -> std::size_t { return mTriggerParams.size(); }

    [[nodiscard]] auto getParamDefine(const Param& param, ParamDefineType type) -> const Param&;

    [[nodiscard]] static auto IsValidParamDefineValueType(ValueType vt) -> bool {
        return vt == ValueType::S32 || vt == ValueType::F32 || vt == ValueType::Bool || vt == ValueType::Enum || vt == ValueType::String || vt == ValueType::ArrangeParam;
    }

private:
    std::vector<Param> mSystemUserParams    = {};
    std::vector<Param> mCustomUserParams    = {};
    std::vector<Param> mSystemAssetParams   = {};
    std::vector<Param> mCustomAssetParams   = {};
    std::vector<Param> mTriggerParams       = {};
};

} // namespace mango