#include "asset/asset.hpp"
#include "asset/handle.hpp"

namespace mango {

AssetCallTableHandle::AssetCallTableHandle() : mAssetCallTable(std::make_shared<Asset>()) {}

AssetCallTableHandle::AssetCallTableHandle(std::shared_ptr<AssetCallTable> act) : mAssetCallTable(std::move(act)) {}

AssetCallTableHandle::AssetCallTableHandle(const AssetCallTableHandle&) = default;

AssetCallTableHandle::AssetCallTableHandle(AssetCallTableHandle&& lhs) = default;

AssetCallTableHandle::~AssetCallTableHandle() = default;

auto AssetCallTableHandle::operator=(const AssetCallTableHandle& lhs) -> AssetCallTableHandle& = default;

auto AssetCallTableHandle::operator=(AssetCallTableHandle&& lhs) -> AssetCallTableHandle& = default;

auto AssetCallTableHandle::set(std::shared_ptr<AssetCallTable> act) -> void {
    mAssetCallTable = std::move(act);
}

} // namespace mango