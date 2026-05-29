#include "trigger/always.hpp"

namespace mango {

auto AlwaysTrigger::setAssetCallTable(AssetCallTableHandle act) -> void {
    mAssetCallTable = std::move(act);
}

auto AlwaysTrigger::addOverwriteParam() -> Param& {
    return mTriggerOverwriteParam.emplace_back();
}

} // namespace mango