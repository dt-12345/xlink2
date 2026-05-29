#pragma once

#include "common/bitflag.hpp"
#include "db/version.hpp"

namespace xlink2 {

template <mango::Game GAME>
struct ResActionSlot {
    mango::PtrT<GAME> nameOffset;
    std::int16_t actionStartIndex;
    std::int16_t actionEndIndex;
};
static_assert(sizeof(ResActionSlot<mango::Game::Park>) == 8);
static_assert(sizeof(ResActionSlot<mango::Game::EXKing>) == 0x10);

template <mango::Game GAME>
struct ResAction;

template <mango::Game GAME> requires (mango::GetResActionLayout(GAME) == 0)
struct ResAction<GAME> {
    mango::PtrT<GAME> nameOffset;
    std::int32_t triggerStartIndex;
    std::int32_t triggerEndIndex;
};

template <mango::Game GAME> requires (mango::GetResActionLayout(GAME) == 1)
struct ResAction<GAME> {
    mango::PtrT<GAME> nameOffset;
    std::int16_t triggerStartIndex;
    bool isPrefix;
    std::int32_t triggerEndIndex;
};
static_assert(sizeof(ResAction<mango::Game::Park>) == 0xc);
static_assert(sizeof(ResAction<mango::Game::EXKing>) == 0x10);

enum class ActionTriggerType {
    OneShot     = 0,
    Always      = 2,
    OnLeave     = 3,
    Previous    = 4,
};

template <mango::Game GAME>
struct ResActionTrigger;

template <mango::Game GAME> requires (mango::GetResActionTriggerLayout(GAME) == 0)
struct ResActionTrigger<GAME> {
    std::uint32_t guid;
    mango::PtrT<GAME> assetCallTableOffset;
    union {
        mango::PtrT<GAME> previousActionNameOffset;
        std::int32_t startFrame;
    };
    std::int32_t endFrame;
    common::BitFlag<ActionTriggerType, std::uint16_t> flags;
    std::uint16_t unknown2;
    mango::PtrT<GAME> triggerOverwriteParamOffset;
};

template <mango::Game GAME> requires (mango::GetResActionTriggerLayout(GAME) == 1)
struct ResActionTrigger<GAME> {
    std::uint32_t guid;
    std::int32_t unknown1;
    mango::PtrT<GAME> assetCallTableOffset;
    union {
        mango::PtrT<GAME> previousActionNameOffset;
        std::int32_t startFrame;
    };
    std::int32_t endFrame;
    common::BitFlag<ActionTriggerType, std::uint16_t> flags;
    std::uint16_t unknown2;
    mango::PtrT<GAME> triggerOverwriteParamOffset;
};
static_assert(sizeof(ResActionTrigger<mango::Game::Park>) == 0x18);
static_assert(sizeof(ResActionTrigger<mango::Game::EXKing>) == 0x28);

} // namespace xlink2