#include "asset/asset.hpp"
#include "asset/blend.hpp"
#include "asset/grid.hpp"
#include "asset/sequence.hpp"
#include "asset/switch.hpp"
#include "asset/handle.hpp"
#include "asset/switch.hpp"
#include "common/error.hpp"
#include "common/hash.hpp"
#include "common/writer.hpp"
#include "condition/blend.hpp"
#include "condition/random.hpp"
#include "condition/sequence.hpp"
#include "condition/switch.hpp"
#include "db/db.hpp"
#include "db/pdt.hpp"
#include "db/user.hpp"
#include "db/version.hpp"
#include "param/arrange.hpp"
#include "param/param.hpp"
#include "property/common.hpp"
#include "res/act.hpp"
#include "res/action.hpp"
#include "res/always.hpp"
#include "res/container.hpp"
#include "res/condition.hpp"
#include "res/header.hpp"
#include "res/param.hpp"
#include "res/pdt.hpp"
#include "res/property.hpp"
#include "res/user.hpp"
#include "trigger/action.hpp"
#include "trigger/always.hpp"

#include <map>
#include <queue>
#include <ranges>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

/*

File Section Order:
- Header
- User Hashes
- User Offsets
- Param Define Table (aligned ptr)
  - Header
  - User Param Defines
  - Asset Param Defines
  - Trigger Param Defines
  - PDT Name Table
- Asset Params (aligned ptr)
- Trigger Overwrite Params
- Local Property Name Refs
- Local Property Enum Name Refs
- Direct Value Table
- Random Call Table
- Curve Call Table
- Curve Point Table
- ExRegion (unaligned)
  - ArrangeGroupParams
- Users
  - Header
  - Local Properties
  - User Params
  - Sorted Asset Id Table
  - Asset Call Tables (aligned 4)
  - Containers
  - Action Slots (trigger table offset refers to this point)
  - Actions
  - Action Triggers
  - Properties
  - Property Triggers
  - Always Triggers
- Condition Table
- Name Table

a byte-perfect serialization is not happening unless we export everything from the file basically as-is which makes editing a huge pain (basically the old version)
since we're not getting a byte-perfect serialization, we might as well take some liberties, as long as the file remains valid

I suspect there are users that are removed from release builds that cause the weird behavior we tend to see here (unused values, weird ordering, etc.)
(it could also be by user declaration order in the original database which we have no way of recovering)

de-duplicated:
- asset params (sorted by first use?)
- trigger overwrite params (sorted by first use?)
- local property names (sorted lexicographically)
- local property enum names (sorted lexicographically)
- direct values
    - sorted by first use (for each user: user params -> asset params -> overwrite params)
    - deduplicated by type and value (we can just do value)
- random
    - probably the same as direct values
- curves
    - probably the same as direct values
- arrange params (sorted by first use)
- conditions
    - nintendo seems to deduplicate conditions by parent call table but that's very inefficient
    - we can do way better and it's probably easier than comparing the stupid call graphs

not deduplicated:
- curve point (sorted by curve order)

*/

namespace mango::detail {

// taken from boost which is based on mmh3
[[nodiscard]] static auto Mix(std::size_t x) -> std::size_t {
    x ^= x >> 32;
    x *= 0xe9846af9b1a615dull;
    x ^= x >> 32;
    x *= 0xe9846af9b1a615dull;
    x ^= x >> 28;
    return x;
}

[[nodiscard]] static auto Combine(std::size_t h1, std::size_t h2) -> std::size_t {
    return Mix(h1 + 0x9e3779b9ull + h2);
}

} // namespace mango::detail

namespace std {

template <>
struct hash<reference_wrapper<const mango::RandomTable>> {
    auto operator()(const reference_wrapper<const mango::RandomTable>& value) const -> size_t {
        auto h = size_t(0);
        h = mango::detail::Combine(h, hash<mango::RandomType>{}(value.get().getType()));
        if (value.get().getType() != mango::RandomType::Linear) {
            h = mango::detail::Combine(h, hash<float>{}(value.get().getPower()));
        }
        h = mango::detail::Combine(h, hash<float>{}(value.get().getMin()));
        h = mango::detail::Combine(h, hash<float>{}(value.get().getMax()));
        return h;
    }
};

template <>
struct hash<reference_wrapper<const mango::Curve>> {
    auto operator()(const reference_wrapper<const mango::Curve>& value) const -> size_t {
        auto h = size_t(0);
        h = mango::detail::Combine(h, hash<mango::PropertyScope>{}(value.get().getPropertyScope()));
        h = mango::detail::Combine(h, hash<string_view>{}(value.get().getPropertyName()));
        h = mango::detail::Combine(h, hash<mango::CurveType>{}(value.get().getType()));
        h = mango::detail::Combine(h, hash<int32_t>{}(value.get().getUnknown1()));
        h = mango::detail::Combine(h, hash<int16_t>{}(value.get().getUnknown2()));
        for (const auto& point : value.get().getPoints()) {
            h = mango::detail::Combine(h, hash<float>{}(point.x));
            h = mango::detail::Combine(h, hash<float>{}(point.y));
        }
        return h;
    }
};

template <>
struct hash<reference_wrapper<const mango::ArrangeParam>> {
    auto operator()(const reference_wrapper<const mango::ArrangeParam>& value) const -> size_t {
        auto h = size_t(0);
        for (const auto& group : value.get()) {
            h = mango::detail::Combine(h, hash<string_view>{}(group.getName()));
            h = mango::detail::Combine(h, hash<mango::LimitType>{}(group.getLimitType()));
            h = mango::detail::Combine(h, hash<int8_t>{}(group.getThreshold()));
            h = mango::detail::Combine(h, hash<bool>{}(group.getIncludeFading()));
        }
        return h;
    }
};

template <>
struct hash<reference_wrapper<const vector<mango::Param>>> {
    auto operator()(const reference_wrapper<const vector<mango::Param>>& value) const -> size_t {
        auto h = std::size_t(0);
        for (const auto& param : value.get()) {
            h = mango::detail::Combine(h, hash<string_view>{}(param.getName()));
            h = mango::detail::Combine(h, hash<mango::ParamType>{}(param.getParamType()));
            switch (param.getValueType()) {
                case mango::ValueType::S32:
                    h = mango::detail::Combine(h, hash<int32_t>{}(param.getValue<mango::ValueType::S32>()));
                    break;
                case mango::ValueType::Bitfield:
                    h = mango::detail::Combine(h, hash<uint32_t>{}(param.getValue<mango::ValueType::Bitfield>()));
                    break;
                case mango::ValueType::F32:
                    h = mango::detail::Combine(h, hash<float>{}(param.getValue<mango::ValueType::F32>()));
                    break;
                case mango::ValueType::Curve:
                    h = mango::detail::Combine(h, hash<reference_wrapper<const mango::Curve>>{}(std::cref(*param.getValue<mango::ValueType::Curve>())));
                    break;
                case mango::ValueType::Random:
                    h = mango::detail::Combine(h, hash<reference_wrapper<const mango::RandomTable>>{}(std::cref(*param.getValue<mango::ValueType::Random>())));
                    break;
                case mango::ValueType::Bool:
                    h = mango::detail::Combine(h, hash<bool>{}(param.getValue<mango::ValueType::Bool>()));
                    break;
                case mango::ValueType::Enum:
                    h = mango::detail::Combine(h, hash<uint32_t>{}(param.getValue<mango::ValueType::Enum>()));
                    break;
                case mango::ValueType::String:
                    h = mango::detail::Combine(h, hash<string>{}(*param.getValue<mango::ValueType::String>()));
                    break;
                case mango::ValueType::ArrangeParam:
                    h = mango::detail::Combine(h, hash<reference_wrapper<const mango::ArrangeParam>>{}(std::cref(*param.getValue<mango::ValueType::ArrangeParam>())));
                    break;
                default:
                    std::unreachable();
            }
        }
        return h;
    }
};

template <>
struct hash<reference_wrapper<const mango::ICondition>> {
    auto operator()(const reference_wrapper<const mango::ICondition>& value) const -> size_t {
        auto h = std::size_t(0);
        h = mango::detail::Combine(h, hash<mango::ConditionType>{}(value.get().getType()));
        switch (value.get().getType()) {
            case mango::ConditionType::Switch: {
                // not sure if sharing isSolved will cause issues, but I think it should be fine?
                const auto& switch_ = static_cast<const mango::SwitchCondition&>(value.get());
                h = mango::detail::Combine(h, hash<mango::SwitchType>{}(switch_.getParentType()));
                h = mango::detail::Combine(h, hash<mango::CompareType>{}(switch_.getCompareType()));
                switch (switch_.getValue().index()) {
                    case 0:
                        h = mango::detail::Combine(h, hash<string>{}(std::get<0>(switch_.getValue())));
                        break;
                    case 1:
                        h = mango::detail::Combine(h, hash<int32_t>{}(std::get<1>(switch_.getValue())));
                        break;
                    case 2:
                        h = mango::detail::Combine(h, hash<float>{}(std::get<2>(switch_.getValue())));
                        break;
                    case 3:
                        h = mango::detail::Combine(h, hash<bool>{}(std::get<3>(switch_.getValue())));
                        break;
                    default:
                        std::unreachable();
                }
                break;
            }
            case mango::ConditionType::Random:
                h = mango::detail::Combine(h, hash<float>{}(static_cast<const mango::RandomCondition&>(value.get()).getWeight()));
                break;
            case mango::ConditionType::RandomNoRepeat:
                h = mango::detail::Combine(h, hash<float>{}(static_cast<const mango::RandomNoRepeatCondition&>(value.get()).getWeight()));
            case mango::ConditionType::Blend:
                break;
            case mango::ConditionType::BlendBy: {
                const auto& blend = static_cast<const mango::BlendByCondition&>(value.get());
                h = mango::detail::Combine(h, hash<float>{}(blend.getValueMin()));
                h = mango::detail::Combine(h, hash<float>{}(blend.getValueMax()));
                h = mango::detail::Combine(h, hash<mango::BlendOp>{}(blend.getBlendOpMin()));
                h = mango::detail::Combine(h, hash<mango::BlendOp>{}(blend.getBlendOpMax()));
                break;
            }
            case mango::ConditionType::Sequence:
                h = mango::detail::Combine(h, hash<std::uint32_t>{}(static_cast<const mango::SequenceCondition&>(value.get()).getContinueOnFade()));
                break;
            case mango::ConditionType::Grid:
            case mango::ConditionType::Jump:
            default:
                break;
        }
        return h;
    }
};

} // namespace std

namespace mango {

auto operator==(const RandomTable& lhs, const RandomTable& rhs) -> bool {
    if (lhs.getType() != rhs.getType()) {
        return false;
    }

    if (lhs.getType() != RandomType::Linear && lhs.getPower() != rhs.getPower()) {
        return false;
    }

    if (lhs.getMin() != rhs.getMin()) {
        return false;
    }

    return lhs.getMax() == rhs.getMax();
}

auto operator==(const CurvePoint& lhs, const CurvePoint& rhs) -> bool {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

auto operator==(const Curve& lhs, const Curve& rhs) -> bool {
    if (lhs.getPropertyScope() != rhs.getPropertyScope()) {
        return false;
    }

    if (lhs.getPropertyName() != rhs.getPropertyName()) {
        return false;
    }

    if (lhs.getType() != rhs.getType()) {
        return false;
    }

    if (lhs.getUnknown1() != rhs.getUnknown1()) {
        return false;
    }

    if (lhs.getUnknown2() != rhs.getUnknown2()) {
        return false;
    }

    return lhs.getPoints() == rhs.getPoints();
}

auto operator==(const ArrangeGroup& lhs, const ArrangeGroup& rhs) -> bool {
    return lhs.getName() == rhs.getName()
        && lhs.getLimitType() == rhs.getLimitType()
        && lhs.getThreshold() == rhs.getThreshold()
        && lhs.getIncludeFading() == rhs.getIncludeFading();
}

auto operator==(const Param& lhs, const Param& rhs) -> bool {
    if (lhs.getName() != rhs.getName()) {
        return false;
    }

    if (lhs.getParamType() != rhs.getParamType()) {
        return false;
    }

    if (lhs.getValueType() != rhs.getValueType()) {
        return false;
    }

    switch (lhs.getValueType()) {
        case ValueType::S32:
            return lhs.getValue<ValueType::S32>() == rhs.getValue<ValueType::S32>();
        case ValueType::Bitfield:
            return lhs.getValue<ValueType::Bitfield>() == rhs.getValue<ValueType::Bitfield>();
        case ValueType::F32:
            return lhs.getValue<ValueType::F32>() == rhs.getValue<ValueType::F32>();
        case ValueType::Curve:
            return *lhs.getValue<ValueType::Curve>() == *rhs.getValue<ValueType::Curve>();
        case ValueType::Random:
            return *lhs.getValue<ValueType::Random>() == *rhs.getValue<ValueType::Random>();
        case ValueType::Bool:
            return lhs.getValue<ValueType::Bool>() == rhs.getValue<ValueType::Bool>();
        case ValueType::Enum:
            return lhs.getValue<ValueType::Enum>() == rhs.getValue<ValueType::Enum>();
        case ValueType::String:
            return *lhs.getValue<ValueType::String>() == *rhs.getValue<ValueType::String>();
        case ValueType::ArrangeParam:
            return *lhs.getValue<ValueType::ArrangeParam>() == *rhs.getValue<ValueType::ArrangeParam>();
        default:
            std::unreachable();
    }
}

auto operator==(const ICondition& lhs, const ICondition& rhs) -> bool {
    if (lhs.getType() != rhs.getType()) {
        return false;
    }

    switch (lhs.getType()) {
        case ConditionType::Switch: {
            // not sure if sharing isSolved will cause issues, but I think it should be fine?
            const auto& condA = static_cast<const SwitchCondition&>(lhs);
            const auto& condB = static_cast<const SwitchCondition&>(rhs);
            
            if (condA.getParentType() != condB.getParentType()) {
                return false;
            }

            if (condA.getCompareType() != condB.getCompareType()) {
                return false;
            }
            
            if (condA.getValue().index() != condB.getValue().index()) {
                return false;
            }

            switch (condA.getValue().index()) {
                case 0:
                    return std::get<0>(condA.getValue()) == std::get<0>(condB.getValue());
                case 1:
                    return std::get<1>(condA.getValue()) == std::get<1>(condB.getValue());
                case 2:
                    return std::get<2>(condA.getValue()) == std::get<2>(condB.getValue());
                case 3:
                    return std::get<3>(condA.getValue()) == std::get<3>(condB.getValue());
                default:
                    std::unreachable();
            }
        }
        case ConditionType::Random:
            return static_cast<const RandomCondition&>(lhs).getWeight() == static_cast<const RandomCondition&>(rhs).getWeight();
        case ConditionType::RandomNoRepeat:
            return static_cast<const RandomNoRepeatCondition&>(lhs).getWeight() == static_cast<const RandomNoRepeatCondition&>(rhs).getWeight();
        case ConditionType::Blend:
            return true; // equal
        case ConditionType::BlendBy: {
            const auto& condA = static_cast<const BlendByCondition&>(lhs);
            const auto& condB = static_cast<const BlendByCondition&>(rhs);
            return condA.getBlendOpMin() == condB.getBlendOpMin() && condA.getBlendOpMax() == condB.getBlendOpMax()
                && condA.getValueMin() == condB.getValueMin() && condA.getValueMax() == condB.getValueMax();
        }
        case ConditionType::Sequence:
            return static_cast<const SequenceCondition&>(lhs).getContinueOnFade() == static_cast<const SequenceCondition&>(rhs).getContinueOnFade();
        case ConditionType::Grid:
        case ConditionType::Jump:
            return true;
        default:
            return true;
    }
}

struct CallTableInfo {
    std::reference_wrapper<const AssetCallTable> act;
    std::uint64_t containerOrAssetParamOffset;
    std::uint64_t conditionOffset;
};

struct UserContext;

struct PDTContext {
    std::uint64_t strPoolSize = 0ull;
    std::map<std::string_view, std::uint64_t> stringPool = {};

    auto getOrAddString(std::string_view value) -> std::uint64_t {
        const auto res = stringPool.try_emplace(value, strPoolSize);
        if (res.second) {
            strPoolSize += value.size() + 1;
        }
        return res.first->second;
    }
};

struct SaveContext {
    common::BinaryWriter writer;
    Game game;

    PDTContext pdtCtx = {};

    std::uint32_t numResParam = 0ull;
    std::uint64_t currentAssetParamOffset = 0ull;
    std::vector<std::reference_wrapper<const std::vector<Param>>> assetParams = {};
    std::unordered_map<std::reference_wrapper<const std::vector<Param>>, std::uint64_t> assetParamOffsetMap = {};

    std::uint64_t currentTriggerParamOffset = 0ull;
    std::vector<std::reference_wrapper<const std::vector<Param>>> triggerParams = {};
    std::unordered_map<std::reference_wrapper<const std::vector<Param>>, std::uint64_t> triggerParamOffsetMap = {};

    std::map<std::string_view, std::size_t> localPropertyNameTable = {};
    std::map<std::string_view, std::size_t> localPropertyEnumNameTable = {};

    std::vector<std::uint32_t> directValues = {};
    std::unordered_map<std::uint32_t, std::size_t> directValueIndexMap = {};

    std::vector<std::reference_wrapper<const RandomTable>> randomTable = {};
    std::unordered_map<std::reference_wrapper<const RandomTable>, std::size_t> randomIndexMap = {};

    std::uint32_t curvePointCount = 0ull;
    std::vector<std::reference_wrapper<const Curve>> curveTable = {};
    std::unordered_map<std::reference_wrapper<const Curve>, std::size_t> curveIndexMap = {};

    std::uint64_t currentArrangeParamOffset = 0ull;
    std::vector<std::reference_wrapper<const ArrangeParam>> arrangeParams = {};
    std::unordered_map<std::reference_wrapper<const ArrangeParam>, std::uint64_t> arrangeParamOffsetMap = {};

    std::uint64_t totalUserSize = 0ull;
    std::map<std::uint32_t, UserContext> users = {};

    std::uint64_t currentConditionOffset = 0ull;
    std::vector<std::reference_wrapper<const ICondition>> conditions = {};
    std::unordered_map<std::reference_wrapper<const ICondition>, std::uint64_t> conditionOffsetMap = {};

    std::uint64_t strPoolSize = 0ull;
    std::map<std::string_view, std::uint64_t> nameTable = {}; // name : offset (offset should be filled in after all names are registered)

    [[nodiscard]] auto getPtrSize() const -> std::size_t {
        return is64Bit() ? 8 : 4;
    }

    auto writePtr(std::uint64_t value) -> void {
        if (is64Bit()) {
            writer.write<std::uint64_t>(value);
        } else {
            writer.write<std::uint32_t>(static_cast<std::uint32_t>(value));
        }
    }

    [[nodiscard]] auto is64Bit() const -> bool { return Is64Bit(game); }

    auto getOrAddAssetParam(const std::vector<Param>& param) -> std::uint64_t {
        const auto res = assetParamOffsetMap.try_emplace(std::cref(param), currentAssetParamOffset);
        if (res.second) {
            assetParams.emplace_back(std::cref(param));
            currentAssetParamOffset += sizeof(std::uint64_t) + SizeOf<xlink2::ResParam>(game) * param.size();
            numResParam += param.size();
        }
        return res.first->second;
    }

    auto getOrAddTriggerParam(const std::vector<Param>& param) -> std::uint64_t {
        if (param.size() == 0) {
            return 0xffffffffull;
        }
        const auto res = triggerParamOffsetMap.try_emplace(std::cref(param), currentTriggerParamOffset);
        if (res.second) {
            triggerParams.emplace_back(std::cref(param));
            currentTriggerParamOffset += sizeof(std::uint32_t) + SizeOf<xlink2::ResParam>(game) * param.size();
        }
        return res.first->second;
    }

    template <typename T> requires (sizeof(T) <= 4)
    auto getOrAddDirectValue(T value) -> std::size_t {
        if constexpr (std::is_same_v<T, bool>) {
            const auto res = directValueIndexMap.try_emplace(static_cast<std::uint32_t>(value), directValues.size());
            if (res.second) {
                directValues.emplace_back(static_cast<std::uint32_t>(value));
            }
            return res.first->second;
        } else {
            const auto res = directValueIndexMap.try_emplace(std::bit_cast<std::uint32_t>(value), directValues.size());
            if (res.second) {
                directValues.emplace_back(std::bit_cast<std::uint32_t>(value));
            }
            return res.first->second;
        }
    }

    auto getOrAddRandom(const RandomTable& random) -> std::size_t  {
        const auto res = randomIndexMap.try_emplace(std::cref(random), randomTable.size());
        if (res.second) {
            randomTable.emplace_back(std::cref(random));
        }
        return res.first->second;
    }

    auto getOrAddCurve(const Curve& curve) -> std::size_t {
        const auto res = curveIndexMap.try_emplace(std::cref(curve), curveTable.size());
        if (res.second) {
            curveTable.emplace_back(std::cref(curve));
            curvePointCount += curve.getPoints().size();
        }
        return res.first->second;
    }

    auto getOrAddArrangeParam(const ArrangeParam& param) -> std::uint64_t {
        const auto res = arrangeParamOffsetMap.try_emplace(std::cref(param), currentArrangeParamOffset);
        if (res.second) {
            arrangeParams.emplace_back(std::cref(param));
            currentArrangeParamOffset += sizeof(std::int32_t) + SizeOf<xlink2::ResArrangeGroup>(this->game) * param.size();
        }
        return res.first->second;
    }

    auto getOrAddString(std::string_view value) -> std::uint64_t {
        const auto res = nameTable.try_emplace(value, strPoolSize);
        if (res.second) {
            strPoolSize += value.size() + 1;
        }
        return res.first->second;
    }
    
    auto getOrAddLocalPropertyName(std::string_view value) -> void {
        getOrAddString(value);
        localPropertyNameTable.emplace(value, 0);
    }
    
    auto getOrAddLocalPropertyEnumName(std::string_view value) -> void {
        getOrAddString(value);
        localPropertyEnumNameTable.emplace(value, 0);
    }

    auto getOrAddCondition(const ICondition* condition) -> std::uint64_t {
        if (condition == nullptr) {
            return 0xffffffffull;
        }

        if (condition->getType() == ConditionType::Blend
            || condition->getType() == ConditionType::Grid
            || condition->getType() == ConditionType::Jump) {
            return 0xffffffffull;
        }

        if (condition->getType() == ConditionType::Switch && static_cast<const SwitchCondition*>(condition)->getCompareType() == CompareType::Default) {
            return 0xffffffffull;
        }

        const auto res = conditionOffsetMap.try_emplace(std::cref(*condition), currentConditionOffset);
        if (!res.second) {
            return res.first->second;
        }

        conditions.emplace_back(std::cref(*condition));

        switch (condition->getType()) {
            case ConditionType::Switch: {
                const auto cond = static_cast<const SwitchCondition*>(condition);
                if (cond->getValue().index() == 0) {
                    switch (cond->getParentType()) {
                        case SwitchType::ActionSlot:
                            if (!SupportsActionSwitch(game)) {
                                common::AbortWithDetail("Action switches are not supported");
                            }
                            getOrAddString(std::get<0>(cond->getValue()));
                            break;
                        case SwitchType::GlobalProperty:
                            getOrAddString(std::get<0>(cond->getValue()));
                            break;
                        case SwitchType::LocalProperty:
                            getOrAddLocalPropertyEnumName(std::get<0>(cond->getValue()));
                            break;
                        case SwitchType::Null:
                            getOrAddString(""); // these shouldn't have conditions so this should be unreachable
                            break;
                    }
                    if (GetResSwitchConditionLayout(game) == 2) {
                        currentConditionOffset += getPtrSize();
                    }
                }
                currentConditionOffset += SizeOf<xlink2::ResSwitchCondition>(game);
                break;
            }
            case ConditionType::Random:
                currentConditionOffset += SizeOf<xlink2::ResRandomCondition>(game);
                break;
            case ConditionType::RandomNoRepeat:
                currentConditionOffset += SizeOf<xlink2::ResRandom2Condition>(game);
                break;
            case ConditionType::Blend:
                currentConditionOffset += SizeOf<xlink2::ResCondition>(game);
                break;
            case ConditionType::BlendBy:
                currentConditionOffset += SizeOf<xlink2::ResBlendByCondition>(game);
                break;
            case ConditionType::Sequence:
                currentConditionOffset += SizeOf<xlink2::ResSequenceCondition>(game);
                break;
            case ConditionType::Grid:
            case ConditionType::Jump:
                currentConditionOffset += SizeOf<xlink2::ResCondition>(game);
                break;
        }

        return res.first->second;
    }
};

struct AssetKey {
    std::string_view key;
    std::uint32_t guid;

    // this sorting isn't correct but it should still work
    auto operator<(const AssetKey& other) const -> bool {
        if (this->key != other.key) {
            return this->key < other.key;
        }
        return this->guid < other.guid;
    }
};

struct UserContext {
    std::reference_wrapper<const User> user;
    std::vector<CallTableInfo> assetCallTables = {};
    std::unordered_map<std::uint32_t, std::size_t> indexMap = {};
    std::map<AssetKey, std::uint16_t> assetIds = {};
    std::vector<std::reference_wrapper<const AssetCallTable>> containers = {};
    std::uint64_t currentContainerOffset = 0ull;
    std::uint64_t totalSize = 0ull;
    std::uint64_t triggerTablePos = 0ull;
    std::uint32_t assetCount = 0u;
    std::uint32_t randomContainerCount = 0u;
    std::uint32_t numActions = 0u;
    std::uint32_t numActionTriggers = 0u;
    std::uint32_t numPropertyTriggers = 0u;

    auto addAssetCallTable(const AssetCallTableHandle& act, SaveContext& ctx) -> void {
        totalSize += SizeOf<xlink2::ResAssetCallTable>(ctx.game) + sizeof(std::uint16_t); // ResAssetCallTable + index
        assetIds.emplace(AssetKey{ act->getKeyName(), act->getGUID() }, assetCallTables.size());
        indexMap.emplace(act->getGUID(), assetCallTables.size());
        assetCallTables.emplace_back(CallTableInfo{
            std::cref(*act),
            act->isContainer() ? currentContainerOffset : ctx.getOrAddAssetParam(static_cast<const Asset*>(act.get())->getParams()),
            ctx.getOrAddCondition(act->getCondition().get())
        });
    }
};

static auto ProcessParam(const Param& param, SaveContext& ctx) -> void {
    ctx.getOrAddString(param.getName());
    switch (param.getValueType()) {
        case ValueType::S32:
            ctx.getOrAddDirectValue(param.getValue<ValueType::S32>());
            break;
        case ValueType::Bitfield:
            ctx.getOrAddDirectValue(param.getValue<ValueType::Bitfield>());
            break;
        case ValueType::F32:
            ctx.getOrAddDirectValue(param.getValue<ValueType::F32>());
            break;
        case ValueType::Curve: {
            const auto& curve = param.getValue<ValueType::Curve>();
            ctx.getOrAddCurve(*curve);
            if (curve->getPropertyScope() == PropertyScope::Global) {
                ctx.getOrAddString(curve->getPropertyName());
            } else {
                ctx.getOrAddLocalPropertyName(curve->getPropertyName());
            }
            break;
        }
        case ValueType::Random:
            ctx.getOrAddRandom(*param.getValue<ValueType::Random>());
            break;
        case ValueType::Bool:
            ctx.getOrAddDirectValue(param.getValue<ValueType::Bool>());
            break;
        case ValueType::Enum:
            ctx.getOrAddDirectValue(param.getValue<ValueType::Enum>());
            break;
        case ValueType::String:
            ctx.getOrAddString(*param.getValue<ValueType::String>());
            break;
        case ValueType::ArrangeParam: {
            const auto& ap = param.getValue<ValueType::ArrangeParam>();
            ctx.getOrAddArrangeParam(*ap);
            for (const auto& group : *ap) {
                ctx.getOrAddString(group.getName());
            }
            break;
        }
        default:
            break;
    }
}

static auto ProcessParamDefine(const Param& param, SaveContext& ctx) -> void {
    ctx.pdtCtx.getOrAddString(param.getName());
    switch (param.getValueType()) {
        case ValueType::S32:
        case ValueType::F32:
        case ValueType::Bool:
        case ValueType::Enum:
            break;
        case ValueType::String:
            ctx.pdtCtx.getOrAddString(*param.getValue<ValueType::String>());
            break;
        case ValueType::ArrangeParam:
        default:
            break;
    }
}

static auto ProcessParamDefineTable(const ParamDefineTable& pdt, SaveContext& ctx) -> void {
    for (const auto& param : pdt.getSystemUserParams()) {
        if (!pdt.IsValidParamDefineValueType(param.getValueType())) {
            common::AbortWithDetail("Invalid value type for param define");
        }
        ProcessParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getCustomUserParams()) {
        if (!pdt.IsValidParamDefineValueType(param.getValueType())) {
            common::AbortWithDetail("Invalid value type for param define");
        }
        ProcessParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getSystemAssetParams()) {
        if (!pdt.IsValidParamDefineValueType(param.getValueType())) {
            common::AbortWithDetail("Invalid value type for param define");
        }
        ProcessParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getCustomAssetParams()) {
        if (!pdt.IsValidParamDefineValueType(param.getValueType())) {
            common::AbortWithDetail("Invalid value type for param define");
        }
        ProcessParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getTriggerParams()) {
        if (!pdt.IsValidParamDefineValueType(param.getValueType())) {
            common::AbortWithDetail("Invalid value type for param define");
        }
        ProcessParamDefine(param, ctx);
    }
}

static auto ProcessAssetCallTable(
    const AssetCallTableHandle& act,
    UserContext& uctx,
    SaveContext& ctx,
    std::queue<AssetCallTableHandle>& queue
) -> void {
    uctx.addAssetCallTable(act, ctx);
    ctx.getOrAddString(act->getKeyName());
    if (act->isContainer()) {
        uctx.containers.emplace_back(std::cref(*act));

        switch (act->getType()) {
            case CallTableType::Switch: {
                uctx.currentContainerOffset += SizeOf<xlink2::ResSwitchContainerParam>(ctx.game);
                const auto switch_ = static_cast<const Switch*>(act.get());
                switch (switch_->getSwitchType()) {
                    case SwitchType::ActionSlot:
                        ctx.getOrAddString(switch_->getSwitchVariable());
                        break;
                    case SwitchType::GlobalProperty:
                        ctx.getOrAddString(switch_->getSwitchVariable());
                        break;
                    case SwitchType::LocalProperty:
                        ctx.getOrAddLocalPropertyName(switch_->getSwitchVariable());
                        break;
                    case SwitchType::Null:
                        ctx.getOrAddString("");
                        break;
                }
                for (const auto& child : switch_->getChildren()) {
                    queue.emplace(child);
                }
                break;
            }
            case CallTableType::Random:
                uctx.currentContainerOffset += SizeOf<xlink2::ResContainerParam>(ctx.game);
                for (const auto& child : act->getChildren()) {
                    queue.emplace(child);
                }
                break;
            case CallTableType::RandomNoRepeat:
                uctx.currentContainerOffset += SizeOf<xlink2::ResContainerParam>(ctx.game);
                ++uctx.randomContainerCount;
                for (const auto& child : act->getChildren()) {
                    queue.emplace(child);
                }
                break;
            case CallTableType::Blend:
                uctx.currentContainerOffset += SizeOf<xlink2::ResContainerParam>(ctx.game);
                for (const auto& child : act->getChildren()) {
                    queue.emplace(child);
                }
                break;
            case CallTableType::BlendBy: {
                if (GetResContainerParamLayout(ctx.game) < 1) {
                    common::AbortWithDetail("Blend by containers are not supported on this version!");
                }
                uctx.currentContainerOffset += SizeOf<xlink2::ResSwitchContainerParam>(ctx.game);
                const auto blend = static_cast<const BlendBy*>(act.get());
                if (blend->getBlendPropertyScope() == PropertyScope::Global) {
                    ctx.getOrAddString(blend->getBlendPropertyName());
                } else {
                    ctx.getOrAddLocalPropertyName(blend->getBlendPropertyName());
                }
                for (const auto& child : blend->getChildren()) {
                    queue.emplace(child);
                }
                break;
            }
            case CallTableType::Sequence:
                uctx.currentContainerOffset += SizeOf<xlink2::ResContainerParam>(ctx.game);
                for (const auto& child : act->getChildren()) {
                    queue.emplace(child);
                }
                break;
            case CallTableType::Grid: {
                if (!SupportsGridContainers(ctx.game)) {
                    common::AbortWithDetail("Grid containers are not supported on this version!");
                }
                const auto grid = static_cast<const Grid*>(act.get());
                grid->ensureValidSize();
                uctx.currentContainerOffset += SizeOf<xlink2::ResGridContainerParam>(ctx.game)
                                            + sizeof(std::uint32_t) * grid->getValues1().size()
                                            + sizeof(std::uint32_t) * grid->getValues2().size()
                                            + sizeof(std::int32_t) * grid->getAssetCallTables().size();
                if (grid->getProperty1Scope() == PropertyScope::Global) {
                    ctx.getOrAddString(grid->getProperty1Name());
                    for (const auto& val : grid->getValues1()) {
                        ctx.getOrAddString(val);
                    }
                } else {
                    ctx.getOrAddLocalPropertyName(grid->getProperty1Name());
                    for (const auto& val : grid->getValues1()) {
                        ctx.getOrAddLocalPropertyEnumName(val);
                    }
                }
                if (grid->getProperty2Scope() == PropertyScope::Global) {
                    ctx.getOrAddString(grid->getProperty2Name());
                    for (const auto& val : grid->getValues2()) {
                        ctx.getOrAddString(val);
                    }
                } else {
                    ctx.getOrAddLocalPropertyName(grid->getProperty2Name());
                    for (const auto& val : grid->getValues2()) {
                        ctx.getOrAddLocalPropertyEnumName(val);
                    }
                }
                auto seen = std::unordered_set<std::uint32_t>{};
                for (const auto& child : grid->getChildren()) {
                    if (!child) {
                        continue;
                    }
                    if (seen.contains(child->getGUID())) {
                        continue;
                    }
                    seen.insert(child->getGUID());
                    queue.emplace(child);
                }
                break;
            }
            case CallTableType::Jump:
                if (!SupportsJumpContainers(ctx.game)) {
                    common::AbortWithDetail("Jump containers are not supported on this version!");
                }
                uctx.currentContainerOffset += SizeOf<xlink2::ResContainerParam>(ctx.game);
                break;
            default:
                std::unreachable();
        }
    } else {
        ++uctx.assetCount;
        for (const auto& param : static_cast<const Asset*>(act.get())->getParams()) {
            ProcessParam(param, ctx);
        }
    }
}

static auto ProcessUser(const User& user, SaveContext& ctx) -> void {
    if (user.hasKnownName()) {
        ctx.getOrAddString(user.getName());
    }
    auto& uctx = ctx.users.emplace(user.getHash(), UserContext{ std::cref(user) }).first->second;
    uctx.totalSize = SizeOf<xlink2::ResUserHeader>(ctx.game);
    for (const auto& prop : user.getLocalProperties()) {
        ctx.getOrAddString(prop);
    }
    uctx.totalSize += ctx.getPtrSize() * user.getLocalProperties().size();
    for (const auto& param : user.getParams()) {
        ProcessParam(param, ctx);
    }
    uctx.totalSize += SizeOf<xlink2::ResParam>(ctx.game) * user.getParams().size();
    auto callTableQueue = std::queue<AssetCallTableHandle>{};
    for (const auto& act : user.getAssetCallTables()) {
        callTableQueue.emplace(act);
    }
    auto seen = std::unordered_set<std::uint32_t>{};
    for (; !callTableQueue.empty(); callTableQueue.pop()) {
        const auto& act = callTableQueue.front();
        if (seen.contains(act->getGUID())) {
            continue;
        }
        seen.insert(act->getGUID());
        ProcessAssetCallTable(act, uctx, ctx, callTableQueue);
    }

    if (uctx.assetCallTables.size() & 1) {
        uctx.totalSize += sizeof(std::uint16_t);
    }
    uctx.totalSize += uctx.currentContainerOffset;
    uctx.triggerTablePos = uctx.totalSize;

    for (const auto& slot : user.getActionSlots()) {
        ctx.getOrAddString(slot.getName());
        for (const auto& action : slot.getActions()) {
            ++uctx.numActions;
            ctx.getOrAddString(action.getName());
            for (const auto& trigger : action.getTriggers()) {
                ++uctx.numActionTriggers;
                if (trigger.getType() == ActionTriggerType::Previous) {
                    ctx.getOrAddString(trigger.getPreviousAction());
                }
                ctx.getOrAddTriggerParam(trigger.getOverwriteParams());
                for (const auto& param : trigger.getOverwriteParams()) {
                    ProcessParam(param, ctx);
                }
            }
        }
    }
    for (const auto& prop : user.getProperties()) {
        ctx.getOrAddString(prop.getName());
        for (const auto& trigger : prop.getTriggers()) {
            ++uctx.numPropertyTriggers;
            ctx.getOrAddCondition(&trigger.getCondition());
            ctx.getOrAddTriggerParam(trigger.getOverwriteParams());
            for (const auto& param : trigger.getOverwriteParams()) {
                ProcessParam(param, ctx);
            }
        }
    }
    for (const auto& trigger : user.getAlwaysTriggers()) {
        ctx.getOrAddTriggerParam(trigger.getOverwriteParams());
        for (const auto& param : trigger.getOverwriteParams()) {
            ProcessParam(param, ctx);
        }
    }
    uctx.totalSize += SizeOf<xlink2::ResActionSlot>(ctx.game) * user.getActionSlots().size();
    uctx.totalSize += SizeOf<xlink2::ResAction>(ctx.game) * uctx.numActions;
    uctx.totalSize += SizeOf<xlink2::ResActionTrigger>(ctx.game) * uctx.numActionTriggers;
    uctx.totalSize += SizeOf<xlink2::ResProperty>(ctx.game) * user.getProperties().size();
    uctx.totalSize += SizeOf<xlink2::ResPropertyTrigger>(ctx.game) * uctx.numPropertyTriggers;
    uctx.totalSize += SizeOf<xlink2::ResAlwaysTrigger>(ctx.game) * user.getAlwaysTriggers().size();

    ctx.totalUserSize += uctx.totalSize;
}

static auto SaveParamDefine(const Param& def, SaveContext& ctx) -> void {
    ctx.writePtr(ctx.pdtCtx.stringPool.at(def.getName()));
    switch (def.getParamType()) {
        case ParamType::S32:
            if (def.getValueType() != ValueType::S32) {
                common::AbortWithDetail("S32 param define does not have S32 value");
            }
            ctx.writer.write(static_cast<std::uint32_t>(xlink2::ParamType::Int));
            if (ctx.is64Bit()) {
                ctx.writer.skip(4);
                ctx.writer.write(static_cast<std::int64_t>(def.getValue<ValueType::S32>()));
            } else {
                ctx.writer.write(def.getValue<ValueType::S32>());
            }
            break;
        case ParamType::F32:
            if (def.getValueType() != ValueType::F32) {
                common::AbortWithDetail("F32 param define does not have F32 value");
            }
            ctx.writer.write(static_cast<std::uint32_t>(xlink2::ParamType::Float));
            if (ctx.is64Bit()) {
                ctx.writer.skip(4);
                ctx.writer.write(static_cast<double>(def.getValue<ValueType::F32>()));
            } else {
                ctx.writer.write(def.getValue<ValueType::F32>());
            }
            break;
        case ParamType::Bool:
            if (def.getValueType() != ValueType::Bool) {
                common::AbortWithDetail("Bool param define does not have Bool value");
            }
            ctx.writer.write(static_cast<std::uint32_t>(xlink2::ParamType::Bool));
            if (ctx.is64Bit()) {
                ctx.writer.skip(4);
                ctx.writer.write(static_cast<std::uint64_t>(def.getValue<ValueType::Bool>()));
            } else {
                ctx.writer.write(static_cast<std::uint32_t>(def.getValue<ValueType::Bool>()));
            }
            break;
        case ParamType::Enum:
            if (def.getValueType() != ValueType::Enum) {
                common::AbortWithDetail("Enum param define does not have Enum value");
            }
            ctx.writer.write(static_cast<std::uint32_t>(xlink2::ParamType::Enum));
            if (ctx.is64Bit()) {
                ctx.writer.skip(4);
                ctx.writer.write(static_cast<std::uint64_t>(def.getValue<ValueType::Enum>()));
            } else {
                ctx.writer.write(def.getValue<ValueType::Enum>());
            }
            break;
        case ParamType::String:
            if (def.getValueType() != ValueType::String) {
                common::AbortWithDetail("String param define does not have String value");
            }
            ctx.writer.write(static_cast<std::uint32_t>(xlink2::ParamType::String));
            if (ctx.is64Bit()) {
                ctx.writer.skip(4);
            }
            ctx.writePtr(ctx.pdtCtx.stringPool.at(*def.getValue<ValueType::String>()));
            break;
        case ParamType::Custom:
            if (def.getValueType() != ValueType::ArrangeParam) {
                common::AbortWithDetail("ArrangeParam param define does not have ArrangeParam value");
            }
            ctx.writer.write(static_cast<std::uint32_t>(xlink2::ParamType::Custom));
            if (ctx.is64Bit()) {
                ctx.writer.skip(4);
            }
            ctx.writePtr(0);
            break;
    }
}

static auto SaveParamDefineTable(const ParamDefineTable& pdt, SaveContext& ctx) -> void {
    const auto dataSize = SizeOf<xlink2::ParamDefineTableHeader>(ctx.game)
                        + SizeOf<xlink2::ResParamDefine>(ctx.game) * (pdt.getNumUserParams() + pdt.getNumAssetParams() + pdt.getNumTriggerParams())
                        + common::AlignUp(ctx.pdtCtx.strPoolSize, ctx.getPtrSize());
    ctx.writer.write(static_cast<std::uint32_t>(dataSize));
    ctx.writer.write(static_cast<std::int32_t>(pdt.getNumUserParams()));
    ctx.writer.write(static_cast<std::int32_t>(pdt.getNumAssetParams()));
    ctx.writer.write(static_cast<std::int32_t>(pdt.getNumCustomAssetParams()));
    ctx.writer.write(static_cast<std::int32_t>(pdt.getNumTriggerParams()));
    if (ctx.is64Bit()) {
        ctx.writer.skip(4);
    }
    for (const auto& param : pdt.getSystemUserParams()) {
        SaveParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getCustomUserParams()) {
        SaveParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getSystemAssetParams()) {
        SaveParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getCustomAssetParams()) {
        SaveParamDefine(param, ctx);
    }
    for (const auto& param : pdt.getTriggerParams()) {
        SaveParamDefine(param, ctx);
    }
    for (const auto& str : ctx.pdtCtx.stringPool | std::views::keys) {
        ctx.writer.writeString(str);
    }
    ctx.writer.alignUp(ctx.getPtrSize());
}

[[nodiscard]] static auto CheckValidParamTypes(const Param& param, const Param& def) -> bool {
    const auto vt = param.getValueType();
    switch (def.getParamType()) {
        case ParamType::S32:
            return vt == ValueType::S32 || vt == ValueType::Bitfield;
        case ParamType::F32:
            return vt == ValueType::F32 || vt == ValueType::Random || vt == ValueType::Curve;
        case ParamType::Bool:
            return vt == ValueType::Bool;
        case ParamType::Enum:
            return vt == ValueType::Enum;
        case ParamType::String:
            return vt == ValueType::String;
        case ParamType::Custom:
            return vt == ValueType::ArrangeParam || vt == ValueType::S32;
        default:
            common::AbortWithDetail("Unknown param type");
    }
}

[[nodiscard]] static auto GetRandomReferenceType(const RandomTable& rand) -> xlink2::ReferenceType {
    switch (rand.getType()) {
        case RandomType::Linear:
            return xlink2::ReferenceType::RandomLinear;
        case RandomType::InflectedPolynomial:
            if (rand.getPower() == 2.f) {
                return xlink2::ReferenceType::RandomInflectedPolynomial2;
            } else if (rand.getPower() == 3.f) {
                return xlink2::ReferenceType::RandomInflectedPolynomial3;
            } else if (rand.getPower() == 4.f) {
                return xlink2::ReferenceType::RandomInflectedPolynomial4;
            } else if (rand.getPower() == 1.5f) {
                return xlink2::ReferenceType::RandomInflectedPolynomial1Point5;
            } else {
                common::AbortWithDetail("Invalid random power");
            }
        case RandomType::IncreasingPolynomial:
            if (rand.getPower() == 2.f) {
                return xlink2::ReferenceType::RandomIncreasingPolynomial2;
            } else if (rand.getPower() == 3.f) {
                return xlink2::ReferenceType::RandomIncreasingPolynomial3;
            } else if (rand.getPower() == 4.f) {
                return xlink2::ReferenceType::RandomIncreasingPolynomial4;
            } else if (rand.getPower() == 1.5f) {
                return xlink2::ReferenceType::RandomIncreasingPolynomial1Point5;
            } else {
                common::AbortWithDetail("Invalid random power");
            }
        case RandomType::DecreasingPolynomial:
            if (rand.getPower() == 2.f) {
                return xlink2::ReferenceType::RandomDecreasingPolynomial2;
            } else if (rand.getPower() == 3.f) {
                return xlink2::ReferenceType::RandomDecreasingPolynomial3;
            } else if (rand.getPower() == 4.f) {
                return xlink2::ReferenceType::RandomDecreasingPolynomial4;
            } else if (rand.getPower() == 1.5f) {
                return xlink2::ReferenceType::RandomDecreasingPolynomial1Point5;
            } else {
                common::AbortWithDetail("Invalid random power");
            }
        default:
            common::AbortWithDetail("Invalid random type");
    }
}

[[nodiscard]] static auto PackParamValue(xlink2::ReferenceType refType, std::uint32_t value) -> std::uint32_t {
    return (value & 0xffffff) | static_cast<std::uint32_t>(refType) << 0x18;
}

static auto SaveParam(const Param& param, SaveContext& ctx) -> void {
    switch (param.getValueType()) {
        case ValueType::S32:
            ctx.writer.write(PackParamValue(
                xlink2::ReferenceType::Direct, ctx.directValueIndexMap.at(static_cast<std::uint32_t>(param.getValue<ValueType::S32>())))
            );
            break;
        case ValueType::Bitfield:
            ctx.writer.write(PackParamValue(xlink2::ReferenceType::Bitfield, ctx.directValueIndexMap.at(param.getValue<ValueType::Bitfield>())));
            break;
        case ValueType::F32:
            ctx.writer.write(PackParamValue(
                xlink2::ReferenceType::Direct, ctx.directValueIndexMap.at(std::bit_cast<std::uint32_t>(param.getValue<ValueType::F32>())))
            );
            break;
        case ValueType::Curve:
            ctx.writer.write(PackParamValue(
                xlink2::ReferenceType::Curve, ctx.curveIndexMap.at(std::cref(*param.getValue<ValueType::Curve>())))
            );
            break;
        case ValueType::Random: {
            const auto& rand = param.getValue<ValueType::Random>();
            ctx.writer.write(PackParamValue(GetRandomReferenceType(*rand), ctx.randomIndexMap.at(std::cref(*rand))));
            break;
        }
        case ValueType::Bool:
            ctx.writer.write(PackParamValue(
                xlink2::ReferenceType::Direct, ctx.directValueIndexMap.at(static_cast<std::uint32_t>(param.getValue<ValueType::Bool>())))
            );
            break;
        case ValueType::Enum:
            ctx.writer.write(PackParamValue(xlink2::ReferenceType::Direct, ctx.directValueIndexMap.at(param.getValue<ValueType::Enum>())));
            break;
        case ValueType::String:
            ctx.writer.write(PackParamValue(xlink2::ReferenceType::String, ctx.nameTable.at(*param.getValue<ValueType::String>())));
            break;
        case ValueType::ArrangeParam:
            if (param.getValue<ValueType::ArrangeParam>()) {
                ctx.writer.write(PackParamValue(
                    xlink2::ReferenceType::ArrangeParam, ctx.arrangeParamOffsetMap.at(std::cref(*param.getValue<ValueType::ArrangeParam>()))
                ));
            } else {
                common::AbortWithDetail("ArrangeParam requires a non-null value!");
            }
            break;
        default:
            std::unreachable();
    }
}

static auto SaveAssetParams(const std::vector<Param>& params, SaveContext& ctx, const std::vector<Param>& sys, const std::vector<Param>& custom) -> void {
    auto unique = std::vector<std::reference_wrapper<const Param>>{};
    auto mask = std::uint64_t(0);
    for (const auto& [i, def] : std::views::concat(sys, custom) | std::views::enumerate) {
        for (const auto& param : params) {
            if (param.getName() == def.getName()) {
                if (!CheckValidParamTypes(param, def)) {
                    common::AbortWithDetail("Invalid value type for param");
                }
                mask |= 1ull << i;
                unique.emplace_back(std::cref(param));
                break;
            }
        }
    }
    ctx.writer.write(mask);
    for (const auto& param : unique) {
        SaveParam(param, ctx);
    }
}

static auto SaveTriggerParams(const std::vector<Param>& params, SaveContext& ctx, const std::vector<Param>& defines) -> void {
    auto unique = std::vector<std::reference_wrapper<const Param>>{};
    auto mask = std::uint32_t(0);
    for (const auto& [i, def] : defines | std::views::enumerate) {
        for (const auto& param : params) {
            if (param.getName() == def.getName()) {
                if (!CheckValidParamTypes(param, def)) {
                    common::AbortWithDetail("Invalid value type for param");
                }
                mask |= 1u << i;
                unique.emplace_back(std::cref(param));
                break;
            }
        }
    }
    ctx.writer.write(mask);
    for (const auto& param : unique) {
        SaveParam(param, ctx);
    }
}

static auto SaveUserParams(const std::vector<Param>& params, SaveContext& ctx, const std::vector<Param>& sys, const std::vector<Param>& custom) -> void {
    for (const auto& def : std::views::concat(sys, custom)) {
        auto found = false;
        for (const auto& param : params) {
            if (param.getName() == def.getName()) {
                if (!CheckValidParamTypes(param, def)) {
                    common::AbortWithDetail("Invalid value type for param");
                }
                SaveParam(param, ctx);
                found = true;
                break;
            }
        }
        if (!found) {
            SaveParam(def, ctx);
        }
    }
}

[[nodiscard]] static auto ConvertCurveType(CurveType type) -> xlink2::CurveType {
    switch (type) {
        case CurveType::Standard: return xlink2::CurveType::Standard;
        case CurveType::Constant: return xlink2::CurveType::Constant;
        default:
            common::AbortWithDetail("Invalid curve type");
    }
}

static auto SaveCurve(const Curve& curve, std::uint16_t& pointIndex, SaveContext& ctx) -> void {
    ctx.writer.write(pointIndex);
    pointIndex += curve.getPoints().size();
    ctx.writer.write(static_cast<std::uint16_t>(curve.getPoints().size()));
    ctx.writer.write(static_cast<std::uint16_t>(ConvertCurveType(curve.getType())));
    ctx.writer.write(static_cast<std::uint16_t>(curve.getPropertyScope() == PropertyScope::Global ? 1 : 0));
    ctx.writePtr(ctx.nameTable.at(curve.getPropertyName()));
    ctx.writer.write(curve.getUnknown1());
    if (curve.getPropertyScope() == PropertyScope::Global) {
        ctx.writer.write(static_cast<std::int16_t>(-1));
    } else {
        ctx.writer.write(static_cast<std::int16_t>(ctx.localPropertyNameTable.at(curve.getPropertyName())));
    }
    ctx.writer.write(curve.getUnknown2());
}

[[nodiscard]] static auto ConvertLimitType(LimitType type) -> xlink2::LimitType {
    switch (type) {
        case LimitType::None: return xlink2::LimitType::None;
        case LimitType::PriorityThenOldest: return xlink2::LimitType::PriorityThenOldest;
        case LimitType::PriorityThenNewest: return xlink2::LimitType::PriorityThenNewest;
        case LimitType::OldestThenPriority: return xlink2::LimitType::OldestThenPriority;
        case LimitType::NewestThenPriority: return xlink2::LimitType::NewestThenPriority;
        case LimitType::SpatialPriorityThenOldest: return xlink2::LimitType::SpatialPriorityThenOldest;
        case LimitType::SpatialPriorityThenNewest: return xlink2::LimitType::SpatialPriorityThenNewest;
        case LimitType::OldestThenSpatialPriority: return xlink2::LimitType::OldestThenSpatialPriority;
        case LimitType::NewestThenSpatialPriority: return xlink2::LimitType::NewestThenSpatialPriority;
        default: common::AbortWithDetail("Invalid limit type");
    }
}

static auto SaveArrangeGroup(const ArrangeGroup& group, SaveContext& ctx) -> void {
    ctx.writePtr(ctx.nameTable.at(group.getName()));
    ctx.writer.write(static_cast<std::uint8_t>(ConvertLimitType(group.getLimitType())));
    ctx.writer.write(group.getThreshold());
    ctx.writer.write(group.getIncludeFading());
    if (ctx.is64Bit()) {
        ctx.writer.skip(5);
    } else {
        ctx.writer.skip(1);
    }
}

static auto SaveAssetCallTable(
    const AssetCallTable& act,
    std::uint64_t paramOffset,
    std::uint64_t condOffset,
    std::int16_t assetIndex,
    UserContext& uctx,
    SaveContext& ctx
) -> void {
    ctx.writePtr(ctx.nameTable.at(act.getKeyName()));
    if (act.isAsset()) {
        ctx.writer.write(assetIndex);
    } else {
        ctx.writer.write(std::int16_t(-1));
    }
    auto flags = common::BitFlag<xlink2::CallTableFlags, std::uint16_t>();
    flags.set(xlink2::CallTableFlags::IsContainer, act.isContainer());
    flags.set(xlink2::CallTableFlags::OneShot, act.getOneShot());
    flags.set(xlink2::CallTableFlags::NoPause, act.getNoPause());
    ctx.writer.write(static_cast<std::uint16_t>(flags));
    ctx.writer.write(act.getEmitCount());
    ctx.writer.write(static_cast<std::int32_t>(act.getParent() ? uctx.indexMap.at(act.getParent()->getGUID()) : -1));
    ctx.writer.write(act.getGUID());
    ctx.writer.write(common::CalcCRC32(act.getKeyName()));
    if (ctx.is64Bit()) {
        ctx.writer.skip(4);
    }
    ctx.writePtr(paramOffset);
    ctx.writePtr(condOffset);
}

[[nodiscard]] static auto ConvertContainerType(CallTableType type) -> xlink2::ContainerType {
    switch (type) {
        case CallTableType::Switch: return xlink2::ContainerType::Switch;
        case CallTableType::Random: return xlink2::ContainerType::Random;
        case CallTableType::RandomNoRepeat: return xlink2::ContainerType::Random2;
        case CallTableType::Blend: return xlink2::ContainerType::Blend;
        case CallTableType::BlendBy: return xlink2::ContainerType::Blend;
        case CallTableType::Sequence: return xlink2::ContainerType::Sequence;
        case CallTableType::Grid: return xlink2::ContainerType::Grid;
        case CallTableType::Jump: return xlink2::ContainerType::Jump;
        default: common::AbortWithDetail("Invalid container type");
    }
}

static auto SaveContainer(const AssetCallTable& act, UserContext& uctx, SaveContext& ctx) -> void {
    if (act.getChildren().size() > 0) {
        if (!act.getChildren().front() || !act.getChildren().back()) {
            common::AbortWithDetail("{}[{:#010x}]", act.getKeyName(), act.getGUID());
        }
    }
    const auto childStart = static_cast<std::int32_t>(act.getChildren().size() > 0 ? uctx.indexMap.at(act.getChildren().front()->getGUID()) : -1);
    const auto childEnd = static_cast<std::int32_t>(act.getChildren().size() > 0 ? uctx.indexMap.at(act.getChildren().back()->getGUID()) : -1);
    switch (GetResContainerParamLayout(ctx.game)) {
        case 0:
            ctx.writer.write(static_cast<std::uint32_t>(ConvertContainerType(act.getType())));
            ctx.writer.write(childStart);
            ctx.writer.write(childEnd);
            break;
        case 1:
            ctx.writer.write(static_cast<std::uint8_t>(ConvertContainerType(act.getType())));
            ctx.writer.write(act.getType() == CallTableType::BlendBy);
            ctx.writer.write(false);
            ctx.writer.skip(1);
            ctx.writer.write(childStart);
            ctx.writer.write(childEnd);
            ctx.writer.skip(4);
            break;
        default:
            common::AbortWithDetail("Unknown container param layout");
    }
    switch (act.getType()) {
        case CallTableType::Switch: {
            const auto& switch_ = static_cast<const Switch&>(act);
            ctx.writePtr(ctx.nameTable.at(switch_.getSwitchVariable()));
            ctx.writer.write(switch_.getUnknown());
            if (switch_.getSwitchType() == SwitchType::LocalProperty) {
                ctx.writer.write(static_cast<std::int16_t>(ctx.localPropertyNameTable.at(switch_.getSwitchVariable())));
            } else {
                ctx.writer.write(static_cast<std::int16_t>(-1));
            }
            ctx.writer.write(switch_.getSwitchType() == SwitchType::GlobalProperty);
            ctx.writer.write(switch_.getSwitchType() == SwitchType::ActionSlot);
            break;
        }
        case CallTableType::Random:
        case CallTableType::RandomNoRepeat:
        case CallTableType::Blend:
            break;
        case CallTableType::BlendBy: {
            const auto& blend = static_cast<const BlendBy&>(act);
            ctx.writePtr(ctx.nameTable.at(blend.getBlendPropertyName()));
            ctx.writer.write(blend.getUnknown());
            if (blend.getBlendPropertyScope() == PropertyScope::Local) {
                ctx.writer.write(static_cast<std::int16_t>(ctx.localPropertyNameTable.at(blend.getBlendPropertyName())));
            } else {
                ctx.writer.write(static_cast<std::int16_t>(-1));
            }
            ctx.writer.write(blend.getBlendPropertyScope() == PropertyScope::Global);
            ctx.writer.write(false);
            break;
        }
        case CallTableType::Sequence:
            break;
        case CallTableType::Grid: {
            const auto& grid = static_cast<const Grid&>(act);
            grid.ensureValidSize();
            ctx.writePtr(ctx.nameTable.at(grid.getProperty1Name()));
            ctx.writePtr(ctx.nameTable.at(grid.getProperty2Name()));
            if (grid.getProperty1Scope() == PropertyScope::Local) {
                ctx.writer.write(static_cast<std::int16_t>(ctx.localPropertyNameTable.at(grid.getProperty1Name())));
            } else {
                ctx.writer.write(static_cast<std::int16_t>(-1));
            }
            if (grid.getProperty2Scope() == PropertyScope::Local) {
                ctx.writer.write(static_cast<std::int16_t>(ctx.localPropertyNameTable.at(grid.getProperty2Name())));
            } else {
                ctx.writer.write(static_cast<std::int16_t>(-1));
            }
            ctx.writer.write(static_cast<std::uint16_t>(
                (grid.getProperty1Scope() == PropertyScope::Global) | (grid.getProperty2Scope() == PropertyScope::Global) << 1
            ));
            ctx.writer.write(static_cast<std::uint8_t>(grid.getValues1().size()));
            ctx.writer.write(static_cast<std::uint8_t>(grid.getValues2().size()));
            for (const auto& val : grid.getValues1()) {
                if (grid.getProperty1Scope() == PropertyScope::Global) {
                    ctx.writer.write(static_cast<std::uint32_t>(ctx.nameTable.at(val)));
                } else {
                    ctx.writer.write(static_cast<std::uint32_t>(ctx.localPropertyEnumNameTable.at(val)));
                }
            }
            for (const auto& val : grid.getValues2()) {
                if (grid.getProperty2Scope() == PropertyScope::Global) {
                    ctx.writer.write(static_cast<std::uint32_t>(ctx.nameTable.at(val)));
                } else {
                    ctx.writer.write(static_cast<std::uint32_t>(ctx.localPropertyEnumNameTable.at(val)));
                }
            }
            for (const auto& act : grid.getAssetCallTables()) {
                if (act) {
                    ctx.writer.write(static_cast<std::int32_t>(uctx.indexMap.at(act->getGUID())));
                } else {
                    ctx.writer.write(-1);
                }
            }
            break;
        }
        case CallTableType::Jump:
            break;
        default:
            common::AbortWithDetail("Invalid container type!");
    }
}

static auto SaveAction(const Action& action, SaveContext& ctx, std::int32_t& currActionTriggerIndex) -> void {
    ctx.writePtr(ctx.nameTable.at(action.getName()));
    switch(GetResActionLayout(ctx.game)) {
        case 0:
            if (action.getTriggers().size() == 0) {
                // TODO: check what nintendo does in this case
                ctx.writer.write(static_cast<std::int32_t>(-1));
                ctx.writer.write(static_cast<std::int32_t>(0));
            } else {
                ctx.writer.write(static_cast<std::int32_t>(currActionTriggerIndex));
                ctx.writer.write(static_cast<std::int32_t>(currActionTriggerIndex + action.getTriggers().size() - 1));
                currActionTriggerIndex += action.getTriggers().size();
            }
            break;
        case 1:
            if (action.getTriggers().size() == 0) {
                // TODO: check what nintendo does in this case
                ctx.writer.write(static_cast<std::int16_t>(-1));
                ctx.writer.write(action.getIsPrefix());
                ctx.writer.skip(1);
                ctx.writer.write(static_cast<std::int32_t>(0));
            } else {
                ctx.writer.write(static_cast<std::int16_t>(currActionTriggerIndex));
                ctx.writer.write(action.getIsPrefix());
                ctx.writer.skip(1);
                ctx.writer.write(static_cast<std::int32_t>(currActionTriggerIndex + action.getTriggers().size() - 1));
                currActionTriggerIndex += action.getTriggers().size();
            }
            break;
        default:
            common::AbortWithDetail("Unknown action layout");
    }
}

static auto SaveActionTrigger(const ActionTrigger& trigger, UserContext& uctx, SaveContext& ctx) -> void {
    ctx.writer.write(trigger.getGUID());
    if (GetResActionTriggerLayout(ctx.game) == 1) {
        ctx.writer.write(trigger.getUnknown1());
    }
    ctx.writePtr(SizeOf<xlink2::ResAssetCallTable>(ctx.game) * uctx.indexMap.at(trigger.getAssetCallTable()->getGUID()));
    if (trigger.getType() == ActionTriggerType::Previous) {
        ctx.writePtr(ctx.nameTable.at(trigger.getPreviousAction()));
    } else {
        ctx.writer.write(trigger.getStartFrame());
        if (ctx.is64Bit()) {
            ctx.writer.skip(4);
        }
    }
    ctx.writer.write(trigger.getEndFrame());
    auto flags = common::BitFlag<xlink2::ActionTriggerType, std::uint16_t>();
    flags.set(xlink2::ActionTriggerType::OneShot, trigger.getOneShot());
    flags.set(xlink2::ActionTriggerType::Always, trigger.getType() == ActionTriggerType::Always);
    flags.set(xlink2::ActionTriggerType::OnLeave, trigger.getType() == ActionTriggerType::OnLeave);
    flags.set(xlink2::ActionTriggerType::Previous, trigger.getType() == ActionTriggerType::Previous);
    ctx.writer.write(static_cast<std::uint16_t>(flags));
    ctx.writer.write(trigger.getUnknown2());
    if (trigger.getOverwriteParams().size() == 0) {
        ctx.writePtr(0xffff'ffff);
    } else {
        ctx.writePtr(ctx.triggerParamOffsetMap.at(std::cref(trigger.getOverwriteParams())));
    }
}

static auto SavePropertyTrigger(const PropertyTrigger& trigger, UserContext& uctx, SaveContext& ctx) -> void {
    ctx.writer.write(trigger.getGUID());
    switch (GetResPropertyTriggerLayout(ctx.game)) {
        case 0: {
            ctx.writePtr(SizeOf<xlink2::ResAssetCallTable>(ctx.game) * uctx.indexMap.at(trigger.getAssetCallTable()->getGUID()));
            ctx.writePtr(ctx.conditionOffsetMap.at(std::cref(trigger.getCondition())));
            ctx.writer.write(static_cast<std::uint16_t>(trigger.getLazy() ? 1 : 0));
            ctx.writer.write(trigger.getUnknown());
            if (trigger.getOverwriteParams().size() == 0) {
                ctx.writePtr(0xffff'ffff);
            } else {
                ctx.writePtr(ctx.triggerParamOffsetMap.at(std::cref(trigger.getOverwriteParams())));
            }
            break;
        }
        case 1: {
            ctx.writer.write(static_cast<std::uint16_t>(trigger.getLazy() ? 1 : 0));
            ctx.writer.write(trigger.getUnknown());
            ctx.writePtr(SizeOf<xlink2::ResAssetCallTable>(ctx.game) * uctx.indexMap.at(trigger.getAssetCallTable()->getGUID()));
            ctx.writePtr(ctx.conditionOffsetMap.at(std::cref(trigger.getCondition())));
            if (trigger.getOverwriteParams().size() == 0) {
                ctx.writePtr(0xffff'ffff);
            } else {
                ctx.writePtr(ctx.triggerParamOffsetMap.at(std::cref(trigger.getOverwriteParams())));
            }
            break;
        }
        default:
            common::AbortWithDetail("Unknown property trigger layout");
    }
}

static auto SaveAlwaysTrigger(const AlwaysTrigger& trigger, UserContext& uctx, SaveContext& ctx) -> void {
    ctx.writer.write(trigger.getGUID());
    switch (GetResAlwaysTriggerLayout(ctx.game)) {
        case 0: {
            ctx.writePtr(SizeOf<xlink2::ResAssetCallTable>(ctx.game) * uctx.indexMap.at(trigger.getAssetCallTable()->getGUID()));
            ctx.writer.write(trigger.getFlags());
            ctx.writer.write(trigger.getUnknown());
            if (trigger.getOverwriteParams().size() == 0) {
                ctx.writePtr(0xffff'ffff);
            } else {
                ctx.writePtr(ctx.triggerParamOffsetMap.at(std::cref(trigger.getOverwriteParams())));
            }
            break;
        }
        case 1: {
            ctx.writer.write(trigger.getFlags());
            ctx.writer.write(trigger.getUnknown());
            ctx.writePtr(SizeOf<xlink2::ResAssetCallTable>(ctx.game) * uctx.indexMap.at(trigger.getAssetCallTable()->getGUID()));
            if (trigger.getOverwriteParams().size() == 0) {
                ctx.writePtr(0xffff'ffff);
            } else {
                ctx.writePtr(ctx.triggerParamOffsetMap.at(std::cref(trigger.getOverwriteParams())));
            }
            break;
        }
        default:
            common::AbortWithDetail("Unknown property trigger layout");
    }
}

static auto SaveUser(const User& user, UserContext& uctx, SaveContext& ctx, const ParamDefineTable& pdt) -> void {
    ctx.writer.write(0u);
    switch (GetResUserHeaderLayout(ctx.game)) {
        case 0:
            ctx.writer.write(static_cast<std::int32_t>(user.getLocalProperties().size()));
            break;
        case 1:
            ctx.writer.write(static_cast<std::int16_t>(user.getLocalProperties().size()));
            ctx.writer.write(user.getUnknown());
            break;
        default:
            common::AbortWithDetail("Unknown user layout");
    }
    ctx.writer.write(static_cast<std::uint32_t>(uctx.assetCallTables.size()));
    ctx.writer.write(static_cast<std::uint32_t>(uctx.assetCount));
    ctx.writer.write(static_cast<std::uint32_t>(uctx.randomContainerCount));
    ctx.writer.write(static_cast<std::uint32_t>(user.getActionSlots().size()));
    ctx.writer.write(static_cast<std::uint32_t>(uctx.numActions));
    ctx.writer.write(static_cast<std::uint32_t>(uctx.numActionTriggers));
    ctx.writer.write(static_cast<std::uint32_t>(user.getProperties().size()));
    ctx.writer.write(static_cast<std::uint32_t>(uctx.numPropertyTriggers));
    ctx.writer.write(static_cast<std::uint32_t>(user.getAlwaysTriggers().size()));
    if (ctx.is64Bit()) {
        ctx.writer.skip(4);
    }
    ctx.writePtr(uctx.triggerTablePos);

    for (const auto& prop : user.getLocalProperties()) {
        ctx.writePtr(ctx.nameTable.at(prop));
    }

    SaveUserParams(user.getParams(), ctx, pdt.getSystemUserParams(), pdt.getCustomUserParams());

    for (const auto id : uctx.assetIds | std::views::values) {
        ctx.writer.write(id);
    }

    if (uctx.assetIds.size() & 1) {
        ctx.writer.skip(2);
    }

    auto assetIndex = std::int16_t(0);
    for (const auto& [i, act] : uctx.assetCallTables | std::views::enumerate) {
        SaveAssetCallTable(act.act, act.containerOrAssetParamOffset, act.conditionOffset, assetIndex, uctx, ctx);
        if (act.act.get().isAsset()) {
            ++assetIndex;
        }
    }

    for (const auto& container : uctx.containers) {
        SaveContainer(container, uctx, ctx);
    }

    auto currActionIndex = 0;
    for (const auto& slot : user.getActionSlots()) {
        ctx.writePtr(ctx.nameTable.at(slot.getName()));
        if (slot.getActions().size() == 0) {
            // TODO: check what nintendo does in this case
            ctx.writer.write(static_cast<std::int16_t>(-1));
            ctx.writer.write(static_cast<std::int16_t>(0));
        } else {
            ctx.writer.write(static_cast<std::int16_t>(currActionIndex));
            ctx.writer.write(static_cast<std::int16_t>(currActionIndex + slot.getActions().size() - 1));
            currActionIndex += slot.getActions().size();
        }
        if (ctx.is64Bit()) {
            ctx.writer.skip(4);
        }
    }

    auto currActionTriggerIndex = 0;
    for (const auto& slot : user.getActionSlots()) {
        for (const auto& action : slot.getActions()) {
            SaveAction(action, ctx, currActionTriggerIndex);
        }
    }

    for (const auto& slot : user.getActionSlots()) {
        for (const auto& action : slot.getActions()) {
            for (const auto& trigger : action.getTriggers()) {
                SaveActionTrigger(trigger, uctx, ctx);
            }
        }
    }

    auto currPropertyTriggerIndex = 0;
    for (const auto& prop : user.getProperties()) {
        ctx.writePtr(ctx.nameTable.at(prop.getName()));
        ctx.writer.write(static_cast<std::uint32_t>(prop.getScope() == PropertyScope::Global ? 1 : 0));
        if (prop.getTriggers().size() == 0) {
            // TODO: check what nintendo does in this case
            ctx.writer.write(static_cast<std::int32_t>(-1));
            ctx.writer.write(static_cast<std::int32_t>(0));
        } else {
            ctx.writer.write(static_cast<std::int32_t>(currPropertyTriggerIndex));
            ctx.writer.write(static_cast<std::int32_t>(currPropertyTriggerIndex + prop.getTriggers().size() - 1));
            currPropertyTriggerIndex += prop.getTriggers().size();
        }
        if (ctx.is64Bit()) {
            ctx.writer.skip(4);
        }
    }

    for (const auto& prop : user.getProperties()) {
        for (const auto& trigger : prop.getTriggers()) {
            SavePropertyTrigger(trigger, uctx, ctx);
        }
    }

    for (const auto& trigger : user.getAlwaysTriggers()) {
        SaveAlwaysTrigger(trigger, uctx, ctx);
    }
}

[[nodiscard]] static auto ConvertConditionType(ConditionType type) -> xlink2::ContainerType {
    switch (type) {
        case ConditionType::Switch: return xlink2::ContainerType::Switch;
        case ConditionType::Random: return xlink2::ContainerType::Random;
        case ConditionType::RandomNoRepeat: return xlink2::ContainerType::Random2;
        case ConditionType::Blend: return xlink2::ContainerType::Blend;
        case ConditionType::BlendBy: return xlink2::ContainerType::Blend;
        case ConditionType::Sequence: return xlink2::ContainerType::Sequence;
        case ConditionType::Grid: return xlink2::ContainerType::Grid;
        case ConditionType::Jump : return xlink2::ContainerType::Jump;
        default: common::AbortWithDetail("Unknown condition type");
    }
}

[[nodiscard]] static auto ConvertPropertyType(PropertyType type) -> xlink2::PropertyType {
    switch (type) {
        case PropertyType::Enum: return xlink2::PropertyType::Enum;
        case PropertyType::S32: return xlink2::PropertyType::S32;
        case PropertyType::F32: return xlink2::PropertyType::F32;
        case PropertyType::Bool: return xlink2::PropertyType::Bool;
        default: common::AbortWithDetail("Unknown property type");
    }
}

[[nodiscard]] static auto ConvertCompareType(CompareType type) -> xlink2::CompareType {
    switch (type) {
        case CompareType::Equal: return xlink2::CompareType::Equal;
        case CompareType::GreaterThan: return xlink2::CompareType::GreaterThan;
        case CompareType::GreaterThanOrEqual: return xlink2::CompareType::GreaterThanOrEqual;
        case CompareType::LessThan: return xlink2::CompareType::LessThan;
        case CompareType::LessThanOrEqual: return xlink2::CompareType::LessThanOrEqual;
        case CompareType::NotEqual: return xlink2::CompareType::NotEqual;
        default: common::AbortWithDetail("Unknown compare type");
    }
}

static auto SaveSwitchCondition(const SwitchCondition& switch_, SaveContext& ctx) -> void {
    PropertyType propType;
    switch (switch_.getValue().index()) {
        case 0:
            propType = PropertyType::Enum;
            break;
        case 1:
            propType = PropertyType::S32;
            break;
        case 2:
            propType = PropertyType::F32;
            break;
        case 3:
            propType = PropertyType::Bool;
            break;
        default:
            std::unreachable();
    }
    switch (GetResSwitchConditionLayout(ctx.game)) {
        case 0: {
            ctx.writer.write(static_cast<std::uint32_t>(ConvertPropertyType(propType)));
            ctx.writer.write(static_cast<std::uint32_t>(ConvertCompareType(switch_.getCompareType())));
            switch (propType) {
                case PropertyType::Enum:
                    switch (switch_.getParentType()) {
                        case SwitchType::ActionSlot:
                            common::AbortWithDetail("Action switches are unsupported on this version");
                        case SwitchType::GlobalProperty:
                            ctx.writePtr(ctx.nameTable.at(std::get<0>(switch_.getValue())));
                            ctx.writer.write(static_cast<std::int16_t>(-1));
                            break;
                        case SwitchType::LocalProperty:
                            ctx.writePtr(0);
                            ctx.writer.write(static_cast<std::int16_t>(ctx.localPropertyEnumNameTable.at(std::get<0>(switch_.getValue()))));
                            break;
                        default:
                            common::AbortWithDetail("Null switch cases shouldn't have conditions!");
                    }
                    break;
                case PropertyType::S32:
                    ctx.writer.write(std::get<1>(switch_.getValue()));
                    ctx.writer.write(static_cast<std::int16_t>(-1));
                    break;
                case PropertyType::F32:
                    ctx.writer.write(std::get<2>(switch_.getValue()));
                    ctx.writer.write(static_cast<std::int16_t>(-1));
                    break;
                case PropertyType::Bool:
                    ctx.writer.write(static_cast<std::uint32_t>(std::get<3>(switch_.getValue())));
                    ctx.writer.write(static_cast<std::int16_t>(-1));
                    break;
            }
            ctx.writer.write(false);
            ctx.writer.write(switch_.getParentType() == SwitchType::GlobalProperty);
            break;
        }
        case 1: {
            ctx.writer.write(static_cast<std::uint8_t>(ConvertPropertyType(propType)));
            ctx.writer.write(static_cast<std::uint8_t>(ConvertCompareType(switch_.getCompareType())));
            ctx.writer.write(false);
            ctx.writer.write(switch_.getParentType() == SwitchType::GlobalProperty);
            switch (propType) {
                case PropertyType::Enum:
                    switch (switch_.getParentType()) {
                        case SwitchType::ActionSlot:
                            common::AbortWithDetail("Action switches are unsupported on this version");
                        case SwitchType::GlobalProperty:
                            ctx.writePtr(ctx.nameTable.at(std::get<0>(switch_.getValue())));
                            ctx.writer.write(-1);
                            break;
                        case SwitchType::LocalProperty:
                            ctx.writePtr(0);
                            ctx.writer.write(static_cast<std::int32_t>(ctx.localPropertyEnumNameTable.at(std::get<0>(switch_.getValue()))));
                            break;
                        default:
                            common::AbortWithDetail("Null switch cases shouldn't have conditions!");
                    }
                    break;
                case PropertyType::S32:
                    ctx.writer.write(std::get<1>(switch_.getValue()));
                    ctx.writer.write(-1);
                    break;
                case PropertyType::F32:
                    ctx.writer.write(std::get<2>(switch_.getValue()));
                    ctx.writer.write(-1);
                    break;
                case PropertyType::Bool:
                    ctx.writer.write(static_cast<std::uint32_t>(std::get<3>(switch_.getValue())));
                    ctx.writer.write(-1);
                    break;
            }
            break;
        }
        case 2: {
            ctx.writer.write(static_cast<std::uint8_t>(ConvertPropertyType(propType)));
            ctx.writer.write(static_cast<std::uint8_t>(ConvertCompareType(switch_.getCompareType())));
            ctx.writer.write(false);
            ctx.writer.write(switch_.getParentType() == SwitchType::GlobalProperty);
            switch (propType) {
                case PropertyType::Enum:
                    switch (switch_.getParentType()) {
                        case SwitchType::ActionSlot:
                            ctx.writer.write(common::CalcCRC32(std::get<0>(switch_.getValue())));
                            ctx.writer.write(0u);
                            ctx.writePtr(ctx.nameTable.at(std::get<0>(switch_.getValue())));
                            break;
                        case SwitchType::GlobalProperty:
                            ctx.writer.write(-1);
                            ctx.writer.write(0u);
                            ctx.writePtr(ctx.nameTable.at(std::get<0>(switch_.getValue())));
                            break;
                        case SwitchType::LocalProperty:
                            ctx.writer.write(static_cast<std::int32_t>(ctx.localPropertyEnumNameTable.at(std::get<0>(switch_.getValue()))));
                            ctx.writer.write(0u);
                            ctx.writePtr(ctx.nameTable.at(std::get<0>(switch_.getValue())));
                            break;
                        default:
                            common::AbortWithDetail("Null switch cases shouldn't have conditions!");
                    }
                    break;
                case PropertyType::S32:
                    ctx.writer.write(-1);
                    ctx.writer.write(std::get<1>(switch_.getValue()));
                    break;
                case PropertyType::F32:
                    ctx.writer.write(-1);
                    ctx.writer.write(std::get<2>(switch_.getValue()));
                    break;
                case PropertyType::Bool:
                    ctx.writer.write(-1);
                    ctx.writer.write(static_cast<std::uint32_t>(std::get<3>(switch_.getValue())));
                    break;
            }
            break;
        }
        default:
            common::AbortWithDetail("Unknown switch condition layout");
    }
}

[[nodiscard]] static auto ConvertBlendOp(BlendOp op) -> xlink2::BlendType {
    switch (op) {
        case BlendOp::None: return xlink2::BlendType::None;
        case BlendOp::Multiply: return xlink2::BlendType::Multiply;
        case BlendOp::SquareRoot: return xlink2::BlendType::SquareRoot;
        case BlendOp::Sin: return xlink2::BlendType::Sin;
        case BlendOp::Add: return xlink2::BlendType::Add;
        case BlendOp::SetToOne: return xlink2::BlendType::SetToOne;
        default: common::AbortWithDetail("Unknown blend op");
    }
}

static auto SaveCondition(const ICondition& condition, SaveContext& ctx) -> void {
    ctx.writer.write(static_cast<std::uint32_t>(ConvertConditionType(condition.getType())));
    switch (condition.getType()) {
        case ConditionType::Switch:
            SaveSwitchCondition(static_cast<const SwitchCondition&>(condition), ctx);
            break;
        case ConditionType::Random:
            ctx.writer.write(static_cast<const RandomCondition&>(condition).getWeight());
            break;
        case ConditionType::RandomNoRepeat:
            ctx.writer.write(static_cast<const RandomNoRepeatCondition&>(condition).getWeight());
            break;
        case ConditionType::Blend:
            break;
        case ConditionType::BlendBy: {
            const auto& blend = static_cast<const BlendByCondition&>(condition);
            ctx.writer.write(blend.getValueMin());
            ctx.writer.write(blend.getValueMax());
            ctx.writer.write(static_cast<std::uint8_t>(ConvertBlendOp(blend.getBlendOpMin())));
            ctx.writer.write(static_cast<std::uint8_t>(ConvertBlendOp(blend.getBlendOpMax())));
            ctx.writer.skip(2);
            break;
        }
        case ConditionType::Sequence:
            ctx.writer.write(static_cast<const SequenceCondition&>(condition).getContinueOnFade());
            break;
        case ConditionType::Grid:
        case ConditionType::Jump:
            break;
    }
}

auto Database::save(Game game, Platform platform) const -> std::vector<std::uint8_t> {
    auto ctx = SaveContext{
        .writer = common::BinaryWriter(GetEndianness(platform)),
        .game = game
    };

    // gather all resource instances
    ProcessParamDefineTable(*mParamDefineTable, ctx);
    for (const auto& user : mUsers) {
        if (user->getParams().size() != mParamDefineTable->getNumUserParams()) {
            common::AbortWithDetail(
                "User {:#010x} does not have the expected number of user params! {} vs. {}",
                user->getHash(), user->getParams().size(), mParamDefineTable->getNumUserParams()
            );
        }
        ProcessUser(*user, ctx);
    }

    {
        auto currOffset = std::uint64_t(0);
        for (auto& [str, offset] : ctx.pdtCtx.stringPool) {
            offset = currOffset;
            currOffset += str.size() + 1;
        }
    }

    for (const auto& [i, index] : ctx.localPropertyNameTable | std::views::values | std::views::enumerate) {
        index = i;
    }

    for (const auto& [i, index] : ctx.localPropertyEnumNameTable | std::views::values | std::views::enumerate) {
        index = i;
    }

    {
        auto currOffset = std::uint64_t(0);
        for (auto& [str, offset] : ctx.nameTable) {
            offset = currOffset;
            currOffset += str.size() + 1;
        }
    }

    // calculate offsets
    const auto pdtSize = SizeOf<xlink2::ParamDefineTableHeader>(ctx.game)
                       + SizeOf<xlink2::ResParamDefine>(ctx.game) * (mParamDefineTable->getNumUserParams()
                                                                         + mParamDefineTable->getNumAssetParams()
                                                                         + mParamDefineTable->getNumTriggerParams())
                       + common::AlignUp(ctx.pdtCtx.strPoolSize, ctx.getPtrSize());
    const auto triggerOverwriteParamTablePos = SizeOf<xlink2::ResourceHeader>(ctx.game)
                                             + common::AlignUp((sizeof(std::uint32_t) + ctx.getPtrSize()) * ctx.users.size(), ctx.getPtrSize())
                                             + pdtSize
                                             + ctx.currentAssetParamOffset;
    const auto localPropertyNameRefTablePos = triggerOverwriteParamTablePos + ctx.currentTriggerParamOffset;
    const auto exRegionPos = localPropertyNameRefTablePos
                           + ctx.getPtrSize() * (ctx.localPropertyNameTable.size() + ctx.localPropertyEnumNameTable.size())
                           + sizeof(std::uint32_t) * ctx.directValues.size()
                           + SizeOf<xlink2::ResRandom>(ctx.game) * ctx.randomTable.size()
                           + SizeOf<xlink2::ResCurve>(ctx.game) * ctx.curveTable.size()
                           + SizeOf<xlink2::ResCurvePoint>(ctx.game) * ctx.curvePointCount;
    const auto userTablePos = exRegionPos + ctx.currentArrangeParamOffset;
    const auto conditionTablePos = userTablePos + ctx.totalUserSize;
    const auto nameTablePos = conditionTablePos + ctx.currentConditionOffset;
    const auto dataSize = nameTablePos + common::AlignUp(ctx.strPoolSize, ctx.getPtrSize());

    // header
    ctx.writer.writeArrayFixed<char, 4>({ 'X', 'L', 'N', 'K' });
    ctx.writer.write(static_cast<std::uint32_t>(dataSize));
    ctx.writer.write(mModuleType == ModuleType::ELink ? GetVersionELink(ctx.game) : GetVersionSLink(ctx.game));
    ctx.writer.write(ctx.numResParam);
    ctx.writer.write(static_cast<std::uint32_t>(ctx.assetParams.size()));
    ctx.writer.write(static_cast<std::uint32_t>(ctx.triggerParams.size()));
    ctx.writePtr(triggerOverwriteParamTablePos);
    ctx.writePtr(localPropertyNameRefTablePos);
    ctx.writer.write(static_cast<std::uint32_t>(ctx.localPropertyNameTable.size()));
    ctx.writer.write(static_cast<std::uint32_t>(ctx.localPropertyEnumNameTable.size()));
    ctx.writer.write(static_cast<std::uint32_t>(ctx.directValues.size()));
    ctx.writer.write(static_cast<std::uint32_t>(ctx.randomTable.size()));
    ctx.writer.write(static_cast<std::uint32_t>(ctx.curveTable.size()));
    ctx.writer.write(ctx.curvePointCount);
    ctx.writePtr(exRegionPos);
    ctx.writer.write(static_cast<std::uint32_t>(ctx.users.size()));
    if (ctx.is64Bit()) {
        ctx.writer.skip(4);
    }
    ctx.writePtr(conditionTablePos);
    ctx.writePtr(nameTablePos);

    // user hashes + offsets
    for (const auto& hash : ctx.users | std::views::keys) {
        ctx.writer.write(hash);
    }
    ctx.writer.alignUp(ctx.getPtrSize());
    {
        auto currentUserOffset = userTablePos;
        for (const auto& user : ctx.users | std::views::values) {
            ctx.writePtr(currentUserOffset);
            currentUserOffset += user.totalSize;
        }
    }

    ctx.writer.alignUp(ctx.getPtrSize());
    // pdt
    SaveParamDefineTable(*mParamDefineTable, ctx);

    ctx.writer.alignUp(ctx.getPtrSize());
    // asset params
    for (const auto& params : ctx.assetParams) {
        SaveAssetParams(params.get(), ctx, mParamDefineTable->getSystemAssetParams(), mParamDefineTable->getCustomAssetParams());
    }
    
    // trigger params
    for (const auto& params : ctx.triggerParams) {
        SaveTriggerParams(params.get(), ctx, mParamDefineTable->getTriggerParams());
    }

    // local property names
    for (const auto& name : ctx.localPropertyNameTable | std::views::keys) {
        ctx.writePtr(ctx.nameTable.at(name));
    }

    // local property enum names
    for (const auto& name : ctx.localPropertyEnumNameTable | std::views::keys) {
        ctx.writePtr(ctx.nameTable.at(name));
    }

    // direct values
    for (const auto val : ctx.directValues) {
        ctx.writer.write(val);
    }

    // random table
    for (const auto& rand : ctx.randomTable) {
        ctx.writer.write(rand.get().getMin());
        ctx.writer.write(rand.get().getMax());
    }

    // curve table
    {
        auto pointIndex = std::uint16_t(0);
        for (const auto& curve : ctx.curveTable) {
            SaveCurve(curve, pointIndex, ctx);
        }
    }

    // curve points
    for (const auto& curve : ctx.curveTable) {
        for (const auto& point : curve.get().getPoints()) {
            ctx.writer.write(point.x);
            ctx.writer.write(point.y);
        }
    }

    // ex region (arrange params)
    for (const auto& param : ctx.arrangeParams) {
        ctx.writer.write(static_cast<std::int32_t>(param.get().size()));
        for (const auto& group : param.get()) {
            SaveArrangeGroup(group, ctx);
        }
    }

    // users
    for (auto& uctx : ctx.users | std::views::values) {
        SaveUser(uctx.user, uctx, ctx, *mParamDefineTable);
    }

    // conditions
    for (const auto& cond : ctx.conditions) {
        SaveCondition(cond.get(), ctx);
    }

    // name table
    for (const auto& str : ctx.nameTable | std::views::keys) {
        ctx.writer.writeString(str);
    }

    if (ctx.writer.tell() < dataSize) {
        ctx.writer.seek(dataSize);
    }
    return ctx.writer.flush();
}

} // namespace mango