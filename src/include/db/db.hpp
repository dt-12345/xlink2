#pragma once

#include "db/version.hpp"

#include <memory>
#include <span>
#include <vector>

namespace mango {

class ParamDefineTable;
class User;
class UsernameDatabase;

struct LoadContext;

struct ParseDataTag {};

class Database {
public:
    Database();

    Database(Database&& lhs);
    auto operator=(Database&& lhs) -> Database&;

    Database(const Database&) = delete;
    auto operator=(const Database&) -> Database& = delete;

    ~Database();

    auto addUser(std::unique_ptr<User> user) -> void;
    auto setModuleType(ModuleType type) -> void { mModuleType = type; }

    [[nodiscard]] auto getParamDefineTable() -> ParamDefineTable& { return *mParamDefineTable; }
    [[nodiscard]] auto getParamDefineTable() const -> const ParamDefineTable& { return *mParamDefineTable; }
    [[nodiscard]] auto getUsers() -> std::vector<std::unique_ptr<User>>& { return mUsers; }
    [[nodiscard]] auto getUsers() const -> const std::vector<std::unique_ptr<User>>& { return mUsers; }
    [[nodiscard]] auto getModuleType() const -> ModuleType { return mModuleType; }

    auto load(std::span<const std::uint8_t> data, Game game, Platform platform = Platform::NX) -> void;
    auto load(std::string_view path, Game game, Platform platform = Platform::NX) -> void;

    [[nodiscard]] auto save(Game game, Platform platform = Platform::NX) const -> std::vector<std::uint8_t>;
    auto save(std::string_view path, Game game, Platform platform = Platform::NX) const -> void;

    auto parse(std::span<const char> text, ParseDataTag tag) -> void;
    auto parse(std::string_view path) -> void;

    [[nodiscard]] auto text(bool useBraces = true) const -> std::string;
    auto text(std::string_view path, bool useBraces = true) const -> void;

    auto reset() -> void;

    auto resolveUsernames(const UsernameDatabase& names, bool overwriteOld = false) -> void;

private:
    std::unique_ptr<ParamDefineTable> mParamDefineTable;
    std::vector<std::unique_ptr<User>> mUsers;
    ModuleType mModuleType;
};

} // namespace mango