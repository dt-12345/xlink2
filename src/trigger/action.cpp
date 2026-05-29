#include "trigger/action.hpp"

namespace mango {

auto ActionTrigger::setAssetCallTable(AssetCallTableHandle act) -> void {
    mAssetCallTable = std::move(act);
}

auto ActionTrigger::addOverwriteParam() -> Param& {
    return mTriggerOverwriteParam.emplace_back();
}

} // namespace mango