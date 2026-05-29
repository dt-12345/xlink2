#include "trigger/property.hpp"

namespace mango {

auto PropertyTrigger::setCondition(SwitchCondition&& condition) -> void {
    mCondition = std::move(condition);
}

auto PropertyTrigger::setCondition(CompareType type, SwitchCondition::ValueType&& value) -> void {
    mCondition.setCompareType(type);
    mCondition.setValue(std::move(value));
}

auto PropertyTrigger::setAssetCallTable(AssetCallTableHandle act) -> void {
    mAssetCallTable = std::move(act);
}

auto PropertyTrigger::addOverwriteParam() -> Param& {
    return mTriggerOverwriteParam.emplace_back();
}


} // namespace mango