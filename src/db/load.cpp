#include "asset/asset.hpp"
#include "asset/blend.hpp"
#include "asset/grid.hpp"
#include "asset/jump.hpp"
#include "asset/random.hpp"
#include "asset/sequence.hpp"
#include "asset/switch.hpp"
#include "common/hash.hpp"
#include "common/error.hpp"
#include "common/reader.hpp"
#include "condition/blend.hpp"
#include "condition/random.hpp"
#include "condition/sequence.hpp"
#include "condition/switch.hpp"
#include "db/db.hpp"
#include "db/pdt.hpp"
#include "db/user.hpp"
#include "db/username.hpp"
#include "db/version.hpp"
#include "res/act.hpp"
#include "res/action.hpp"
#include "res/condition.hpp"
#include "res/param.hpp"
#include "res/pdt.hpp"
#include "res/property.hpp"

#include <memory>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

// TODO: double-check signedness of all fields

namespace mango {

struct LoadContext {
    common::BinaryReader reader;
    Game game;

    std::uint32_t numSystemUserParam = 0u;

    std::uint32_t numResParam = 0u;
    std::uint32_t numResAssetParam = 0u;
    std::uint32_t numResTriggerOverwriteParam = 0u;
    std::uint32_t numDirectValueTable = 0u;
    std::uint32_t numRandomTable = 0u;
    std::uint32_t numCurveTable = 0u;
    std::uint32_t numCurvePointTable = 0u;

    std::uint64_t assetParamPos = 0u;
    std::uint64_t triggerOverwriteParamTablePos = 0u;
    std::uint64_t directValueTablePos = 0u;
    std::uint64_t randomTablePos = 0u;
    std::uint64_t curveTablePos = 0u;
    std::uint64_t curvePointTablePos = 0u;
    std::uint64_t exRegionPos = 0u;
    std::uint64_t conditionTablePos = 0u;

    // this changes with every user
    std::uint64_t assetCallTableTablePos = 0u;
    std::uint64_t containerTablePos = 0u;
    std::uint64_t actionSlotTablePos = 0u;
    std::uint64_t actionTablePos = 0u;
    std::uint64_t actionTriggerTablePos = 0u;
    std::uint64_t propertyTablePos = 0u;
    std::uint64_t propertyTriggerTablePos = 0u;
    std::uint64_t alwaysTriggerTablePos = 0u;

    std::vector<std::string_view> localPropertyNameRefTable = {};
    std::vector<std::string_view> localPropertyEnumNameRefTable = {};
    std::unordered_map<std::uint64_t, std::string_view> nameTable = {};

    [[nodiscard]] auto readPtr() -> std::uint64_t {
        return is64Bit() ? reader.read<std::uint64_t>() : reader.read<std::uint32_t>();
    }

    auto alignToPtr() -> void {
        reader.alignUp(is64Bit() ? 8 : 4);
    }

    [[nodiscard]] auto is64Bit() const -> bool { return Is64Bit(game); }

    [[nodiscard]] auto getString(std::uint64_t offset) const -> std::string_view {
        const auto res = this->nameTable.find(offset);
        if (res == this->nameTable.end()) {
            common::AbortWithDetail("Invalid string offset @ {:#x}: {:#x}", this->reader.tell(), offset);
        }
        return res->second;
    }
    
    [[nodiscard]] auto getLocalPropertyName(std::uint32_t index) const -> std::string_view {
        if (index >= this->localPropertyNameRefTable.size()) {
            common::AbortWithDetail("Out of range local property name index @ {:#x}: {} (max {})", this->reader.tell(), index, localPropertyNameRefTable.size());
        }
        return this->localPropertyNameRefTable.at(index);
    }
    
    [[nodiscard]] auto getLocalPropertyEnumName(std::uint32_t index) const -> std::string_view {
        if (index >= this->localPropertyEnumNameRefTable.size()) {
            common::AbortWithDetail("Out of range local property enum name index @ {:#x}: {} (max {})", this->reader.tell(), index, localPropertyEnumNameRefTable.size());
        }
        return this->localPropertyEnumNameRefTable.at(index);
    }
    
    [[nodiscard]] auto getDirectValueU32(std::uint32_t index) const -> std::uint32_t {
        if (index >= numDirectValueTable) {
            common::AbortWithDetail("Out-of-range direct value! {} (count: {})", index, numDirectValueTable);
        }
        return reader.read<std::uint32_t>(directValueTablePos + sizeof(std::uint32_t) * index);
    }
    
    [[nodiscard]] auto getDirectValueS32(std::uint32_t index) const -> std::int32_t {
        if (index >= numDirectValueTable) {
            common::AbortWithDetail("Out-of-range direct value! {} (count: {})", index, numDirectValueTable);
        }
        return reader.read<std::int32_t>(directValueTablePos + sizeof(std::uint32_t) * index);
    }
    
    [[nodiscard]] auto getDirectValueF32(std::uint32_t index) const -> float {
        if (index >= numDirectValueTable) {
            common::AbortWithDetail("Out-of-range direct value! {} (count: {})", index, numDirectValueTable);
        }
        return reader.read<float>(directValueTablePos + sizeof(std::uint32_t) * index);
    }
};

struct CallTableInfo {
    std::uint64_t conditionOffset = 0xffffffffull;
    std::int32_t childStart = -1;
    std::int32_t childEnd = -1;
    std::vector<std::int32_t> childIndices = {};
};

static constexpr auto ConvertLimitType(xlink2::LimitType type) -> LimitType {
    switch (type) {
        case xlink2::LimitType::None: return LimitType::None;
        case xlink2::LimitType::PriorityThenOldest: return LimitType::PriorityThenOldest;
        case xlink2::LimitType::PriorityThenNewest: return LimitType::PriorityThenNewest;
        case xlink2::LimitType::OldestThenPriority: return LimitType::OldestThenPriority;
        case xlink2::LimitType::NewestThenPriority: return LimitType::NewestThenPriority;
        case xlink2::LimitType::SpatialPriorityThenOldest: return LimitType::SpatialPriorityThenOldest;
        case xlink2::LimitType::SpatialPriorityThenNewest: return LimitType::SpatialPriorityThenNewest;
        case xlink2::LimitType::OldestThenSpatialPriority: return LimitType::OldestThenSpatialPriority;
        case xlink2::LimitType::NewestThenSpatialPriority: return LimitType::NewestThenSpatialPriority;
        default: common::AbortWithDetail("Unknown arrange group limit type: {:#x}", std::to_underlying(type));
    }
}

[[nodiscard]] static auto LoadArrangeParam(LoadContext& ctx) -> std::unique_ptr<ArrangeParam> {
    const auto count = ctx.reader.read<std::int32_t>();
    if (count < 1) {
        return std::make_unique<ArrangeParam>();
    }
    auto param = std::make_unique<ArrangeParam>(count);
    for (auto& group : *param) {
        group.setName(ctx.getString(ctx.readPtr()));
        group.setLimitType(ConvertLimitType(ctx.reader.read<xlink2::LimitType>()));
        group.setThreshold(ctx.reader.read<std::int8_t>());
        group.setIncludeFading(ctx.reader.read<bool>());
        if (ctx.is64Bit()) {
            ctx.reader.skip(5);
        } else {
            ctx.reader.skip(1);
        }
    }
    return param;
}

static auto LoadParamDefine(Param& param, LoadContext& ctx, const std::unordered_map<std::uint64_t, std::string_view>& strings) -> void {
    const auto nameOffset = ctx.readPtr();
    {
        const auto res = strings.find(nameOffset);
        if (res == strings.end()) {
            common::AbortWithDetail("Invalid PDT string offset @ {:#x}: {:#x}", ctx.reader.tell(), nameOffset);
        }
        param.setName(res->second);
    }

    const auto type = ctx.reader.read<xlink2::ParamType>();
    ctx.alignToPtr();
    switch (type) {
        case xlink2::ParamType::Int:
            if (ctx.is64Bit()) {
                param.setTypeAndValue(ParamType::S32, static_cast<std::int32_t>(ctx.reader.read<std::int64_t>()));
            } else {
                param.setTypeAndValue(ParamType::S32, ctx.reader.read<std::int32_t>());
            }
            break;
        case xlink2::ParamType::Float:
            if (ctx.is64Bit()) {
                param.setTypeAndValue(ParamType::F32, static_cast<float>(ctx.reader.read<double>()));
            } else {
                param.setTypeAndValue(ParamType::F32, ctx.reader.read<float>());
            }
            break;
        case xlink2::ParamType::Bool:
            param.setTypeAndValue(ParamType::Bool, ctx.readPtr() != 0);
            break;
        case xlink2::ParamType::Enum:
            param.setTypeAndValue(ParamType::Enum, EnumValue(static_cast<std::uint32_t>(ctx.readPtr())));
            break;
        case xlink2::ParamType::String: {
            const auto offset = ctx.readPtr();
            const auto res = strings.find(offset);
            if (res == strings.end()) {
                common::AbortWithDetail("Invalid PDT string offset @ {:#x}: {:#x}", ctx.reader.tell(), offset);
            }
            param.setTypeAndValue(ParamType::String, std::make_unique<std::string>(res->second));
            break;
        }
        case xlink2::ParamType::Custom: {
            const auto value = ctx.readPtr();
            if (value != 0) {
                common::AbortWithDetail("ParamDefines for custom types should have a zeroed default value! {:#x}", ctx.reader.tell());
            }
            param.setType(ParamType::Custom);
            break;
        }
        default:
            common::AbortWithDetail("Unknown param type for param define @ {:#x}! {:#x}", ctx.reader.tell(), std::to_underlying(type));
    }
}

auto ParamDefineTable::load(LoadContext& ctx) -> void {
    reset();

    const auto base = ctx.reader.tell();
    
    const auto dataSize = ctx.reader.read<std::uint32_t>();
    const auto numUserParam = ctx.reader.read<std::uint32_t>();
    const auto numAssetParam = ctx.reader.read<std::uint32_t>();
    const auto numCustomAssetParam = ctx.reader.read<std::uint32_t>();
    const auto numTriggerParam = ctx.reader.read<std::uint32_t>();
    
    ctx.assetParamPos = common::AlignUp(base + dataSize, static_cast<std::size_t>(ctx.is64Bit() ? 8 : 4));
    
    const auto tableEnd = ctx.reader.tell() + dataSize;
    if (tableEnd > ctx.reader.size()) {
        common::AbortWithDetail(
            "Reported ParamDefineTable size is larger than provided buffer @ {:#x}! {:#x} vs. {:#x}",
            ctx.reader.tell(), dataSize, ctx.reader.size() - ctx.reader.tell()
        );
    }

    if (numUserParam < ctx.numSystemUserParam) {
        common::AbortWithDetail(
            "ParamDefineTable has fewer total user params than system user params for the current profile @ {:#x}! {} vs. {}",
            ctx.reader.tell(), numUserParam, ctx.numSystemUserParam
        );
    }
    
    if (numAssetParam < numCustomAssetParam) {
        common::AbortWithDetail("ParamDefineTable has fewer total asset params than user asset params @ {:#x}! {} vs. {}", ctx.reader.tell(), numAssetParam, numCustomAssetParam);
    }

    ctx.alignToPtr();
    const auto paramStart = ctx.reader.tell();
    const auto paramDefineSize = SizeOf<xlink2::ResParamDefine>(ctx.game);
    const auto nameTablePos = paramStart + (numUserParam + numAssetParam + numTriggerParam) * paramDefineSize;

    ctx.reader.seek(nameTablePos);
    auto nameTable = std::unordered_map<std::uint64_t, std::string_view>();
    while (ctx.reader.tell() < tableEnd) {
        const auto pos = ctx.reader.tell() - nameTablePos;
        const auto str = reinterpret_cast<const char*>(ctx.reader.data() + ctx.reader.tell());
        const auto len = strnlen(str, tableEnd - ctx.reader.tell());
        nameTable.emplace(pos, std::string_view{ str, len });
        ctx.reader.skip(len + 1);
    }

    ctx.reader.seek(paramStart);

    mSystemUserParams.resize(ctx.numSystemUserParam);
    mCustomUserParams.resize(numUserParam - ctx.numSystemUserParam);
    mSystemAssetParams.resize(numAssetParam - numCustomAssetParam);
    mCustomAssetParams.resize(numCustomAssetParam);
    mTriggerParams.resize(numTriggerParam);

    for (auto& param : mSystemUserParams) {
        LoadParamDefine(param, ctx, nameTable);
    }

    for (auto& param : mCustomUserParams) {
        LoadParamDefine(param, ctx, nameTable);
    }

    for (auto& param : mSystemAssetParams) {
        LoadParamDefine(param, ctx, nameTable);
    }

    for (auto& param : mCustomAssetParams) {
        LoadParamDefine(param, ctx, nameTable);
    }

    for (auto& param : mTriggerParams) {
        LoadParamDefine(param, ctx, nameTable);
    }
}

static constexpr auto ConvertCurveType(xlink2::CurveType type) -> CurveType {
    switch (type) {
        case xlink2::CurveType::Standard: return CurveType::Standard;
        case xlink2::CurveType::Constant: return CurveType::Constant;
        default: common::AbortWithDetail("Unknown curve type: {:#x}", std::to_underlying(type));
    }
};

static auto ReadRandom(Param& param, LoadContext& ctx, xlink2::ReferenceType refType) -> void {
    const auto min = ctx.reader.read<float>();
    const auto max = ctx.reader.read<float>();
    switch (refType) {
        case xlink2::ReferenceType::RandomLinear:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::Linear, min, max));
            break;
        case xlink2::ReferenceType::RandomInflectedPolynomial2:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::InflectedPolynomial, 2.f, min, max));
            break;
        case xlink2::ReferenceType::RandomInflectedPolynomial3:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::InflectedPolynomial, 3.f, min, max));
            break;
        case xlink2::ReferenceType::RandomInflectedPolynomial4:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::InflectedPolynomial, 4.f, min, max));
            break;
        case xlink2::ReferenceType::RandomInflectedPolynomial1Point5:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::InflectedPolynomial, 1.5f, min, max));
            break;
        case xlink2::ReferenceType::RandomIncreasingPolynomial2:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::IncreasingPolynomial, 2.f, min, max));
            break;
        case xlink2::ReferenceType::RandomIncreasingPolynomial3:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::IncreasingPolynomial, 3.f, min, max));
            break;
        case xlink2::ReferenceType::RandomIncreasingPolynomial4:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::IncreasingPolynomial, 4.f, min, max));
            break;
        case xlink2::ReferenceType::RandomIncreasingPolynomial1Point5:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::IncreasingPolynomial, 1.5f, min, max));
            break;
        case xlink2::ReferenceType::RandomDecreasingPolynomial2:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::DecreasingPolynomial, 2.f, min, max));
            break;
        case xlink2::ReferenceType::RandomDecreasingPolynomial3:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::DecreasingPolynomial, 3.f, min, max));
            break;
        case xlink2::ReferenceType::RandomDecreasingPolynomial4:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::DecreasingPolynomial, 4.f, min, max));
            break;
        case xlink2::ReferenceType::RandomDecreasingPolynomial1Point5:
            param.setTypeAndValue(ParamType::F32, std::make_unique<RandomTable>(RandomType::DecreasingPolynomial, 1.5f, min, max));
            break;
        default:
            common::AbortWithDetail("Invalid random type");
    }
}

static auto ResolveParam(Param& param, LoadContext& ctx, const Param& def) -> void {
    const auto packed = ctx.reader.read<std::uint32_t>();
    const auto value = packed & 0xffffffu;
    const auto refType = static_cast<xlink2::ReferenceType>(packed >> 0x18);
    param.setName(def.getName());
    switch (refType) {
        case xlink2::ReferenceType::Direct: {
            switch (def.getParamType()) {
                case ParamType::S32:
                    param.setTypeAndValue(ParamType::S32, ctx.getDirectValueS32(value));
                    break;
                case ParamType::F32:
                    param.setTypeAndValue(ParamType::F32, ctx.getDirectValueF32(value));
                    break;
                case ParamType::Bool:
                    param.setTypeAndValue(ParamType::Bool, ctx.getDirectValueU32(value) != 0);
                    break;
                case ParamType::Enum:
                    param.setTypeAndValue(ParamType::Enum, EnumValue(ctx.getDirectValueU32(value)));
                    break;
                default:
                    common::AbortWithDetail(
                        "Invalid param type for direct value reference @ {:#x}: {} - {:#x}",
                        ctx.reader.tell(), def.getName(), std::to_underlying(def.getParamType())
                    );
            }
            break;
        }
        case xlink2::ReferenceType::String:
            if (def.getParamType() != ParamType::String) {
                common::AbortWithDetail(
                    "String can only be used with string params @ {:#x}! {} - {:#x}",
                    ctx.reader.tell(), def.getName(), std::to_underlying(def.getParamType())
                );
            }
            param.setTypeAndValue(ParamType::String, std::make_unique<std::string>(ctx.getString(value)));
            break;
        case xlink2::ReferenceType::Curve: {
            if (def.getParamType() != ParamType::F32) {
                common::AbortWithDetail(
                    "Curve can only be used with F32 params @ {:#x}! {} - {:#x}",
                    ctx.reader.tell(), def.getName(), std::to_underlying(def.getParamType())
                );
            }
            if (value >= ctx.numCurveTable) {
                common::AbortWithDetail("Out-of-range curve @ {:#x}!: {} (max: {})", ctx.reader.tell(), value, ctx.numCurveTable);
            }
            auto curve = std::make_unique<Curve>();
            const auto scope = ctx.reader.createScope(ctx.curveTablePos + SizeOf<xlink2::ResCurve>(ctx.game) * value);
            const auto baseIdx = ctx.reader.read<std::uint16_t>();
            const auto pointCount = ctx.reader.read<std::uint16_t>();
            curve->setType(ConvertCurveType(ctx.reader.read<xlink2::CurveType>()));
            curve->setPropertyScope(ctx.reader.read<std::uint16_t>() == 1 ? PropertyScope::Global : PropertyScope::Local);
            curve->setPropertyName(ctx.getString(ctx.readPtr()));
            curve->setUnknown1(ctx.reader.read<std::int32_t>());
            if (curve->getPropertyScope() == PropertyScope::Local && ctx.getLocalPropertyName(ctx.reader.read<std::int16_t>()) != curve->getPropertyName()) {
                common::AbortWithDetail(
                    "Curve property name does not match selected local property @ {:#x}!: \"{}\" vs. \"{}\"",
                    ctx.reader.tell(), curve->getPropertyName(), ctx.getLocalPropertyName(ctx.reader.read<std::int16_t>())
                );
            }
            curve->setUnknown2(ctx.reader.read<std::int16_t>());
            if (baseIdx + pointCount > ctx.numCurvePointTable) {
                common::AbortWithDetail("Out-of-range curve point @ {:#x}!: {} + {} (max: {})", ctx.reader.tell(), baseIdx, pointCount, ctx.numCurvePointTable);
            }
            ctx.reader.seek(ctx.curvePointTablePos + SizeOf<xlink2::ResCurvePoint>(ctx.game) * baseIdx);
            for (const auto _ : std::views::iota(0u, pointCount)) {
                const auto x = ctx.reader.read<float>();
                const auto y = ctx.reader.read<float>();
                curve->addPoint(x, y);
            }
            param.setTypeAndValue(ParamType::F32, std::move(curve));
            break;
        }
        case xlink2::ReferenceType::RandomLinear: {
            if (def.getParamType() != ParamType::F32) {
                common::AbortWithDetail(
                    "Random can only be used with F32 params @ {:#x}! {} - {:#x}",
                    ctx.reader.tell(), def.getName(), std::to_underlying(def.getParamType())
                );
            }
            if (value >= ctx.numRandomTable) {
                common::AbortWithDetail("Out-of-range random @ {:#x}!: {} (max: {})", ctx.reader.tell(), value, ctx.numRandomTable);
            }
            const auto scope = ctx.reader.createScope(ctx.randomTablePos + SizeOf<xlink2::ResRandom>(ctx.game) * value);
            ReadRandom(param, ctx, refType);
            break;
        }
        case xlink2::ReferenceType::ArrangeParam: {
            if (def.getParamType() != ParamType::Custom) {
                common::AbortWithDetail(
                    "ArrangeParam can only be used with custom params @ {:#x}! {} - {:#x}",
                    ctx.reader.tell(), def.getName(), std::to_underlying(def.getParamType())
                );
            }
            const auto scope = ctx.reader.createScope(ctx.exRegionPos + value);
            param.setTypeAndValue(ParamType::Custom, LoadArrangeParam(ctx));
            break;
        }
        case xlink2::ReferenceType::Bitfield:
            if (def.getParamType() != ParamType::S32) {
                common::AbortWithDetail(
                    "Bitfield can only be used with S32 params @ {:#x}! {} - {:#x}",
                    ctx.reader.tell(), def.getName(), std::to_underlying(def.getParamType())
                );
            }
            param.setTypeAndValue(ParamType::S32, BitfieldValue(ctx.getDirectValueU32(value)));
            break;
        case xlink2::ReferenceType::RandomInflectedPolynomial2:
        case xlink2::ReferenceType::RandomInflectedPolynomial3:
        case xlink2::ReferenceType::RandomInflectedPolynomial4:
        case xlink2::ReferenceType::RandomInflectedPolynomial1Point5:
        case xlink2::ReferenceType::RandomIncreasingPolynomial2:
        case xlink2::ReferenceType::RandomIncreasingPolynomial3:
        case xlink2::ReferenceType::RandomIncreasingPolynomial4:
        case xlink2::ReferenceType::RandomIncreasingPolynomial1Point5:
        case xlink2::ReferenceType::RandomDecreasingPolynomial2:
        case xlink2::ReferenceType::RandomDecreasingPolynomial3:
        case xlink2::ReferenceType::RandomDecreasingPolynomial4:
        case xlink2::ReferenceType::RandomDecreasingPolynomial1Point5: {
            if (def.getParamType() != ParamType::F32) {
                common::AbortWithDetail(
                    "Random can only be used with F32 params @ {:#x}! {} - {:#x}",
                    ctx.reader.tell(), def.getName(), std::to_underlying(def.getParamType())
                );
            }
            if (value >= ctx.numRandomTable) {
                common::AbortWithDetail("Out-of-range random @ {:#x}!: {} (max: {})", ctx.reader.tell(), value, ctx.numRandomTable);
            }
            const auto scope = ctx.reader.createScope(ctx.randomTablePos + SizeOf<xlink2::ResRandom>(ctx.game) * value);
            ReadRandom(param, ctx, refType);
            break;
        }
        default:
            common::AbortWithDetail("Unknown reference type @ {:#x}: {:#x}", ctx.reader.tell(), std::to_underlying(refType));
    }
}

[[nodiscard]] static auto LoadContainerType(LoadContext& ctx) -> CallTableType {
    switch (GetResContainerParamLayout(ctx.game)) {
        case 0: {
            switch (const auto type = ctx.reader.read<std::uint32_t>()) {
                case 0:
                    return CallTableType::Switch;
                case 1:
                    return CallTableType::Random;
                case 2:
                    return CallTableType::RandomNoRepeat;
                case 3:
                    return CallTableType::Blend;
                case 4:
                    return CallTableType::Sequence;
                case 5:
                    if (SupportsGridContainers(ctx.game)) {
                        return CallTableType::Grid;
                    }
                    common::AbortWithDetail("Grid containers are not supported on this version");
                case 6:
                    if (SupportsJumpContainers(ctx.game)) {
                        return CallTableType::Jump;
                    }
                    common::AbortWithDetail("Jump containers are not supported on this version");
                default:
                    common::AbortWithDetail("Unknown container type @ {:#x}!: {:#x}", ctx.reader.tell(), type);
            }
        }
        case 1: {
            const auto type = ctx.reader.read<std::uint8_t>();
            const auto isBlendBy = ctx.reader.read<bool>();
            ctx.reader.skip(2); // isNeedObserve set at runtime and padding
            switch (type) {
                case 0:
                    return CallTableType::Switch;
                case 1:
                    return CallTableType::Random;
                case 2:
                    return CallTableType::RandomNoRepeat;
                case 3:
                    return isBlendBy ? CallTableType::BlendBy : CallTableType::Blend;
                case 4:
                    return CallTableType::Sequence;
                case 5:
                    if (SupportsGridContainers(ctx.game)) {
                        return CallTableType::Grid;
                    }
                    common::AbortWithDetail("Grid containers are not supported on this version");
                case 6:
                    if (SupportsJumpContainers(ctx.game)) {
                        return CallTableType::Jump;
                    }
                    common::AbortWithDetail("Jump containers are not supported on this version");
                default:
                    common::AbortWithDetail("Unknown container type @ {:#x}!: {:#x}", ctx.reader.tell(), type);
            }
        }
        default:
            common::AbortWithDetail("Unsupported container type layout");
    }
}

[[nodiscard]] static auto ResolveGridPropertyEnumNames(const std::vector<std::uint32_t>& values, PropertyScope scope, const LoadContext& ctx) -> std::vector<std::string> {
    auto out = std::vector<std::string>{};
    for (const auto value : values) {
        if (scope == PropertyScope::Global) {
            out.emplace_back(ctx.getString(value));
        } else {
            out.emplace_back(ctx.getLocalPropertyEnumName(value));
        }
    }
    return out;
}

static auto ResolveGridChildCallTables(
    const AssetCallTableHandle& act,
    const std::vector<std::int32_t>& values,
    std::int32_t childStart,
    std::int32_t childEnd,
    const LoadContext& ctx,
    std::vector<AssetCallTableHandle>& assetCallTables
) -> std::vector<AssetCallTableHandle> {
    auto out = std::vector<AssetCallTableHandle>{};
    for (const auto value : values) {
        if (value < 0) {
            out.emplace_back(std::shared_ptr<AssetCallTable>()); // null case
            continue;
        }
        if (value < childStart || value > childEnd) {
            common::AbortWithDetail("Grid child index out of range @ {:#x}! {} ({} - {})", ctx.reader.tell(), value, childStart, childEnd);
        }
        auto& child = assetCallTables.at(value);
        if (child->getParent()->getGUID() != act->getGUID()) {
            common::AbortWithDetail("Child doesn't belong to grid container");
        }
        out.emplace_back(child);
    }
    return out;
}

static auto LoadContainer(
    CallTableInfo& info,
    LoadContext& ctx
) -> AssetCallTableHandle {
    const auto type = LoadContainerType(ctx);
    info.childStart = ctx.reader.read<std::int32_t>();
    info.childEnd = ctx.reader.read<std::int32_t>();
    if (ctx.is64Bit()) {
        ctx.reader.skip(4); // padding
    }
    switch (type) {
        case CallTableType::Switch: {
            auto switch_ = std::make_shared<Switch>();
            const auto varNameOffset = ctx.readPtr();
            switch_->setSwitchVariable(ctx.getString(varNameOffset));
            switch_->setUnknown(ctx.reader.read<std::int32_t>());
            const auto propIndex = ctx.reader.read<std::int16_t>();
            const auto isGlobal = ctx.reader.read<bool>();
            const auto isAction = ctx.reader.read<bool>();
            if (isAction) {
                if (!SupportsActionSwitch(ctx.game)) {
                    common::AbortWithDetail("Action switch statement is not supported on this version!");
                }
                switch_->setSwitchType(SwitchType::ActionSlot);
            } else if (isGlobal) {
                switch_->setSwitchType(SwitchType::GlobalProperty);
            } else if (propIndex >= 0) {
                switch_->setSwitchType(SwitchType::LocalProperty);
                if (ctx.getLocalPropertyName(propIndex) != switch_->getSwitchVariable()) {
                    common::AbortWithDetail(
                        "Property index does not match provided property name for switch container @ {:#x}! {} vs. {}",
                        ctx.reader.tell(), ctx.getLocalPropertyName(propIndex), switch_->getSwitchVariable()
                    );
                }
            } else {
                switch_->setSwitchType(SwitchType::Null);
            }
            return AssetCallTableHandle(std::move(switch_));
        }
        case CallTableType::Random: {
            return AssetCallTableHandle(std::make_shared<Random>());
        }
        case CallTableType::RandomNoRepeat: {
            return AssetCallTableHandle(std::make_unique<RandomNoRepeat>());
        }
        case CallTableType::Blend: {
            return AssetCallTableHandle(std::make_unique<Blend>());
        }
        case CallTableType::BlendBy: {
            auto blend = std::make_unique<BlendBy>();
            const auto varNameOffset = ctx.readPtr();
            blend->setBlendPropertyName(ctx.getString(varNameOffset));
            blend->setUnknown(ctx.reader.read<std::int32_t>());
            const auto propIndex = ctx.reader.read<std::int16_t>();
            const auto isGlobal = ctx.reader.read<bool>();
            const auto isAction = ctx.reader.read<bool>();
            if (isAction) {
                common::AbortWithDetail("Blend container does not support action matching @ {:#x}!", ctx.reader.tell());
            } else if (isGlobal) {
                blend->setBlendPropertyScope(PropertyScope::Global);
            } else {
                blend->setBlendPropertyScope(PropertyScope::Local);
                if (propIndex < 0) {
                    common::AbortWithDetail("Invalid property index for blend container @ {:#x}! {}", ctx.reader.tell(), propIndex);
                }
                if (ctx.getLocalPropertyName(propIndex) != blend->getBlendPropertyName()) {
                    common::AbortWithDetail(
                        "Property index does not match provided property name for blend container @ {:#x}! {} vs. {}",
                        ctx.reader.tell(), ctx.getLocalPropertyName(propIndex), blend->getBlendPropertyName()
                    );
                }
            }
            return AssetCallTableHandle(std::move(blend));
        }
        case CallTableType::Sequence: {
            return AssetCallTableHandle(std::make_unique<Sequence>());
        }
        case CallTableType::Grid: {
            auto grid = std::make_unique<Grid>();
            const auto prop1NameOffset = ctx.readPtr();
            const auto prop2NameOffset = ctx.readPtr();
            const auto prop1Index = ctx.reader.read<std::int16_t>();
            const auto prop2Index = ctx.reader.read<std::int16_t>();
            const auto flags = ctx.reader.read<std::uint16_t>();
            const auto prop1Count = ctx.reader.read<std::uint8_t>();
            const auto prop2Count = ctx.reader.read<std::uint8_t>();
            grid->setProperty1Name(ctx.getString(prop1NameOffset));
            grid->setProperty2Name(ctx.getString(prop2NameOffset));
            grid->setProperty1Scope(flags & 1 ? PropertyScope::Global : PropertyScope::Local);
            grid->setProperty2Scope(flags >> 1 & 1 ? PropertyScope::Global : PropertyScope::Local);
            if (grid->getProperty1Scope() == PropertyScope::Local) {
                if (prop1Index < 0) {
                    common::AbortWithDetail("Invalid property 1 index for grid container @ {:#x}! {}", ctx.reader.tell(), prop1Index);
                }
                if (ctx.getLocalPropertyName(prop1Index) != grid->getProperty1Name()) {
                    common::AbortWithDetail(
                        "Grid container property 1 name does not match property index @ {:#x}! {} vs {}",
                        ctx.reader.tell(), ctx.getLocalPropertyName(prop1Index), grid->getProperty1Name()
                    );
                }
            }
            if (grid->getProperty2Scope() == PropertyScope::Local) {
                if (prop2Index < 0) {
                    common::AbortWithDetail("Invalid property 2 index for grid container @ {:#x}! {}", ctx.reader.tell(), prop2Index);
                }
                if (ctx.getLocalPropertyName(prop2Index) != grid->getProperty2Name()) {
                    common::AbortWithDetail(
                        "Grid container property 2 name does not match property index @ {:#x}! {} vs {}",
                        ctx.reader.tell(), ctx.getLocalPropertyName(prop2Index), grid->getProperty2Name()
                    );
                }
            }
            const auto values1 = ctx.reader.readArray<std::uint32_t>(prop1Count);
            const auto values2 = ctx.reader.readArray<std::uint32_t>(prop2Count);
            info.childIndices = ctx.reader.readArray<std::int32_t>(prop1Count * prop2Count);
            grid->setValuesAndAssetCallTables(
                ResolveGridPropertyEnumNames(values1, grid->getProperty1Scope(), ctx),
                ResolveGridPropertyEnumNames(values2, grid->getProperty2Scope(), ctx),
                {}
            );
            return AssetCallTableHandle(std::move(grid));
        }
        case CallTableType::Jump: {
            return AssetCallTableHandle(std::make_unique<Jump>());
        }
        default:
            std::unreachable();
    }
}

[[nodiscard]] static auto ConvertPropertyType(xlink2::PropertyType type) -> PropertyType {
    switch (type) {
        case xlink2::PropertyType::Enum: return PropertyType::Enum;
        case xlink2::PropertyType::S32: return PropertyType::S32;
        case xlink2::PropertyType::F32: return PropertyType::F32;
        case xlink2::PropertyType::Bool: return PropertyType::Bool;
        case xlink2::PropertyType::_04: return PropertyType::S32;
        case xlink2::PropertyType::_05: return PropertyType::F32;
        default: common::AbortWithDetail("Unknown property type: {:#x}", std::to_underlying(type));
    }
};

[[nodiscard]] static auto ConvertCompareType(xlink2::CompareType type) -> CompareType {
    switch (type) {
        case xlink2::CompareType::Equal: return CompareType::Equal;
        case xlink2::CompareType::GreaterThan: return CompareType::GreaterThan;
        case xlink2::CompareType::GreaterThanOrEqual: return CompareType::GreaterThanOrEqual;
        case xlink2::CompareType::LessThan: return CompareType::LessThan;
        case xlink2::CompareType::LessThanOrEqual: return CompareType::LessThanOrEqual;
        case xlink2::CompareType::NotEqual: return CompareType::NotEqual;
        default: common::AbortWithDetail("Unknown compare type: {:#x}", std::to_underlying(type));
    }
};

static auto LoadSwitchCondition(SwitchCondition& cond, LoadContext& ctx, SwitchType type) -> void {
    if (const auto ctype = ctx.reader.read<std::uint32_t>(); ctype != 0) {
        common::AbortWithDetail("Switch container child has invalid condition type @ {:#x}! {}", ctx.reader.tell(), ctype);
    }
    cond.setParentType(type);
    switch (GetResSwitchConditionLayout(ctx.game)) {
        case 0: {
            const auto propType = ConvertPropertyType(static_cast<xlink2::PropertyType>(ctx.reader.read<std::uint32_t>()));
            cond.setCompareType(ConvertCompareType(static_cast<xlink2::CompareType>(ctx.reader.read<std::uint32_t>())));
            switch (propType) {
                case PropertyType::Enum:
                    if (type == SwitchType::GlobalProperty) {
                        cond.setValue(std::string(ctx.getString(ctx.reader.read<std::uint32_t>())));
                        ctx.reader.skip(sizeof(std::uint16_t));
                    } else {
                        ctx.reader.skip(sizeof(std::uint32_t));
                        cond.setValue(std::string(ctx.getLocalPropertyEnumName(ctx.reader.read<std::uint16_t>())));
                    }
                    break;
                case PropertyType::S32:
                    cond.setValue(ctx.reader.read<std::int32_t>());
                    ctx.reader.skip(sizeof(std::uint16_t));
                    break;
                case PropertyType::F32:
                    cond.setValue(ctx.reader.read<float>());
                    ctx.reader.skip(sizeof(std::uint16_t));
                    break;
                case PropertyType::Bool:
                    cond.setValue(ctx.reader.read<std::uint32_t>() != 0);
                    ctx.reader.skip(sizeof(std::uint16_t));
                    break;
            }
            ctx.reader.skip(sizeof(bool)); // isSolved
            if (ctx.reader.read<bool>() != (type == SwitchType::GlobalProperty)) {
                common::AbortWithDetail("Switch child and container are not the same scope @ {:#x}!", ctx.reader.tell());
            }
            break;
        }
        case 1: {
            const auto propType = ConvertPropertyType(static_cast<xlink2::PropertyType>(ctx.reader.read<std::uint8_t>()));
            cond.setCompareType(ConvertCompareType(static_cast<xlink2::CompareType>(ctx.reader.read<std::uint8_t>())));
            ctx.reader.skip(sizeof(bool)); // isSolved
            if (ctx.reader.read<bool>() != (type == SwitchType::GlobalProperty)) {
                common::AbortWithDetail("Switch child and container are not the same scope @ {:#x}!", ctx.reader.tell());
            }
            switch (propType) {
                case PropertyType::Enum:
                    switch (type) {
                        case SwitchType::GlobalProperty: {
                            cond.setValue(std::string(ctx.getString(ctx.readPtr())));
                            ctx.reader.skip(sizeof(std::uint32_t));
                            break;
                        }
                        case SwitchType::LocalProperty: {
                            ctx.reader.skip(sizeof(std::uint32_t));
                            cond.setValue(std::string(ctx.getLocalPropertyEnumName(ctx.reader.read<std::uint32_t>())));
                            break;
                        }
                        default:
                            common::AbortWithDetail("Null switch containers should not have conditions");
                    }
                    break;
                case PropertyType::S32:
                    cond.setValue(ctx.reader.read<std::int32_t>());
                    ctx.reader.skip(sizeof(std::uint32_t));
                    break;
                case PropertyType::F32:
                    cond.setValue(ctx.reader.read<float>());
                    ctx.reader.skip(sizeof(std::uint32_t));
                    break;
                case PropertyType::Bool:
                    cond.setValue(ctx.reader.read<std::uint32_t>() != 0);
                    ctx.reader.skip(sizeof(std::uint32_t));
                    break;
            }
            break;
        }
        case 2: {
            const auto propType = ConvertPropertyType(static_cast<xlink2::PropertyType>(ctx.reader.read<std::uint8_t>()));
            cond.setCompareType(ConvertCompareType(static_cast<xlink2::CompareType>(ctx.reader.read<std::uint8_t>())));
            ctx.reader.skip(sizeof(bool)); // isSolved
            if (ctx.reader.read<bool>() != (type == SwitchType::GlobalProperty)) {
                common::AbortWithDetail("Switch child and container are not the same scope @ {:#x}!", ctx.reader.tell());
            }
            switch (propType) {
                case PropertyType::Enum:
                    switch (type) {
                        case SwitchType::ActionSlot: {
                            const auto hash = ctx.reader.read<std::uint32_t>();
                            ctx.reader.skip(sizeof(std::uint32_t));
                            const auto actionName = ctx.getString(ctx.readPtr());
                            if (common::CalcCRC32(actionName) != hash) {
                                common::AbortWithDetail("Action hash and action name don't match! {:#x} {}", hash, actionName);
                            }
                            cond.setValue(std::string(actionName));
                            break;
                        }
                        case SwitchType::GlobalProperty: {
                            ctx.reader.skip(sizeof(std::uint32_t));
                            ctx.reader.skip(sizeof(std::uint32_t));
                            cond.setValue(std::string(ctx.getString(ctx.readPtr())));
                            break;
                        }
                        case SwitchType::LocalProperty: {
                            cond.setValue(std::string(ctx.getLocalPropertyEnumName(ctx.reader.read<std::uint32_t>())));
                            ctx.reader.skip(sizeof(std::uint32_t));
                            if (const auto name = ctx.getString(ctx.readPtr()); std::get<std::string>(cond.getValue()) != name) {
                                common::AbortWithDetail(
                                    "Switch child enum name mismatch @ {:#x}: {} vs {}",
                                    ctx.reader.tell(), std::get<std::string>(cond.getValue()), name
                                );
                            }
                            break;
                        }
                        default:
                            common::AbortWithDetail("Null switch containers should not have conditions");
                    }
                    break;
                case PropertyType::S32:
                    ctx.reader.skip(sizeof(std::uint32_t));
                    cond.setValue(ctx.reader.read<std::int32_t>());
                    break;
                case PropertyType::F32:
                    ctx.reader.skip(sizeof(std::uint32_t));
                    cond.setValue(ctx.reader.read<float>());
                    break;
                case PropertyType::Bool:
                    ctx.reader.skip(sizeof(std::uint32_t));
                    cond.setValue(ctx.reader.read<std::uint32_t>() != 0);
                    break;
            }
            break;
        }
        default:
            common::AbortWithDetail("Unsupported switch condition format");
    }
}

static auto LoadAssetCallTable(
    CallTableInfo& info,
    LoadContext& ctx,
    const ParamDefineTable& pdt
) -> AssetCallTableHandle {
    const auto keyName = ctx.getString(ctx.readPtr());
    ctx.reader.skip(sizeof(std::int16_t)); // asset index
    const auto flags = common::BitFlag<xlink2::CallTableFlags>(ctx.reader.read<std::uint16_t>());
    const auto emitCount = ctx.reader.read<std::int32_t>();
    [[maybe_unused]] const auto parentIndex = ctx.reader.read<std::int32_t>();
    const auto guid = ctx.reader.read<std::uint32_t>();
    ctx.reader.skip(sizeof(std::uint32_t)); // key hash
    if (ctx.is64Bit()) {
        ctx.reader.skip(4);
    }
    const auto paramOffset = ctx.readPtr();
    info.conditionOffset = ctx.readPtr();

    AssetCallTableHandle act;
    if (flags.isOn(xlink2::CallTableFlags::IsContainer)) {
        const auto scope = ctx.reader.createScope(ctx.containerTablePos + paramOffset);
        act = LoadContainer(info, ctx);
    } else {
        auto asset = std::make_shared<Asset>();
        const auto scope = ctx.reader.createScope(ctx.assetParamPos + paramOffset);
        const auto mask = ctx.reader.read<std::uint64_t>();
        for (const auto i : std::views::iota(0u, pdt.getNumAssetParams())) {
            if ((mask >> i & 1) == 0) {
                continue;
            }
            ResolveParam(asset->getParams().emplace_back(), ctx, pdt.getAssetParam(i));
        }
        act = AssetCallTableHandle(asset);
    }

    act->setKeyName(keyName);
    act->setOneShot(flags.isOn(xlink2::CallTableFlags::OneShot));
    act->setNoPause(flags.isOn(xlink2::CallTableFlags::NoPause));
    act->setEmitCount(emitCount);
    act->setGUID(guid);
    return act;
}

static auto ResolveSwitchCondition(
    Switch* act,
    AssetCallTableHandle& child,
    LoadContext& ctx,
    const std::unordered_map<std::uint32_t, CallTableInfo>& callTableInfo
) -> void {
    child->setCondition(std::make_unique<SwitchCondition>());
    auto switchCond = static_cast<SwitchCondition*>(child->getCondition().get());
    const auto offset = callTableInfo.at(child->getGUID()).conditionOffset;
    if (offset == 0xffffffff) {
        switchCond->setCompareType(CompareType::Default);
        return;
    }
    const auto scope = ctx.reader.createScope(ctx.conditionTablePos + offset);
    LoadSwitchCondition(*switchCond, ctx, act->getSwitchType());
}

static auto ResolveRandomCondition(
    Random* act,
    AssetCallTableHandle& child,
    LoadContext& ctx,
    const std::unordered_map<std::uint32_t, CallTableInfo>& callTableInfo
) -> void {
    child->setCondition(std::make_unique<RandomCondition>());
    const auto offset = callTableInfo.at(child->getGUID()).conditionOffset;
    if (offset == 0xffffffff) {
        common::AbortWithDetail("Random container child missing condition! {} -> {}", act->getKeyName(), child->getKeyName());
    }
    const auto scope = ctx.reader.createScope(ctx.conditionTablePos + offset);
    if (const auto type = ctx.reader.read<std::uint32_t>(); type != 1 && type != 2) { // why do they do this
        common::AbortWithDetail("Random container child has invalid condition type @ {:#x}! {}", ctx.reader.tell(), type);
    }
    static_cast<RandomCondition*>(child->getCondition().get())->setWeight(ctx.reader.read<float>());
}

static auto ResolveRandomNoRepeatCondition(
    RandomNoRepeat* act,
    AssetCallTableHandle& child,
    LoadContext& ctx,
    const std::unordered_map<std::uint32_t, CallTableInfo>& callTableInfo
) -> void {
    child->setCondition(std::make_unique<RandomNoRepeatCondition>());
    const auto offset = callTableInfo.at(child->getGUID()).conditionOffset;
    if (offset == 0xffffffff) {
        common::AbortWithDetail("RandomNoRepeat container child missing condition! {} -> {}", act->getKeyName(), child->getKeyName());
    }
    const auto scope = ctx.reader.createScope(ctx.conditionTablePos + offset);
    if (const auto type = ctx.reader.read<std::uint32_t>(); type != 1 && type != 2) { // why do they do this
        common::AbortWithDetail("RandomNoRepeat container child has invalid condition type @ {:#x}! {}", ctx.reader.tell(), type);
    }
    static_cast<RandomNoRepeatCondition*>(child->getCondition().get())->setWeight(ctx.reader.read<float>());
}

[[nodiscard]] static auto ConvertBlendType(xlink2::BlendType type) -> BlendOp {
    switch (type) {
        case xlink2::BlendType::None: return BlendOp::None;
        case xlink2::BlendType::Multiply: return BlendOp::Multiply;
        case xlink2::BlendType::SquareRoot: return BlendOp::SquareRoot;
        case xlink2::BlendType::Sin: return BlendOp::Sin;
        case xlink2::BlendType::Add: return BlendOp::Add;
        case xlink2::BlendType::SetToOne: return BlendOp::SetToOne;
        default: common::AbortWithDetail("Unknown blend type! {:#x}", std::to_underlying(type));
    }
}

static auto ResolveBlendByCondition(
    BlendBy* act,
    AssetCallTableHandle& child,
    LoadContext& ctx,
    const std::unordered_map<std::uint32_t, CallTableInfo>& callTableInfo
) -> void {
    child->setCondition(std::make_unique<BlendByCondition>());
    const auto offset = callTableInfo.at(child->getGUID()).conditionOffset;
    if (offset == 0xffffffff) {
        common::AbortWithDetail("BlendBy container child missing condition! {} -> {}", act->getKeyName(), child->getKeyName());
    }
    const auto scope = ctx.reader.createScope(ctx.conditionTablePos + offset);
    if (const auto type = ctx.reader.read<std::uint32_t>(); type != 3) {
        common::AbortWithDetail("BlendBy container child has invalid condition type @ {:#x}! {}", ctx.reader.tell(), type);
    }
    auto blendCond = static_cast<BlendByCondition*>(child->getCondition().get());
    blendCond->setValueMin(ctx.reader.read<float>());
    blendCond->setValueMax(ctx.reader.read<float>());
    blendCond->setBlendOpMin(ConvertBlendType(static_cast<xlink2::BlendType>(ctx.reader.read<std::uint8_t>())));
    blendCond->setBlendOpMax(ConvertBlendType(static_cast<xlink2::BlendType>(ctx.reader.read<std::uint8_t>())));
    ctx.reader.skip(2); // padding
}

static auto ResolveSequenceCondition(
    AssetCallTableHandle& child,
    LoadContext& ctx,
    const std::unordered_map<std::uint32_t, CallTableInfo>& callTableInfo
) -> void {
    const auto offset = callTableInfo.at(child->getGUID()).conditionOffset;
    if (offset == 0xffffffff) {
        return;
    }
    const auto scope = ctx.reader.createScope(ctx.conditionTablePos + offset);
    if (const auto type = ctx.reader.read<std::uint32_t>(); type != 4) {
        // for some reason, it seems like sequence container children can have non-sequence conditions?
        // afaik, a non-sequence condition does nothing though
        return;
    }
    child->setCondition(std::make_unique<SequenceCondition>());
    auto seqCond = static_cast<SequenceCondition*>(child->getCondition().get());
    seqCond->setContinueOnFade(ctx.reader.read<std::uint32_t>());
}

static auto SetOrVerifyParent(AssetCallTableHandle& child, AssetCallTableHandle& parent) -> void {
    if (child->getParent()) {
        if (child->getParent()->getGUID() != parent->getGUID()) {
            common::AbortWithDetail("Parent mismatch");
        }
    } else {
        child->setParent(parent);
    }
}

static auto ResolveAssetCallTable(
    AssetCallTableHandle& act,
    LoadContext& ctx,
    std::vector<AssetCallTableHandle>& assetCallTables,
    const std::unordered_map<std::uint32_t, CallTableInfo>& callTableInfo,
    std::unordered_set<std::uint32_t>& seen,
    std::unordered_set<std::uint32_t>& active
) -> void {
    if (active.contains(act->getGUID())) {
        common::AbortWithDetail("Circular reference detected! {}", act->getKeyName());
    }
    
    if (seen.contains(act->getGUID())) {
        return;
    }

    active.insert(act->getGUID());
    seen.insert(act->getGUID());

    const auto& info = callTableInfo.at(act->getGUID());
    if (info.childStart >= 0) {
        if (static_cast<std::size_t>(info.childStart) >= assetCallTables.size() || static_cast<std::size_t>(info.childEnd) >= assetCallTables.size()) {
            common::AbortWithDetail(
                "Out-of-range child call table indices @ {:#x}! {} + {} (max: {})",
                ctx.reader.tell(), info.childStart, info.childEnd, assetCallTables.size()
            );
        }
    } else {
        if (act->getType() != CallTableType::Asset && act->getType() != CallTableType::Jump) {
            common::AbortWithDetail("Negative child index! {}[{:#010x}]", act->getKeyName(), act->getGUID());
        }
    }
    switch (act->getType()) {
        case CallTableType::Switch: {
            for (const auto i : std::views::iota(info.childStart, info.childEnd + 1)) {
                auto& child = assetCallTables.at(i);
                SetOrVerifyParent(child, act);
                act->addChild(child);
                ResolveSwitchCondition(static_cast<Switch*>(act.get()), child, ctx, callTableInfo);
                ResolveAssetCallTable(child, ctx, assetCallTables, callTableInfo, seen, active);
            }
            break;
        }
        case CallTableType::Random: {
            for (const auto i : std::views::iota(info.childStart, info.childEnd + 1)) {
                auto& child = assetCallTables.at(i);
                SetOrVerifyParent(child, act);
                act->addChild(child);
                ResolveRandomCondition(static_cast<Random*>(act.get()), child, ctx, callTableInfo);
                ResolveAssetCallTable(child, ctx, assetCallTables, callTableInfo, seen, active);
            }
            break;
        }
        case CallTableType::RandomNoRepeat: {
            for (const auto i : std::views::iota(info.childStart, info.childEnd + 1)) {
                auto& child = assetCallTables.at(i);
                SetOrVerifyParent(child, act);
                act->addChild(child);
                ResolveRandomNoRepeatCondition(static_cast<RandomNoRepeat*>(act.get()), child, ctx, callTableInfo);
                ResolveAssetCallTable(child, ctx, assetCallTables, callTableInfo, seen, active);
            }
            break;
        }
        case CallTableType::Blend: {
            for (const auto i : std::views::iota(info.childStart, info.childEnd + 1)) {
                auto& child = assetCallTables.at(i);
                SetOrVerifyParent(child, act);
                act->addChild(child);
                ResolveAssetCallTable(child, ctx, assetCallTables, callTableInfo, seen, active);
            }
            break;
        }
        case CallTableType::BlendBy: {
            for (const auto i : std::views::iota(info.childStart, info.childEnd + 1)) {
                auto& child = assetCallTables.at(i);
                SetOrVerifyParent(child, act);
                act->addChild(child);
                ResolveBlendByCondition(static_cast<BlendBy*>(act.get()), child, ctx, callTableInfo);
                ResolveAssetCallTable(child, ctx, assetCallTables, callTableInfo, seen, active);
            }
            break;
        }
        case CallTableType::Sequence: {
            for (const auto i : std::views::iota(info.childStart, info.childEnd + 1)) {
                auto& child = assetCallTables.at(i);
                SetOrVerifyParent(child, act);
                act->addChild(child);
                ResolveSequenceCondition(child, ctx, callTableInfo);
                ResolveAssetCallTable(child, ctx, assetCallTables, callTableInfo, seen, active);
            }
            break;
        }
        case CallTableType::Grid: {
            for (const auto i : std::views::iota(info.childStart, info.childEnd + 1)) {
                auto& child = assetCallTables.at(i);
                SetOrVerifyParent(child, act);
                act->addChild(child);
                ResolveAssetCallTable(child, ctx, assetCallTables, callTableInfo, seen, active);
            }
            static_cast<Grid*>(act.get())->getAssetCallTables() = ResolveGridChildCallTables(
                act, info.childIndices, info.childStart, info.childEnd, ctx, assetCallTables
            );
            break;
        }
        case CallTableType::Jump: {
            if (info.childStart >= 0) {
                act->addChild(assetCallTables.at(info.childStart));
                ResolveAssetCallTable(assetCallTables.at(info.childStart), ctx, assetCallTables, callTableInfo, seen, active);
            }
            break;
        }
        case CallTableType::Asset:
            break;
        default:
            std::unreachable();
    }

    active.erase(act->getGUID());
}

template <typename T> requires requires (T& v) { v.addOverwriteParam(); }
static auto LoadTriggerOverwriteParam(T& trigger, LoadContext& ctx, const ParamDefineTable& pdt) -> void {
    const auto paramOffset = ctx.readPtr();
    if (paramOffset != 0xffff'ffff) {
        const auto scope = ctx.reader.createScope(ctx.triggerOverwriteParamTablePos + paramOffset);
        const auto values = ctx.reader.read<std::uint32_t>();
        for (const auto i : std::views::iota(0u, pdt.getNumTriggerParams())) {
            if ((values >> i & 1) == 0) {
                continue;
            }
            ResolveParam(trigger.addOverwriteParam(), ctx, pdt.getTriggerParam(i));
        }
    }
}

template <typename T> requires requires (T& v, AssetCallTableHandle act) { v.setAssetCallTable(act); }
static auto GetAssetCallTable(T& trigger, LoadContext& ctx, const std::unordered_map<std::uint64_t, AssetCallTableHandle>& assetCallTables) -> void {
    const auto assetCallTableOffset = ctx.readPtr();
    if (const auto res = assetCallTables.find(ctx.assetCallTableTablePos + assetCallTableOffset); res == assetCallTables.end()) {
        common::AbortWithDetail("Invalid offset for trigger asset call table @ {:#x}! {:#x}", ctx.reader.tell(), assetCallTableOffset);
    } else {
        trigger.setAssetCallTable(res->second);
    }
}

static auto LoadActionSlot(ActionSlot& slot, LoadContext& ctx, const std::unordered_map<std::uint64_t, AssetCallTableHandle>& assetCallTables, const ParamDefineTable& pdt) -> void {
    slot.setName(ctx.getString(ctx.readPtr()));
    const auto actionStart = ctx.reader.read<std::int16_t>();
    const auto actionEnd = ctx.reader.read<std::int16_t>();
    if (ctx.is64Bit()) {
        ctx.reader.skip(4); // padding
    }

    if (actionStart < 0) {
        return;
    }

    for (const auto i : std::views::iota(actionStart, actionEnd + 1)) {
        const auto scope = ctx.reader.createScope(ctx.actionTablePos + SizeOf<xlink2::ResAction>(ctx.game) * i);
        auto& action = slot.addAction();
        action.setName(ctx.getString(ctx.readPtr()));
        std::int32_t triggerStart, triggerEnd;
        switch (GetResActionLayout(ctx.game)) {
            case 0:
                triggerStart = ctx.reader.read<std::int32_t>();
                triggerEnd = ctx.reader.read<std::int32_t>();
                break;
            case 1:
                triggerStart = ctx.reader.read<std::int16_t>();
                action.setIsPrefix(ctx.reader.read<bool>());
                ctx.reader.skip(1); // padding
                triggerEnd = ctx.reader.read<std::int32_t>();
                break;
            default:
                common::AbortWithDetail("Unknown action layout");
        }

        if (triggerEnd < triggerStart) {
            continue;
        }

        for (const auto j : std::views::iota(triggerStart, triggerEnd + 1)) {
            const auto triggerScope = ctx.reader.createScope(ctx.actionTriggerTablePos + SizeOf<xlink2::ResActionTrigger>(ctx.game) * j);
            auto& trigger = action.addTrigger();
            trigger.setGUID(ctx.reader.read<std::uint32_t>());
            if (GetResActionTriggerLayout(ctx.game) == 1) {
                trigger.setUnknown1(ctx.reader.read<std::uint32_t>());
            }
            GetAssetCallTable(trigger, ctx, assetCallTables);
            const auto prevActionNameOffset = ctx.readPtr();
            trigger.setEndFrame(ctx.reader.read<std::int32_t>());
            const auto flags = common::BitFlag<xlink2::ActionTriggerType>(ctx.reader.read<std::uint16_t>());
            trigger.setUnknown2(ctx.reader.read<std::uint16_t>());
            LoadTriggerOverwriteParam(trigger, ctx, pdt);
            if (flags.isOn(xlink2::ActionTriggerType::Always)) {
                trigger.setType(ActionTriggerType::Always);
            } else if (flags.isOn(xlink2::ActionTriggerType::OnLeave)) {
                trigger.setType(ActionTriggerType::OnLeave);
            } else if (flags.isOn(xlink2::ActionTriggerType::Previous)) {
                trigger.setType(ActionTriggerType::Previous);
                trigger.setPreviousAction(ctx.getString(prevActionNameOffset));
            } else {
                trigger.setType(ActionTriggerType::FrameWindow);
                trigger.setStartFrame(static_cast<std::int32_t>(static_cast<std::uint32_t>(prevActionNameOffset & 0xffff'ffff)));
            }
            trigger.setOneShot(flags.isOn(xlink2::ActionTriggerType::OneShot));
        }
    }
}

static auto LoadProperty(Property& prop, LoadContext& ctx, const std::unordered_map<std::uint64_t, AssetCallTableHandle>& assetCallTables, const ParamDefineTable& pdt) -> void {
    prop.setName(ctx.getString(ctx.readPtr()));
    prop.setScope(ctx.reader.read<std::uint32_t>() == 1 ? PropertyScope::Global : PropertyScope::Local);
    const auto triggerStart = ctx.reader.read<std::int32_t>();
    const auto triggerEnd = ctx.reader.read<std::int32_t>();
    if (ctx.is64Bit()) {
        ctx.reader.skip(4);
    }

    if (triggerEnd < triggerStart) {
        return;
    }

    for (const auto i : std::views::iota(triggerStart, triggerEnd + 1)) {
        const auto scope = ctx.reader.createScope(ctx.propertyTriggerTablePos + SizeOf<xlink2::ResPropertyTrigger>(ctx.game) * i);
        auto& trigger = prop.addTrigger();
        switch (GetResPropertyTriggerLayout(ctx.game)) {
            case 0: {
                trigger.setGUID(ctx.reader.read<std::uint32_t>());
                GetAssetCallTable(trigger, ctx, assetCallTables);
                const auto conditionOffset = ctx.readPtr();
                trigger.setLazy((ctx.reader.read<std::uint16_t>() & 1) != 0);
                trigger.setUnknown(ctx.reader.read<std::uint16_t>());
                LoadTriggerOverwriteParam(trigger, ctx, pdt);
                if (conditionOffset == 0xffff'ffff) {
                    common::AbortWithDetail("Property trigger is missing condition!");
                }
                ctx.reader.seek(ctx.conditionTablePos + conditionOffset);
                auto cond = SwitchCondition();
                LoadSwitchCondition(cond, ctx, prop.getScope() == PropertyScope::Global ? SwitchType::GlobalProperty : SwitchType::LocalProperty);
                trigger.setCondition(std::move(cond));
                break;
            }
            case 1: {
                trigger.setGUID(ctx.reader.read<std::uint32_t>());
                trigger.setLazy((ctx.reader.read<std::uint16_t>() & 1) != 0);
                trigger.setUnknown(ctx.reader.read<std::uint16_t>());
                const auto assetCallTableOffset = ctx.readPtr();
                if (const auto res = assetCallTables.find(ctx.assetCallTableTablePos + assetCallTableOffset); res == assetCallTables.end()) {
                    common::AbortWithDetail("Invalid offset for action trigger asset call table @ {:#x}! {:#x}", ctx.reader.tell(), assetCallTableOffset);
                } else {
                    trigger.setAssetCallTable(res->second);
                }
                const auto conditionOffset = ctx.readPtr();
                LoadTriggerOverwriteParam(trigger, ctx, pdt);
                if (conditionOffset == 0xffff'ffff) {
                    common::AbortWithDetail("Property trigger is missing condition!");
                }
                ctx.reader.seek(ctx.conditionTablePos + conditionOffset);
                auto cond = SwitchCondition();
                LoadSwitchCondition(cond, ctx, prop.getScope() == PropertyScope::Global ? SwitchType::GlobalProperty : SwitchType::LocalProperty);
                trigger.setCondition(std::move(cond));
                break;
            }
            default:
                common::AbortWithDetail("Unknown property trigger layout");
        }
    }
}

static auto LoadAlwaysTrigger(
    AlwaysTrigger& trigger, LoadContext& ctx, const std::unordered_map<std::uint64_t, AssetCallTableHandle>& assetCallTables, const ParamDefineTable& pdt
) -> void {
    trigger.setGUID(ctx.reader.read<std::uint32_t>());
    switch (GetResAlwaysTriggerLayout(ctx.game)) {
        case 0: {
            GetAssetCallTable(trigger, ctx, assetCallTables);
            trigger.setFlags(ctx.reader.read<std::uint16_t>());
            trigger.setUnknown(ctx.reader.read<std::uint16_t>());
            break;
        }
        case 1: {
            trigger.setFlags(ctx.reader.read<std::uint16_t>());
            trigger.setUnknown(ctx.reader.read<std::uint16_t>());
            GetAssetCallTable(trigger, ctx, assetCallTables);
            break;
        }
        default:
            common::AbortWithDetail("Unknown always trigger layout");
    }
    LoadTriggerOverwriteParam(trigger, ctx, pdt);
}

static auto LoadUser(User& user, LoadContext& ctx, const ParamDefineTable& pdt) -> void {
    const auto base = ctx.reader.tell();

    const auto _ = ctx.reader.read<std::uint32_t>(); // isSetup, written to at runtime
    std::uint32_t numLocalProperty;
    if (GetResUserHeaderLayout(ctx.game) == 1) {
        numLocalProperty = ctx.reader.read<std::uint16_t>();
        user.setUnknown(ctx.reader.read<std::int16_t>());
    } else {
        numLocalProperty = ctx.reader.read<std::uint32_t>();
    }

    const auto numCallTable = ctx.reader.read<std::uint32_t>();
    [[maybe_unused]] const auto numAsset = ctx.reader.read<std::uint32_t>();
    [[maybe_unused]] const auto numRandomContainer = ctx.reader.read<std::uint32_t>();
    const auto numResActionSlot = ctx.reader.read<std::uint32_t>();
    const auto numResAction = ctx.reader.read<std::uint32_t>();
    const auto numResActionTrigger = ctx.reader.read<std::uint32_t>();
    const auto numResProperty = ctx.reader.read<std::uint32_t>();
    const auto numResPropertyTrigger = ctx.reader.read<std::uint32_t>();
    const auto numResAlwaysTrigger = ctx.reader.read<std::uint32_t>();
    if (ctx.is64Bit()) {
        ctx.reader.skip(4);
    }
    const auto triggerTablePos = ctx.readPtr();

    ctx.actionSlotTablePos = base + triggerTablePos;
    ctx.actionTablePos = ctx.actionSlotTablePos + SizeOf<xlink2::ResActionSlot>(ctx.game) * numResActionSlot;
    ctx.actionTriggerTablePos = ctx.actionTablePos + SizeOf<xlink2::ResAction>(ctx.game) * numResAction;
    ctx.propertyTablePos = ctx.actionTriggerTablePos + SizeOf<xlink2::ResActionTrigger>(ctx.game) * numResActionTrigger;
    ctx.propertyTriggerTablePos = ctx.propertyTablePos + SizeOf<xlink2::ResProperty>(ctx.game) * numResProperty;
    ctx.alwaysTriggerTablePos = ctx.propertyTriggerTablePos + SizeOf<xlink2::ResPropertyTrigger>(ctx.game) * numResPropertyTrigger;

    for (const auto _ : std::views::iota(0u, numLocalProperty)) {
        user.addLocalProperty(ctx.getString(ctx.readPtr()));
    }

    for (const auto i : std::views::iota(0u, pdt.getNumUserParams())) {
        ResolveParam(user.addParam(), ctx, pdt.getUserParam(i));
    }

    // sorted asset call table indices (we don't need to store these)
    ctx.reader.skip(sizeof(std::uint16_t) * numCallTable);
    ctx.reader.alignUp(4);

    ctx.assetCallTableTablePos = ctx.reader.tell();
    ctx.containerTablePos = ctx.assetCallTableTablePos + SizeOf<xlink2::ResAssetCallTable>(ctx.game) * numCallTable;

    auto callTables = std::vector<AssetCallTableHandle>();
    auto callTableInfo = std::unordered_map<std::uint32_t, CallTableInfo>();
    auto offsetMap = std::unordered_map<std::uint64_t, AssetCallTableHandle>();
    auto usedGuids = std::unordered_set<std::uint32_t>{};
    for (const auto _ : std::views::iota(0u, numCallTable)) {
        const auto offset = ctx.reader.tell();
        auto info = CallTableInfo{};
        auto act = LoadAssetCallTable(info, ctx, pdt);
        callTables.emplace_back(act);
        offsetMap.emplace(offset, act);
        // this is an attempt at compatibility with files written with the older tool
        // the guids were mostly ignored before and so copy/pasting would result in a duplicates
        while (!usedGuids.emplace(act->getGUID()).second) {
            act->rollGUID(); // if guid is taken, try rolling a new one
        }
        if (usedGuids.size() >= 0x1'000'000ull) { // guid is a u32 so just in case we run out (never actually going to happen)
            common::AbortWithDetail("Out of possible GUIDs!");
        }
        callTableInfo.emplace(act->getGUID(), std::move(info));
    }

    auto seen = std::unordered_set<std::uint32_t>{};
    for (auto& act : callTables) {
        auto active = std::unordered_set<std::uint32_t>{};
        ResolveAssetCallTable(act, ctx, callTables, callTableInfo, seen, active);
    }

    ctx.reader.seek(ctx.actionSlotTablePos);
    for (const auto _ : std::views::iota(0u, numResActionSlot)) {
        LoadActionSlot(user.addActionSlot(), ctx, offsetMap, pdt);
    }

    ctx.reader.seek(ctx.propertyTablePos);
    for (const auto _ : std::views::iota(0u, numResProperty)) {
        LoadProperty(user.addProperty(), ctx, offsetMap, pdt);
    }

    ctx.reader.seek(ctx.alwaysTriggerTablePos);
    for (const auto _ : std::views::iota(0u, numResAlwaysTrigger)) {
        LoadAlwaysTrigger(user.addAlwaysTrigger(), ctx, offsetMap, pdt);
    }

    for (const auto& act : callTables) {
        if (!act->getParent()) {
            user.addAssetCallTable(act);
        }
    }
}

auto Database::load(std::span<const std::uint8_t> data, Game game, Platform platform) -> void {
    reset();

    auto ctx = LoadContext{
        .reader = common::BinaryReader(data, GetEndianness(platform)),
        .game = game,
    };

    const auto magic = ctx.reader.readArrayFixed<char, 4>();
    if (magic[0] != 'X' || magic[1] != 'L' || magic[2] != 'N' || magic[3] != 'K') {
        common::AbortWithDetail("Invalid file magic, expected XLNK but got {}{}{}{}", magic[0], magic[1], magic[2], magic[3]);
    }

    const auto dataSize = ctx.reader.read<std::uint32_t>();
    if (ctx.reader.size() < dataSize) {
        common::AbortWithDetail("Reported data size is larger than provided buffer! {:#x} vs. {:#x}", dataSize, ctx.reader.size());
    }

    const auto version = ctx.reader.read<std::uint32_t>();
    if (version == GetVersionELink(ctx.game)) {
        ctx.numSystemUserParam = GetNumSystemUserParamsELink(ctx.game);
        mModuleType = ModuleType::ELink;
    } else if (version == GetVersionSLink(ctx.game)) {
        ctx.numSystemUserParam = GetNumSystemUserParamsSLink(ctx.game);
        mModuleType = ModuleType::SLink;
    } else {
        common::AbortWithDetail("Unexpected version for current profile, expected {:#x} or {:#x} but got {:#x}", GetVersionELink(ctx.game), GetVersionSLink(ctx.game), version);
    }

    ctx.numResParam = ctx.reader.read<std::uint32_t>();
    ctx.numResAssetParam = ctx.reader.read<std::uint32_t>();
    ctx.numResTriggerOverwriteParam = ctx.reader.read<std::uint32_t>(); // numResTriggerOverwirteParam typo
    ctx.alignToPtr();
    ctx.triggerOverwriteParamTablePos = ctx.readPtr();
    const auto localPropertyNameRefTablePos = ctx.readPtr();
    const auto numLocalPropertyNameRefTable = ctx.reader.read<std::uint32_t>();
    const auto numLocalPropertyEnumNameRefTable = ctx.reader.read<std::uint32_t>();
    ctx.numDirectValueTable = ctx.reader.read<std::uint32_t>();
    ctx.numRandomTable = ctx.reader.read<std::uint32_t>();
    ctx.numCurveTable = ctx.reader.read<std::uint32_t>();
    ctx.numCurvePointTable = ctx.reader.read<std::uint32_t>();
    ctx.alignToPtr();
    ctx.exRegionPos = ctx.readPtr();
    const auto numUser = ctx.reader.read<std::uint32_t>();
    ctx.alignToPtr();
    ctx.conditionTablePos = ctx.readPtr();
    const auto nameTablePos = ctx.readPtr();

    const auto userHashes = ctx.reader.readArray<std::uint32_t>(numUser);
    auto userOffsets = std::vector<std::uint64_t>(numUser);
    ctx.alignToPtr();
    for (auto& offset : userOffsets) {
        offset = ctx.readPtr();
    }

    ctx.alignToPtr();
    mParamDefineTable->load(ctx);
    
    ctx.reader.seek(nameTablePos);
    while (ctx.reader.tell() < dataSize) {
        const auto pos = ctx.reader.tell() - nameTablePos;
        const auto str = reinterpret_cast<const char*>(ctx.reader.data() + ctx.reader.tell());
        const auto len = strnlen(str, dataSize - ctx.reader.tell());
        ctx.nameTable.emplace(pos, std::string_view{ str, len });
        ctx.reader.skip(len + 1);
    }

    ctx.reader.seek(localPropertyNameRefTablePos);
    for (const auto _ : std::views::iota(0u, numLocalPropertyNameRefTable)) {
        ctx.localPropertyNameRefTable.emplace_back(ctx.getString(ctx.readPtr()));
    }

    for (const auto _ : std::views::iota(0u, numLocalPropertyEnumNameRefTable)) {
        ctx.localPropertyEnumNameRefTable.emplace_back(ctx.getString(ctx.readPtr()));
    }

    ctx.directValueTablePos = ctx.reader.tell();
    ctx.randomTablePos = ctx.directValueTablePos + sizeof(std::uint32_t) * ctx.numDirectValueTable;
    ctx.curveTablePos = ctx.randomTablePos + SizeOf<xlink2::ResRandom>(ctx.game) * ctx.numRandomTable;
    ctx.curvePointTablePos = ctx.curveTablePos + SizeOf<xlink2::ResCurve>(ctx.game) * ctx.numCurveTable;

    for (const auto i : std::views::iota(0u, numUser)) {
        auto& user = mUsers.emplace_back(std::make_unique<User>(userHashes[i]));
        ctx.reader.seek(userOffsets[i]);
        LoadUser(*user, ctx, getParamDefineTable());
    }

    auto nameDb = UsernameDatabase();
    for (auto& str : ctx.nameTable | std::views::values) {
        nameDb.add(str, true);
    }

    resolveUsernames(nameDb);
}

} // namespace mango