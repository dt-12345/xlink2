#include "asset/asset.hpp"
#include "asset/blend.hpp"
#include "asset/grid.hpp"
#include "asset/sequence.hpp"
#include "asset/switch.hpp"
#include "common/enum.hpp"
#include "common/error.hpp"
#include "condition/blend.hpp"
#include "condition/condition.hpp"
#include "condition/random.hpp"
#include "condition/sequence.hpp"
#include "condition/switch.hpp"
#include "db/db.hpp"
#include "db/pdt.hpp"
#include "db/user.hpp"
#include "property/common.hpp"
#include "trigger/always.hpp"

#include <cmath>
#include <ranges>
#include <unordered_set>

namespace {

[[nodiscard]] static auto NeedsQuotes(char c) -> bool {
    if (std::isspace(c)) {
        return true;
    }

    switch (c) {
        case '"':
        case '=':
        case '!':
        case '<':
        case '>':
        case '(':
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
        case '@':
        case ',':
        case '#':
        case ':':
            return true;
        default:
            return false;
    }
}

struct EscapedString {
    std::string_view value;
};

struct QuotedString {
    std::string_view value;
};

struct Float {
    float value;  
};

struct Condition {
    const mango::SwitchCondition* condition;
    std::string_view varName;
};

}

namespace fmt {

template <>
struct formatter<EscapedString> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const EscapedString& value, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        // https://stackoverflow.com/a/33799784
        for (const auto c : value.value) {
            switch (c) {
                case '"': fmt::format_to(ctx.out(), "\\\""); break;
                case '\\': fmt::format_to(ctx.out(), "\\\\"); break;
                case '\b': fmt::format_to(ctx.out(), "\\b"); break;
                case '\f': fmt::format_to(ctx.out(), "\\f"); break;
                case '\n': fmt::format_to(ctx.out(), "\\n"); break;
                case '\r': fmt::format_to(ctx.out(), "\\r"); break;
                case '\t': fmt::format_to(ctx.out(), "\\t"); break;
                default:
                    if ('\x00' <= c && c <= '\x1f') {
                        fmt::format_to(ctx.out(), "\\u{:04x}", +c);
                    } else {
                        fmt::format_to(ctx.out(), "{}", c);
                    }
            }
        }
        return ctx.out();
    }
};

template <>
struct formatter<QuotedString> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const QuotedString& value, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        if (value.value.empty()) {
            return fmt::format_to(ctx.out(), "\"\"");
        }
        for (const auto c : value.value) {
            if (NeedsQuotes(c)) {
                return fmt::format_to(ctx.out(), "\"{}\"", EscapedString(value.value));
            }
        }
        return fmt::format_to(ctx.out(), "{}", value.value);
    }
};

template <>
struct formatter<Float> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const Float& value, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        if (std::fabs(std::roundf(value.value) - value.value) == 0.f) {
            return fmt::format_to(ctx.out(), "{:.1f}", value.value);
        } else {
            return fmt::format_to(ctx.out(), "{:.9g}", value.value);
        }
    }
};

template <>
struct formatter<mango::PropertyInfo> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const mango::PropertyInfo& prop, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        return fmt::format_to(ctx.out(), "{}::{}", common::ToString(prop.scope), QuotedString{ prop.name });
    }
};

template <>
struct formatter<mango::AssetCallTable> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const mango::AssetCallTable& act, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        return fmt::format_to(ctx.out(), "{}[{:#010x}]", QuotedString{ act.getKeyName() }, act.getGUID());
    }
};

template <>
struct formatter<mango::User> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const mango::User& user, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        if (user.hasKnownName()) {
            return fmt::format_to(ctx.out(), "{}", QuotedString{ user.getName() });
        } else {
            return fmt::format_to(ctx.out(), "{:#010x}", user.getHash());
        }
    }
};

template <>
struct formatter<mango::Switch> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const mango::Switch& switch_, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        switch (switch_.getSwitchType()) {
            case mango::SwitchType::ActionSlot:
                return fmt::format_to(ctx.out(), "ActionSlot::{}", QuotedString{ switch_.getSwitchVariable() });
            case mango::SwitchType::LocalProperty:
                return fmt::format_to(ctx.out(), "Local::{}", QuotedString{ switch_.getSwitchVariable() });
            case mango::SwitchType::GlobalProperty:
                return fmt::format_to(ctx.out(), "Global::{}", QuotedString{ switch_.getSwitchVariable() });
            case mango::SwitchType::Null:
                return fmt::format_to(ctx.out(), "<null>");
            default:
                common::AbortWithDetail("Unknown switch type! {:#x}", std::to_underlying(switch_.getType()));
        }
    }
};

template <>
struct formatter<mango::BlendByCondition> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const mango::BlendByCondition& cond, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        return fmt::format_to(
            ctx.out(), 
            "[Min: {}, Op: {}], [Max: {}, Op: {}]",
            Float(cond.getValueMin()), common::ToString(cond.getBlendOpMin()),
            Float(cond.getValueMax()), common::ToString(cond.getBlendOpMax())
        );
    }
};

template <>
struct formatter<Condition> {
    constexpr auto parse(fmt::format_parse_context& ctx) -> const char* {
        return ctx.begin();
    }

    auto format(const Condition& cond, fmt::format_context& ctx) const -> fmt::format_context::iterator {
        std::string_view compare;
        switch (cond.condition->getCompareType()) {
            case mango::CompareType::Equal:
                compare = "==";
                break;
            case mango::CompareType::GreaterThan:
                compare = ">";
                break;
            case mango::CompareType::GreaterThanOrEqual:
                compare = ">=";
                break;
            case mango::CompareType::LessThan:
                compare = "<";
                break;
            case mango::CompareType::LessThanOrEqual:
                compare = "<=";
                break;
            case mango::CompareType::NotEqual:
                compare = "!=";
                break;
            case mango::CompareType::Default:
                return fmt::format_to(ctx.out(), "_");
        }

        switch (cond.condition->getValue().index()) {
            case 0:
                if (cond.condition->getParentType() == mango::SwitchType::ActionSlot) {
                    return fmt::format_to(ctx.out(), "<value> {} <action>::{}", compare, QuotedString{ std::get<0>(cond.condition->getValue()) });
                } else {
                    return fmt::format_to(ctx.out(), "<value> {} {}::{}", compare, QuotedString{ cond.varName }, QuotedString{ std::get<0>(cond.condition->getValue()) });
                }
            case 1:
                return fmt::format_to(ctx.out(), "<value> {} {}", compare, std::get<1>(cond.condition->getValue()));
            case 2:
                return fmt::format_to(ctx.out(), "<value> {} {}", compare, Float(std::get<2>(cond.condition->getValue())));
            case 3:
                return fmt::format_to(ctx.out(), "<value> {} {}", compare, std::get<3>(cond.condition->getValue()));
            default:
                common::AbortWithDetail("Unreachable default case reached");
        }
    }
};

} // namespace std

namespace mango {

class TextWriter {
public:
    explicit TextWriter(std::size_t bufferSize, bool useBraces = true) : mBuffer(), mIndentLevel(0), mUseBraces(useBraces) {
        mBuffer.reserve(bufferSize);
    }

    template <typename... Ts>
    auto write(const fmt::format_string<Ts...>& fmt, Ts&&... args) -> void {
        fmt::format_to(std::back_inserter(mBuffer), fmt, std::forward<Ts>(args)...);
    }

    auto write(const char* value) -> void {
        mBuffer.append(value);
    }

    auto write(std::string_view value) -> void {
        mBuffer += value;
    }

    auto write(const std::string& value) -> void {
        mBuffer += value;
    }

    auto write(char value) -> void {
        mBuffer += value;
    }

    template <typename... Ts>
    auto writeLine(const fmt::format_string<Ts...>& fmt, Ts&&... args) -> void {
        writeIndents();
        fmt::format_to(std::back_inserter(mBuffer), fmt, std::forward<Ts>(args)...);
        mBuffer += "\n";
    }

    auto writeLine(const char* value) -> void {
        writeIndents();
        mBuffer.append(value);
        mBuffer += "\n";
    }

    auto writeLine(std::string_view value) -> void {
        writeIndents();
        mBuffer += value;
        mBuffer += "\n";
    }

    auto writeLine(const std::string& value) -> void {
        writeIndents();
        mBuffer += value;
        mBuffer += "\n";
    }

    auto writeLine(char value) -> void {
        writeIndents();
        mBuffer += value;
        mBuffer += "\n";
    }

    auto writeIndents() -> void {
        for (std::int32_t i = 0; i < mIndentLevel; ++i) {
            write("  ");
        }
    }

    auto flush() -> std::string { return std::move(mBuffer); }

    friend struct Scope;

    struct Scope {
        Scope() = delete;

        template <typename... Ts>
        Scope(TextWriter& writer, bool indent, const fmt::format_string<Ts...>& format, Ts&&... args) : writer_(&writer) {
            if (indent) {
                writer.writeIndents();
            }
            if (format.get().size() == 0) {
                if (writer.mUseBraces) {
                    writer.write("{\n");
                } else {
                    writer.write("\n");
                }
            } else {
                writer.write(format, std::forward<Ts>(args)...);
                if (writer.mUseBraces) {
                    writer.write(" {\n");
                } else {
                    writer.write("\n");
                }
            }
            ++writer.mIndentLevel;
        }

        Scope(TextWriter& writer, std::string_view name, bool indent) : writer_(&writer) {
            if (indent) {
                writer.writeIndents();
            }
            if (name.empty()) {
                if (writer.mUseBraces) {
                    writer.write("{\n");
                } else {
                    writer.write("\n");
                }
            } else {
                if (writer.mUseBraces) {
                    writer.write("{} {{\n", name);
                } else {
                    writer.write("{}\n", name);
                }
            }
            ++writer.mIndentLevel;
        }

        ~Scope() {
            if (writer_) {
                --writer_->mIndentLevel;
                if (writer_->mUseBraces) {
                    writer_->writeLine("}");
                }
            }
        }

    private:
        TextWriter* writer_;
    };

    template <typename... Ts>
    auto scope(bool indent, const fmt::format_string<Ts...>& fmt, Ts&&... args) -> Scope { return Scope(*this, indent, fmt, std::forward<Ts>(args)...); }
    auto scope(std::string_view name, bool indent = true) -> Scope { return Scope(*this, name, indent); }

    [[nodiscard]] auto hasCallTable(const AssetCallTable& act) const -> bool { return mWrittenCallTables.contains(act.getGUID()); }
    auto addCallTable(const AssetCallTable& act) -> void { mWrittenCallTables.emplace(act.getGUID()); }
    auto clearCallTables() -> void { mWrittenCallTables.clear(); }

private:
    std::string mBuffer;
    std::unordered_set<std::uint32_t> mWrittenCallTables;
    std::int32_t mIndentLevel;
    bool mUseBraces;
};

static auto WriteParamDefine(TextWriter& writer, const Param& def) -> void {
    switch (def.getParamType()) {
        case ParamType::S32:
            writer.writeLine("{} = {}", def.getName(), def.getValue<ValueType::S32>());
            break;
        case ParamType::F32:
            writer.writeLine("{} = {}", def.getName(), Float(def.getValue<ValueType::F32>()));
            break;
        case ParamType::Bool:
            writer.writeLine("{} = {}", def.getName(), def.getValue<ValueType::Bool>());
            break;
        case ParamType::Enum:
            writer.writeLine("{} = {:#x}", def.getName(), def.getValue<ValueType::Enum>());
            break;
        case ParamType::String:
            writer.writeLine("{} = \"{}\"", def.getName(), EscapedString(*def.getValue<ValueType::String>()));
            break;
        case ParamType::Custom: {
            writer.writeLine("{} = <custom>", def.getName());
            break;
        }
    }
}

static auto WriteParam(TextWriter& writer, const Param& param) -> void {
    writer.writeIndents();
    writer.write("{} = ", param.getName());

    switch (param.getValueType()) {
        case ValueType::S32:
            writer.write("{}\n", param.getValue<ValueType::S32>());
            break;
        case ValueType::Bitfield:
            writer.write("{:#b}\n", param.getValue<ValueType::Bitfield>());
            break;
        case ValueType::F32:
            writer.write("{}\n", Float(param.getValue<ValueType::F32>()));
            break;
        case ValueType::Curve: {
            if (!param.getValue<ValueType::Curve>()) {
                common::AbortWithDetail("{} has null curve!", param.getName());
            }
            const auto& curve = param.getValue<ValueType::Curve>();
            const auto _ = writer.scope("CURVE", false);
            writer.writeLine("Type = {}", common::ToString(curve->getType()));
            writer.writeLine("Property = {}", curve->getProperty());
            writer.writeLine("Unknown1 = {}", curve->getUnknown1());
            writer.writeLine("Unknown2 = {}", curve->getUnknown2());
            {
                const auto _ = writer.scope("Points =");
                for (const auto& point : curve->getPoints()) {
                    writer.writeLine("({}, {})", Float(point.x), Float(point.y));
                }
            }
            break;
        }
        case ValueType::Random: {
            if (!param.getValue<ValueType::Random>()) {
                common::AbortWithDetail("{} has null random!", param.getName());
            }
            const auto& random = param.getValue<ValueType::Random>();
            const auto _ = writer.scope("RANDOM", false);
            writer.writeLine("Type = {}", common::ToString(random->getType()));
            if (random->getType() != RandomType::Linear) {
                writer.writeLine("Power = {}", Float(random->getPower()));
            }
            writer.writeLine("Min = {}", Float(random->getMin()));
            writer.writeLine("Max = {}", Float(random->getMax()));
            break;
        }
        case ValueType::Bool:
            writer.write("{}\n", param.getValue<ValueType::Bool>());
            break;
        case ValueType::Enum:
            writer.write("{:#x}\n", param.getValue<ValueType::Enum>());
            break;
        case ValueType::String:
            writer.write("\"{}\"\n", EscapedString(*param.getValue<ValueType::String>()));
            break;
        case ValueType::ArrangeParam: {
            if (!param.getValue<ValueType::ArrangeParam>()) {
                common::AbortWithDetail("{} has a null ArrangeGroup!", param.getName());
            }
            const auto _ = writer.scope("ARRANGE", false);
            for (const auto& group : *param.getValue<ValueType::ArrangeParam>()) {
                const auto _ = writer.scope(true, "{}", QuotedString{ group.getName() });
                writer.writeLine("LimitType = {}", common::ToString(group.getLimitType()));
                writer.writeLine("Threshold = {}", group.getThreshold());
                writer.writeLine("IncludeFading = {}", group.getIncludeFading());
            }
            break;
        }
        default:
            std::unreachable();
    }
}

static auto WriteActionSlot(TextWriter& writer, const ActionSlot& slot) -> void {
    const auto _ = writer.scope(true, "{}", QuotedString{ slot.getName() });
    for (const auto& action : slot.getActions()) {
        if (action.getIsPrefix()) {
            writer.writeLine("@Prefix");
        }
        const auto _ = writer.scope(true, "{}", QuotedString{ action.getName() });
        for (const auto& trigger : action.getTriggers()) {
            if (trigger.getAssetCallTable() == nullptr) {
                common::AbortWithDetail(
                    "Missing asset call table for action trigger! ActionSlot: {}, Action: {}, GUID: {:#010x}", slot.getName(), action.getName(), trigger.getGUID()
                );
            }
            const auto _ = writer.scope(true, "{:#010x}", trigger.getGUID());
            writer.writeLine("Type = {}", common::ToString(trigger.getType()));
            writer.writeLine("Start = {}", trigger.getStartFrame());
            writer.writeLine("End = {}", trigger.getStartFrame());
            switch (trigger.getType()) {
                case ActionTriggerType::FrameWindow:
                case ActionTriggerType::Always:
                case ActionTriggerType::OnLeave:
                    break;
                case ActionTriggerType::Previous:
                    writer.writeLine("PreviousAction = {}", QuotedString{ trigger.getPreviousAction() });
                    break;
            }
            writer.writeLine("Unknown1 = {}", trigger.getUnknown1());
            writer.writeLine("Unknown2 = {}", trigger.getUnknown2());
            writer.writeLine("Oneshot = {}", trigger.getOneShot());
            writer.writeLine("Asset = {}", *trigger.getAssetCallTable());
            if (trigger.getOverwriteParams().size() > 0) {
                const auto _ = writer.scope("OverwriteParams = ");
                for (const auto& param : trigger.getOverwriteParams()) {
                    WriteParam(writer, param);
                }
            }
        }
    }
}

static auto WriteProperty(TextWriter& writer, const Property& prop) -> void {
    const auto _ = writer.scope(true, "{}", prop.getInfo());
    for (const auto& trigger : prop.getTriggers()) {
        if (trigger.getAssetCallTable() == nullptr) {
            common::AbortWithDetail("Missing asset call table for property trigger! Property: {} GUID: {:#010x}", prop.getName(), trigger.getGUID());
        }
        const auto _ = writer.scope(
            true, "if {} => {:#010x}", Condition(&trigger.getCondition(), prop.getName()), trigger.getGUID()
        );
        writer.writeLine("Lazy = {}", trigger.getLazy());
        writer.writeLine("Unknown = {}", trigger.getUnknown());
        writer.writeLine("Asset = {}", *trigger.getAssetCallTable());
        if (trigger.getOverwriteParams().size() > 0) {
            const auto _ = writer.scope("OverwriteParams = ");
            for (const auto& param : trigger.getOverwriteParams()) {
                WriteParam(writer, param);
            }
        }
    }
}

static auto WriteAlwaysTrigger(TextWriter& writer, const AlwaysTrigger& trigger) -> void {
    if (trigger.getAssetCallTable() == nullptr) {
        common::AbortWithDetail("Missing asset call table for always trigger! GUID: {:#010x}", trigger.getGUID());
    }
    
    const auto _ = writer.scope(true, "{:#010x}", trigger.getGUID());
    writer.writeLine("Flags = {}", trigger.getFlags());
    writer.writeLine("Unknown = {}", trigger.getUnknown());
    writer.writeLine("Asset = {}", *trigger.getAssetCallTable());
    if (trigger.getOverwriteParams().size() > 0) {
        const auto _ = writer.scope("OverwriteParams = ");
        for (const auto& param : trigger.getOverwriteParams()) {
            WriteParam(writer, param);
        }
    }
}

static auto WriteAssetCallTable(TextWriter& writer, const AssetCallTable& act, bool indent) -> void {
    if (writer.hasCallTable(act)) {
        if (indent) {
            writer.writeLine("{}", act);
        } else {
            writer.write("{}\n", act);
        }
        return;
    }

    writer.addCallTable(act);

    const auto _ = writer.scope(indent, "{}", act);
    writer.writeLine("EmitCount = {}", act.getEmitCount());
    writer.writeLine("Oneshot = {}", act.getOneShot());
    writer.writeLine("NoPause = {}", act.getNoPause());
    writer.writeLine("UserFlags = {:#x}", +act.getUserFlags());

    switch (act.getType()) {
        case CallTableType::Switch: {
            const auto& switch_ = static_cast<const Switch&>(act);
            writer.writeLine("@Unknown = {}", switch_.getUnknown());
            const auto _ = writer.scope(true, "Execute = Switch ({})", switch_);
            for (const auto& child : switch_.getChildren()) {
                const auto& cond = child->getCondition();
                if (cond->getType() != ConditionType::Switch) {
                    common::AbortWithDetail(
                        "Switch container {} child {} has invalid child condition type! {}",
                        act, *child, common::ToString(cond->getType())
                    );
                }
                const auto switchCond = static_cast<const SwitchCondition*>(cond.get());
                writer.writeIndents();
                writer.write("({}) => ", Condition(switchCond, switch_.getSwitchVariable()));
                WriteAssetCallTable(writer, *child, false);
            }
            break;
        }
        case CallTableType::Random: {
            const auto _ = writer.scope("Execute = Random");
            for (const auto& child : act.getChildren()) {
                const auto& cond = child->getCondition();
                writer.writeIndents();
                if (cond->getType() == ConditionType::Random) {
                    writer.write("{} => ", Float(static_cast<const RandomCondition*>(cond.get())->getWeight()));
                } else if (cond->getType() == ConditionType::RandomNoRepeat) {
                    writer.write("{} => ", Float(static_cast<const RandomNoRepeatCondition*>(cond.get())->getWeight()));
                } else {
                    common::AbortWithDetail(
                        "Random container {} child {} has invalid child condition type! {}",
                        act, *child, common::ToString(cond->getType())
                    );
                }
                WriteAssetCallTable(writer, *child, false);
            }
            break;
        }
        case CallTableType::RandomNoRepeat: {
            const auto _ = writer.scope("Execute = RandomNoRepeat");
            for (const auto& child : act.getChildren()) {
                const auto& cond = child->getCondition();
                writer.writeIndents();
                if (cond->getType() == ConditionType::Random) {
                    writer.write("{} => ", Float(static_cast<const RandomCondition*>(cond.get())->getWeight()));
                } else if (cond->getType() == ConditionType::RandomNoRepeat) {
                    writer.write("{} => ", Float(static_cast<const RandomNoRepeatCondition*>(cond.get())->getWeight()));
                } else {
                    common::AbortWithDetail(
                        "RandomNoRepeat container {} child {} has invalid child condition type! {}",
                        act, *child, common::ToString(cond->getType())
                    );
                }
                WriteAssetCallTable(writer, *child, false);
            }
            break;
        }
        case CallTableType::Blend: {
            const auto _ = writer.scope("Execute = Blend");
            for (const auto& child : act.getChildren()) {
                WriteAssetCallTable(writer, *child, true);
            }
            break;
        }
        case CallTableType::BlendBy: {
            const auto& blend = static_cast<const BlendBy&>(act);
            writer.writeLine("@Unknown = {}", blend.getUnknown());
            const auto _ = writer.scope(true, "Execute = BlendBy ({})", blend.getBlendProperty());
            for (const auto& child : blend.getChildren()) {
                const auto& cond = child->getCondition();
                if (cond->getType() != ConditionType::BlendBy) {
                    common::AbortWithDetail(
                        "BlendBy container {} child {} has invalid child condition type! {}",
                        act, *child, common::ToString(cond->getType())
                    );
                }
                const auto blendCond = static_cast<const BlendByCondition*>(cond.get());
                writer.writeIndents();
                writer.write("({}) => ", *blendCond);
                WriteAssetCallTable(writer, *child, false);
            }
            break;
        }
        case CallTableType::Sequence: {
            const auto _ = writer.scope("Execute = Sequence");
            for (const auto& child : act.getChildren()) {
                const auto& cond = child->getCondition();
                if (cond && cond->getType() == ConditionType::Sequence) {
                    writer.writeLine("@ContinueOnFade = {}", static_cast<const SequenceCondition*>(cond.get())->getContinueOnFade());
                }
                WriteAssetCallTable(writer, *child, true);
            }
            break;
        }
        case CallTableType::Grid: {
            const auto& grid = static_cast<const Grid&>(act);
            grid.ensureValidSize();
            const auto _ = writer.scope(true, "Execute = Grid ({}, {})", grid.getProperty1(), grid.getProperty2());
            {
                const auto _ = writer.scope("Cases");
                for (const auto [i, val1] : grid.getValues1() | std::views::enumerate) {
                    for (const auto [j, val2] : grid.getValues2() | std::views::enumerate) {
                        const auto& child = grid.getAssetCallTables().at(i * grid.getValues2().size() + j);
                        if (child) {
                            writer.writeLine(
                                "({}::{}, {}::{}) => {}",
                                QuotedString{ grid.getProperty1Name() }, QuotedString{ val1 },
                                QuotedString{ grid.getProperty2Name() }, QuotedString{ val2 },
                                *child
                            );
                        } else {
                            writer.writeLine(
                                "({}::{}, {}::{}) => <null>",
                                QuotedString{ grid.getProperty1Name() }, QuotedString{ val1 },
                                QuotedString{ grid.getProperty2Name() }, QuotedString{ val2 }
                            );
                        }
                    }
                }
            }
            {
                const auto _ = writer.scope("Children");
                for (const auto& child : grid.getChildren()) {
                    WriteAssetCallTable(writer, *child, true);
                }
            }
            break;
        }
        case CallTableType::Jump: {
            if (act.getChildren().size() > 0 && act.getChildren().front()) {
                writer.writeLine("Execute = Jump => {}", *act.getChildren().front());
            } else {
                writer.writeLine("Execute = Jump => <null>");
            }
            break;
        }
        case CallTableType::Asset: {
            const auto _ = writer.scope("Execute = Asset");
            for (const auto& param : static_cast<const Asset&>(act).getParams()) {
                WriteParam(writer, param);
            }
            break;
        }
        default:
            std::unreachable();
    }
}

auto Database::text(bool useBraces) const -> std::string {
    if (!mParamDefineTable) {
        common::AbortWithDetail("ParamDefineTable has not been initialized!");
    }

    auto writer = TextWriter(0x1000000, useBraces); // 16 MB, might be a bit low for some files
    {
        const auto _ = writer.scope("Metadata");
        writer.writeLine("ModuleType = {}", common::ToString(mModuleType));
    }

    {
        const auto _ = writer.scope("ParamDefines");
        {
            const auto _ = writer.scope("SystemUserParams");
            for (const auto& def : mParamDefineTable->getSystemUserParams()) {
                WriteParamDefine(writer, def);
            }
        }
        {
            const auto _ = writer.scope("CustomUserParams");
            for (const auto& def : mParamDefineTable->getCustomUserParams()) {
                WriteParamDefine(writer, def);
            }
        }
        {
            const auto _ = writer.scope("SystemAssetParams");
            for (const auto& def : mParamDefineTable->getSystemAssetParams()) {
                WriteParamDefine(writer, def);
            }
        }
        {
            const auto _ = writer.scope("CustomAssetParams");
            for (const auto& def : mParamDefineTable->getCustomAssetParams()) {
                WriteParamDefine(writer, def);
            }
        }
        {
            const auto _ = writer.scope("TriggerParams");
            for (const auto& def : mParamDefineTable->getTriggerParams()) {
                WriteParamDefine(writer, def);
            }
        }
    }

    {
        const auto _ = writer.scope("Users");
        for (const auto& user : mUsers) {
            if (!user) {
                continue;
            }
            if (!useBraces) {
                writer.write("=========================\n");
            }
            const auto _ = writer.scope(true, "{}", *user);
            writer.writeLine("Unknown = {}", user->getUnknown());
            {
                const auto _ = writer.scope("UserParams");
                for (const auto& param : user->getParams()) {
                    WriteParam(writer, param);
                }
            }
            if (user->getLocalProperties().size() > 0) {
                const auto _ = writer.scope("LocalProperties");
                for (const auto& prop : user->getLocalProperties()) {
                    writer.writeLine("{}", QuotedString{ prop });
                }
            }
            if (user->getActionSlots().size() > 0) {
                const auto _ = writer.scope("ActionSlots");
                for (const auto& slot : user->getActionSlots()) {
                    WriteActionSlot(writer, slot);
                }
            }
            if (user->getProperties().size() > 0) {
                const auto _ = writer.scope("Properties");
                for (const auto& prop : user->getProperties()) {
                    WriteProperty(writer, prop);
                }
            }
            if (user->getAlwaysTriggers().size() > 0) {
                const auto _ = writer.scope("AlwaysTriggers");
                for (const auto& trigger : user->getAlwaysTriggers()) {
                    WriteAlwaysTrigger(writer, trigger);
                }
            }
            {
                const auto _ = writer.scope("AssetCallTables");
                for (const auto& act : user->getAssetCallTables()) {
                    if (!act) {
                        continue;
                    }
                    WriteAssetCallTable(writer, *act, true);
                }
            }
            writer.clearCallTables();
            if (!useBraces) {
                writer.write("=========================\n\n");
            }
        }
    }

    return writer.flush();
}

} // namespace mango