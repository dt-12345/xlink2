#include "common/error.hpp"
#include "common/file.hpp"
#include "db/db.hpp"
#include "db/pdt.hpp"
#include "db/user.hpp"
#include "db/username.hpp"

namespace mango {

Database::Database() : mParamDefineTable(std::make_unique<ParamDefineTable>()), mUsers() {}

Database::Database(Database&& lhs) = default;

auto Database::operator=(Database&& lhs) -> Database& = default;

Database::~Database() = default;

auto Database::addUser(std::unique_ptr<User> user) -> void {
    if (user) {
        mUsers.emplace_back(std::move(user));
    }
}

auto Database::load(std::string_view path, Game game, Platform platform) -> void {
    const auto data = common::ReadFile(path);
    if (data.empty()) {
        common::AbortWithDetail("File not found or empty: {}", path);
    }

    load(data, game, platform);
}

auto Database::save(std::string_view path, Game game, Platform platform) const -> void {
    const auto data = save(game, platform);
    common::WriteFile(path, std::span{ reinterpret_cast<const std::uint8_t*>(data.data()), data.size() });
}

auto Database::parse(std::string_view path) -> void {
    const auto data = common::ReadFile(path);
    if (data.empty()) {
        common::AbortWithDetail("File not found or empty: {}", path);
    }

    parse(std::span{ reinterpret_cast<const char*>(data.data()), data.size() }, ParseDataTag{});
}

auto Database::text(std::string_view path) const -> void {
    const auto data = text();
    common::WriteFile(path, std::span{ reinterpret_cast<const std::uint8_t*>(data.data()), data.size() });
}

auto Database::reset() -> void {
    mParamDefineTable->reset();
    mUsers.clear();
}

auto Database::resolveUsernames(const UsernameDatabase& names, bool overwriteOld) -> void {
    for (auto& user : mUsers) {
        if (!overwriteOld && user->hasKnownName()) {
            continue;
        }
        const auto res = names.query(user->getHash());
        if (res) {
            user->setName(*res);
        }
    }
}

} // namespace mango