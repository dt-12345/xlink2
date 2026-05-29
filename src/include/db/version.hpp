#pragma once

#include <bit>
#include <cstdint>
#include <utility>

namespace mango {

enum class Platform {
    Cafe,
    NX,
    Ounce,
};

enum class ModuleType {
    ELink,
    SLink,
};

enum class Game {
    UKing,      // TLoZ BotW
    Tandem,     // 12S
    TurboS,     // MK8D
    ARMS,       // Arms
    Blitz,      // S2
    Babel,      // Labo 01 Variety Kit
                // Labo 02 Robot Kit
                // Labo 03 Drive Kit
                // Labo 04 VR Kit
                // GBG
    Slope,      // SMM2
    Billy,      // RFA
    Park,       // ACNH
    Stardust,   // SM 3DAS
    Elsa,       // NSS
    Thunder,    // S3
    EXKing,     // TLoZ TotK
    Secred,     // SMBW
    GrandeS,    // MvDK
    Rockstock,  // NSO PP
    Colony,     // TLLtD
};

[[nodiscard]] constexpr auto operator<(const Game& lhs, const Game& rhs) -> bool {
    return std::to_underlying(lhs) < std::to_underlying(rhs);
}

[[nodiscard]] constexpr auto GetEndianness(Platform platform) -> std::endian {
    switch (platform) {
        case Platform::Cafe: return std::endian::big;
        case Platform::NX: return std::endian::little;
        case Platform::Ounce: return std::endian::little;
        default: std::unreachable();
    }
}

[[nodiscard]] constexpr auto GetVersionSLink(Game game) -> std::uint32_t {
    switch (game) {
        case Game::UKing: return 0x1cu;
        case Game::Tandem: return 0x1cu;
        case Game::TurboS: return 0x1cu;
        case Game::ARMS: return 0x1cu;
        case Game::Blitz: return 0x1cu;
        case Game::Babel: return 0x1cu;
        case Game::Slope: return 0x1cu;
        case Game::Billy: return 0x1cu;
        case Game::Park: return 0x1du;
        case Game::Stardust: return 0x1fu;
        case Game::Elsa: return 0x1fu;
        case Game::Thunder: return 0x1fu;
        case Game::EXKing: return 0x21u;
        case Game::Secred: return 0x21u;
        case Game::GrandeS: return 0x21u;
        case Game::Rockstock: return 0x22u;
        case Game::Colony: return 0x22u;
        default: std::unreachable();
    }
}

[[nodiscard]] constexpr auto GetVersionELink(Game game) -> std::uint32_t {
    switch (game) {
        case Game::UKing: return 0x1eu;
        case Game::Tandem: return 0x1eu;
        case Game::TurboS: return 0x1eu;
        case Game::ARMS: return 0x1eu;
        case Game::Blitz: return 0x1eu;
        case Game::Babel: return 0x1eu;
        case Game::Slope: return 0x1eu;
        case Game::Billy: return 0x1eu;
        case Game::Park: return 0x1fu;
        case Game::Stardust: return 0x22u;
        case Game::Elsa: return 0x22u;
        case Game::Thunder: return 0x22u;
        case Game::EXKing: return 0x24u;
        case Game::Secred: return 0x24u;
        case Game::GrandeS: return 0x24u;
        case Game::Rockstock: return 0x24u;
        case Game::Colony: return 0x24u;
        default: std::unreachable();
    }
}

[[nodiscard]] constexpr auto GetNumSystemUserParamsSLink(Game game [[maybe_unused]]) -> std::uint32_t {
    return 8u;
}

[[nodiscard]] constexpr auto GetNumSystemUserParamsELink(Game game [[maybe_unused]]) -> std::uint32_t {
    return 0u;
}

[[nodiscard]] constexpr auto Is64Bit(Game game) -> bool {
    return game >= Game::Stardust;
}

[[nodiscard]] constexpr auto GetResContainerParamLayout(Game game) -> std::uint32_t {
    return game >= Game::Stardust ? 1u : 0u;
}

[[nodiscard]] constexpr auto GetResUserHeaderLayout(Game game) -> std::uint32_t {
    return game >= Game::EXKing ? 1u : 0u;
}

[[nodiscard]] constexpr auto GetResActionLayout(Game game) -> std::uint32_t {
    return game >= Game::Stardust ? 1u : 0u;
}

[[nodiscard]] constexpr auto GetResActionTriggerLayout(Game game) -> std::uint32_t {
    return game >= Game::Stardust ? 1u : 0u;
}

[[nodiscard]] constexpr auto GetResPropertyTriggerLayout(Game game) -> std::uint32_t {
    return game >= Game::Stardust ? 1u : 0u;
}

[[nodiscard]] constexpr auto GetResAlwaysTriggerLayout(Game game) -> std::uint32_t {
    return game >= Game::Stardust ? 1u : 0u;
}

[[nodiscard]] constexpr auto GetResSwitchConditionLayout(Game game) -> std::uint32_t {
    if (game >= Game::Stardust) return 2u;
    if (game >= Game::Park) return 1u;
    return 0u;
}

[[nodiscard]] constexpr auto SupportsActionSwitch(Game game) -> bool {
    return game >= Game::Stardust;
}

[[nodiscard]] constexpr auto SupportsGridContainers(Game game) -> bool {
    return game >= Game::Stardust;
}

[[nodiscard]] constexpr auto SupportsJumpContainers(Game game) -> bool {
    return game >= Game::EXKing;
}

template <template <Game GAME> typename T>
[[nodiscard]] constexpr auto SizeOf(Game game) -> std::size_t {
    switch (game) {
        case Game::UKing: return sizeof(T<Game::UKing>);
        case Game::Tandem: return sizeof(T<Game::Tandem>);
        case Game::TurboS: return sizeof(T<Game::TurboS>);
        case Game::ARMS: return sizeof(T<Game::ARMS>);
        case Game::Blitz: return sizeof(T<Game::Blitz>);
        case Game::Babel: return sizeof(T<Game::Babel>);
        case Game::Slope: return sizeof(T<Game::Slope>);
        case Game::Billy: return sizeof(T<Game::Billy>);
        case Game::Park: return sizeof(T<Game::Park>);
        case Game::Stardust: return sizeof(T<Game::Stardust>);
        case Game::Elsa: return sizeof(T<Game::Elsa>);
        case Game::Thunder: return sizeof(T<Game::Thunder>);
        case Game::EXKing: return sizeof(T<Game::EXKing>);
        case Game::Secred: return sizeof(T<Game::Secred>);
        case Game::GrandeS: return sizeof(T<Game::GrandeS>);
        case Game::Rockstock: return sizeof(T<Game::Rockstock>);
        case Game::Colony: return sizeof(T<Game::Colony>);
        default: std::unreachable();
    }
}

namespace detail {

template <Game GAME>
struct PtrHelper;

template <Game GAME> requires (!Is64Bit(GAME))
struct PtrHelper<GAME> {
    using T = std::uint32_t;
};

template <Game GAME> requires (Is64Bit(GAME))
struct PtrHelper<GAME> {
    using T = std::uint64_t;
};

} // namespace detail

template <Game GAME>
using PtrT = detail::PtrHelper<GAME>::T;

} // namespace mango