#include "common/error.hpp"
#include "common/hash.hpp"
#include "db/user.hpp"

#include <utility>

namespace mango {

auto User::setHash(std::uint32_t hash) -> void {
    mKey = hash;
}

auto User::setName(std::string_view name) -> void {
    mKey = std::string(name);
}

auto User::getHash() const -> std::uint32_t {
    switch (mKey.index()) {
        case 0:
            return std::get<0>(mKey);
        case 1:
            return common::CalcCRC32(std::get<1>(mKey));
        default:
            std::unreachable();
    }
}

auto User::getName() const -> std::string_view {
    if (mKey.index() == 1) {
        return std::get<1>(mKey);
    }

    common::AbortWithDetail("User has no known name! {:#x}", std::get<0>(mKey));
}

} // namespace mango