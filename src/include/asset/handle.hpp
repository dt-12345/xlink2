#pragma once

#include <memory>

namespace mango {

class AssetCallTable;

class AssetCallTableHandle {
public:
    AssetCallTableHandle();
    AssetCallTableHandle(std::shared_ptr<AssetCallTable> act);
    AssetCallTableHandle(const AssetCallTableHandle&);
    AssetCallTableHandle(AssetCallTableHandle&& lhs);
    ~AssetCallTableHandle();

    auto operator=(const AssetCallTableHandle& lhs) -> AssetCallTableHandle&;
    auto operator=(AssetCallTableHandle&& lhs) -> AssetCallTableHandle&;

    auto set(std::shared_ptr<AssetCallTable> act) -> void;
    [[nodiscard]] constexpr auto get() const noexcept -> AssetCallTable* { return mAssetCallTable.get(); }

    [[nodiscard]] constexpr auto operator->() const noexcept -> AssetCallTable* { return mAssetCallTable.get(); }
    [[nodiscard]] constexpr auto operator*() const noexcept -> AssetCallTable& { return *get(); }

    [[nodiscard]] constexpr operator bool() const noexcept { return get() != nullptr; }

    [[nodiscard]] constexpr auto getRefCount() const noexcept -> std::size_t { return mAssetCallTable.use_count(); }

private:
    std::shared_ptr<AssetCallTable> mAssetCallTable;
};

} // namespace mango