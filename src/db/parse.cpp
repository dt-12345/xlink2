#include "asset/asset.hpp"
#include "asset/handle.hpp"
#include "asset/blend.hpp"
#include "asset/grid.hpp"
#include "asset/jump.hpp"
#include "asset/random.hpp"
#include "asset/sequence.hpp"
#include "asset/switch.hpp"
#include "common/enum.hpp"
#include "common/error.hpp"
#include "condition/blend.hpp"
#include "condition/random.hpp"
#include "condition/sequence.hpp"
#include "condition/switch.hpp"
#include "db/db.hpp"
#include "db/pdt.hpp"
#include "db/user.hpp"
#include "trigger/action.hpp"
#include "trigger/always.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// TODO: comment support?

namespace mango {

// takes in the escaped string without the quotes on either side
[[nodiscard]] static auto Unescape(std::string_view str) -> std::string {
    auto o = std::string("");
    for (auto i = std::size_t(0); i < str.size(); ++i) {
        const auto c = str[i];
        if (c != '\\' || i + 1 >= str.size()) {
            o += c;
            continue;
        }
        switch (str[i + 1]) {
            case '"': o += '"'; ++i; break;
            case '\\': o += '\\'; ++i; break;
            case 'b': o += '\b'; ++i; break;
            case 'f': o += '\f'; ++i; break;
            case 'n': o += '\n'; ++i; break;
            case 'r': o += '\r'; ++i; break;
            case 't': o += '\t'; ++i; break;
            case 'u': {
                if (i + 5 < str.size()) {
                    auto value = '\0';
                    std::from_chars(str.data() + 2, str.data() + 6, value, 16);
                    o += value;
                    i += 5;
                    break;
                }
                [[fallthrough]];
            }
            default:
                o += c;
                break;
        }
    }
    return o;
}

[[nodiscard]] static auto ToS32(std::string_view str, std::int32_t base) -> std::int32_t {
    auto value = 0;
    auto start = str.begin();
    if (base == 2 || base == 16) {
        if (str.size() < 3) {
            common::AbortWithDetail("String is too short to parse as S32!");
        }
        start = str.begin() + 2;
    }
    const auto [_, ec] = std::from_chars(start, str.end(), value, base);
    if (ec != std::errc()) {
        common::AbortWithDetail("Failed to parse S32 from {}", str);
    }
    return value;
}

[[nodiscard]] static auto ToU32(std::string_view str, std::int32_t base) -> std::uint32_t {
    auto value = 0u;
    auto start = str.begin();
    if (base == 2 || base == 16) {
        if (str.size() < 3) {
            common::AbortWithDetail("String is too short to parse as U32!");
        }
        start = str.begin() + 2;
    }
    const auto [_, ec] = std::from_chars(start, str.end(), value, base);
    if (ec != std::errc()) {
        common::AbortWithDetail("Failed to parse U32 from {}", str);
    }
    return value;
}

[[nodiscard]] static auto ToF32(std::string_view str) -> float {
    auto value = 0.f;
    auto start = str.begin();
    if (str.size() > 0 && str[0] == '+') {
        start = str.data() + 1;
    }
    const auto [_, ec] = std::from_chars(start, str.end(), value);
    if (ec != std::errc()) {
        common::AbortWithDetail("Failed to parse float from {}", str);
    }
    return value;
}

struct Token {
    enum Type {
        Identifier,
        IntegerBin,
        IntegerDec,
        IntegerHex,
        Float,
        String,
        Assign,
        Arrow,
        Equal,
        NotEqual,
        LessThan,
        GreaterThan,
        LessThanEqual,
        GreaterThanEqual,
        ParenthesisOpen,
        ParenthesisClose,
        ScopeBegin,
        ScopeEnd,
        BracketOpen,
        BracketClose,
        At,
        Comma,
        Colon,
        Comment,
        Scope,
        EndOfFile,
    };

    Type type;
    std::string_view value = "";
};

[[nodiscard]] static auto IsDigit(char c) -> bool {
    switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return true;
        default:
            return false;
    }
}

[[nodiscard]] static auto IsBinDigit(char c) -> bool {
    switch (c) {
        case '0':
        case '1':
            return true;
        default:
            return false;
    }
}

[[nodiscard]] static auto IsHexDigit(char c) -> bool {
    switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            return true;
        default:
            return false;
    }
}

[[nodiscard]] static auto IsEndOfToken(char c) -> bool {
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

[[nodiscard]] static auto HasValue(Token::Type type) -> bool {
    switch (type) {
        case Token::Identifier:
        case Token::IntegerBin:
        case Token::IntegerDec:
        case Token::IntegerHex:
        case Token::Float:
        case Token::String:
            return true;
        default:
            return false;
    }
}

class Lexer {
public:
    explicit Lexer(const std::span<const char> data) : mData(data) {}

    [[nodiscard]] auto getLine() const -> std::size_t { return mCurrentLine + 1; }
    [[nodiscard]] auto getColumn() const -> std::size_t { return mCurrentColumn + 1; }
    [[nodiscard]] auto getPosition() const -> std::size_t { return mCurrentPosition; }

    [[nodiscard]] auto next() -> Token {
        skipWhitespace();
        if (isEOF()) {
            return Token{ Token::EndOfFile };
        }

        switch (peek()) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '+':
            case '-':
            case 'i':
            case 'I':
            case 'n':
            case 'N':
                return readNumber();
            case '"':
                return readString();
            case '=':
                return readEqual();
            case '!':
                return readNotEqual();
            case '<':
                return readLessThan();
            case '>':
                return readGreaterThan();
            case '(':
                return readParenthesisOpen();
            case ')':
                return readParenthesisClose();
            case '{':
                return readScopeBegin();
            case '}':
                return readScopeEnd();
            case '[':
                return readBracketOpen();
            case ']':
                return readBracketClose();
            case '@':
                return readAtSign();
            case ',':
                return readComma();
            case '#':
                return readComment();
            case ':':
                return readColon();
            default:
                return readIdentifier();
        }
    }

private:
    auto skipWhitespace() -> void {
        const auto startLine = mCurrentLine;
        while (!isEOF()) {
            const auto c = peek();
            if (std::isspace(c)) {
                advance();
                if (c == '\n') { ++mCurrentLine; mCurrentColumn = 0; }
            } else {
                break;
            }
        }
        if (startLine != mCurrentLine) {
            mCurrentIndent = mCurrentColumn;
        }
    }

    [[noreturn]] auto error(const std::string& msg) -> void {
        common::AbortWithDetail("[LEXER ERROR] {}\n  Line: {}\n  Position: {}\n", msg, getLine(), getColumn());
    }

    [[nodiscard]] auto isEOF() const -> bool { return mCurrentPosition >= mData.size(); }
    [[nodiscard]] auto peek() const -> char { return mData[mCurrentPosition]; }
    [[nodiscard]] auto peekAt(std::size_t pos) const -> char { return mData[pos]; }
    auto advance(std::size_t amount = 1) -> void { mCurrentPosition += amount; mCurrentColumn += amount; }
    [[nodiscard]] auto checkNext(char c) const -> bool {
        if (mCurrentPosition + 1 >= mData.size()) {
            return false;
        }

        return mData[mCurrentPosition + 1] == c;
    }
    [[nodiscard]] auto checkNext(std::string_view str) const -> bool {
        if (mCurrentPosition + str.size() >= mData.size()) {
            return false;
        }

        return std::string_view{ mData.data() + mCurrentPosition + 1, str.size() } == str;
    }

    [[nodiscard]] auto readIdentifier() -> Token {
        const auto start = mCurrentPosition;
        while (!isEOF() && !IsEndOfToken(peek())) {
            advance();
        }
        return Token{ Token::Identifier, { mData.data() + start, mData.data() + mCurrentPosition } };
    }

    [[nodiscard]] auto readNumber() -> Token {
        const auto start = mCurrentPosition;
        auto error = false; // bail and return an identifier if error
        auto foundExp = false;
        auto expPos = std::size_t(-1);
        auto foundDot = false;
        auto foundHex = false;
        auto foundBin = false;
        auto foundNan = false;
        auto foundInf = false;
        auto hasSign = false;
        while (!isEOF() && !IsEndOfToken(peek())) {
            if (error) {
                break;
            }
            switch (const char c = peek()) {
                case 'b':
                    if (!foundHex) {
                        if (foundBin || foundHex || foundDot || foundExp || foundInf || foundNan) {
                            error = true;
                        } else if (mCurrentPosition == start + 1 && mData[start] == '0') {
                            foundBin = true;
                        }
                    }
                    break;
                case 'x':
                    if (foundBin || foundHex || foundDot || foundExp || foundInf || foundNan) {
                        error = true;
                    } else if (mCurrentPosition == start + 1 && mData[start] == '0') {
                        foundHex = true;
                    }
                    break;
                case 'e':
                case 'E':
                    if (!foundHex) {
                        if (mCurrentPosition == start || foundBin || foundExp || foundInf || foundNan) {
                            error = true;
                        } else {
                            foundExp = true;
                            expPos = mCurrentPosition;
                        }
                    }
                    break;
                case '+':
                case '-':
                    if (mCurrentPosition != start) {
                        if (foundHex || foundBin || !foundExp || (foundExp && expPos != mCurrentPosition - 1) || foundInf || foundNan) {
                            error = true;
                        }
                    } else {
                        hasSign = true;
                    }
                    break;
                case '.':
                    if (foundHex || foundBin || foundDot || foundExp || foundInf || foundNan) {
                        error = true;
                    } else {
                        foundDot = true;
                    }
                    break;
                default:
                    if (foundHex) {
                        error = !IsHexDigit(c);
                    } else if (foundBin) {
                        error = !IsBinDigit(c);
                    } else if (!IsDigit(c)) {
                        if (foundExp || foundDot) {
                            error = true;
                        } else if (c == 'i' || c == 'I') {
                            foundInf = true;
                            if (hasSign) {
                                error = mCurrentPosition != start + 1;
                            } else {
                                error = mCurrentPosition != start;
                            }
                        } else if (c == 'f' || c == 'F') {
                            if (hasSign) {
                                error = !foundInf && mCurrentPosition != start + 3;
                            } else {
                                error = !foundInf && mCurrentPosition != start + 2;
                            }
                            foundInf = foundInf && !error;
                        } else if (c == 'a' || c == 'A') {
                            if (hasSign) {
                                error = !foundNan && mCurrentPosition != start + 3;
                            } else {
                                error = !foundNan && mCurrentPosition != start + 2;
                            }
                            foundNan = foundNan && !error;
                        } else if (c == 'n' || c == 'N') {
                            // with nan or inf, n can be in any of the first three spots but the other checks will fail if it's neither nan nor inf
                            if (hasSign) {
                                foundInf = foundInf && mCurrentPosition == start + 2;
                                foundNan = (mCurrentPosition == start + 1) ? true : (mCurrentPosition == start + 3);
                                error = mCurrentPosition > start + 3;
                            } else {
                                foundInf = foundInf && mCurrentPosition == start + 1;
                                foundNan = mCurrentPosition == start ? true : (mCurrentPosition == start + 2);
                                error = mCurrentPosition > start + 2;
                            }
                        } else {
                            error = true;
                        }
                    } else {
                        if (foundNan || foundInf) {
                            error = true;
                        }
                    }
            }
            advance();
        }

        if (foundInf || foundNan) {
            if (mCurrentPosition - start != (hasSign ? 4 : 3)) {
                error = true;
            } else if (foundInf && foundNan) {
                error = true;
            }
        }

        if (error) {
            while (!isEOF() && !IsEndOfToken(peek())) {
                advance();
            }
            return Token{ Token::Identifier, { mData.data() + start, mData.data() + mCurrentPosition } };
        } else if (foundDot || foundExp || foundInf || foundNan) {
            return Token{ Token::Float, { mData.data() + start, mData.data() + mCurrentPosition } };
        } else if (foundHex) {
            return Token{ Token::IntegerHex, { mData.data() + start, mData.data() + mCurrentPosition } };
        } else if (foundBin) {
            return Token{ Token::IntegerBin, { mData.data() + start, mData.data() + mCurrentPosition } };
        } else {
            return Token{ Token::IntegerDec, { mData.data() + start, mData.data() + mCurrentPosition } };
        }
    }

    [[nodiscard]] auto readString() -> Token {
        advance();
        const auto start = mCurrentPosition;
        if (isEOF()) {
            error("Unclosed string!");
        }
        auto backslashCount = 0;
        while (!isEOF()) {
            switch (peek()) {
                case '\n':
                    error("Unclosed string!");
                case '\\':
                    ++backslashCount;
                    break;
                case '"':
                    if (backslashCount % 2 == 0) {
                        const auto end = mCurrentPosition;
                        advance();
                        return Token{ Token::String, { mData.data() + start, mData.data() + end } };
                    }
                    [[fallthrough]];
                default:
                    backslashCount = 0;
                    break;
            }
            advance();
        }
        error("Unclosed string!");
    }

    [[nodiscard]] auto readEqual() -> Token {
        if (checkNext('=')) {
            advance(); advance();
            return Token{ Token::Equal };
        } else if (checkNext('>')) {
            advance(); advance();
            return Token{ Token::Arrow };
        } else {
            advance();
            return Token{ Token::Assign };
        }
    }

    [[nodiscard]] auto readNotEqual() -> Token {
        if (checkNext('=')) {
            advance(); advance();
            return Token{ Token::NotEqual };
        } else {
            return readIdentifier();
        }
    }

    [[nodiscard]] auto readLessThan() -> Token {
        if (checkNext("=")) {
            advance(); advance();
            return Token{ Token::LessThanEqual };
        } else {
            advance();
            return Token{ Token::LessThan };
        }
    }

    [[nodiscard]] auto readGreaterThan() -> Token {
        if (checkNext("=")) {
            advance(); advance();
            return Token{ Token::GreaterThanEqual };
        } else {
            advance();
            return Token{ Token::GreaterThan };
        }
    }

    [[nodiscard]] auto readParenthesisOpen() -> Token {
        advance();
        return Token{ Token::ParenthesisOpen };
    }
    
    [[nodiscard]] auto readParenthesisClose() -> Token {
        advance();
        return Token{ Token::ParenthesisClose };
    }

    [[nodiscard]] auto readScopeBegin() -> Token {
        advance();
        return Token{ Token::ScopeBegin };
    }

    [[nodiscard]] auto readScopeEnd() -> Token {
        advance();
        return Token{ Token::ScopeEnd };
    }

    [[nodiscard]] auto readBracketOpen() -> Token {
        advance();
        return Token{ Token::BracketOpen };
    }

    [[nodiscard]] auto readBracketClose() -> Token {
        advance();
        return Token{ Token::BracketClose };
    }

    [[nodiscard]] auto readAtSign() -> Token {
        advance();
        return Token{ Token::At };
    }

    [[nodiscard]] auto readComma() -> Token {
        advance();
        return Token{ Token::Comma };
    }

    [[nodiscard]] auto readComment() -> Token {
        while (!isEOF() && peek() != '\n') { advance(); }
        return Token{ Token::Comment };
    }

    [[nodiscard]] auto readColon() -> Token {
        if (checkNext(":")) {
            advance(); advance();
            return Token{ Token::Scope };
        } else {
            advance();
            return Token{ Token::Colon };
        }
    }

    [[nodiscard]] auto readEndOfFile() -> Token {
        return Token{ Token::EndOfFile };
    }

    friend class Scope;

    const std::span<const char> mData;
    std::size_t mCurrentLine = 0;
    std::size_t mCurrentColumn = 0;
    std::size_t mCurrentPosition = 0;
    std::size_t mCurrentIndent = 0;
};

template <typename... Ts>
[[noreturn]] static auto SyntaxError(const Lexer& lexer, const Token& token, const fmt::format_string<Ts...>& format, Ts&&... args) -> void {
    auto msg = std::string("[SYNTAX ERROR] ");
    fmt::format_to(std::back_inserter(msg), format, std::forward<Ts>(args)...);
    fmt::format_to(
        std::back_inserter(msg),
        "\n  Last Token: {} ({})\n  Line: {}\n  Pos:  {}",
        common::ToString(token.type), token.value, lexer.getLine(), lexer.getColumn()
    );
    common::AbortWithDetail(msg.c_str());
}

class Scope {
public:
    Scope(Lexer& lexer, const char* name) : mLexer(std::ref(lexer)), mName(name) {
        mLastLine = mLexer.get().mCurrentLine;
        const auto initalIndent = mLexer.get().mCurrentIndent;
        mLexer.get().skipWhitespace();
        if (mLexer.get().peek() == '{') {
            mLastToken = mLexer.get().next();
            mActive = true;
        } else {
            while (mLexer.get().peek() == '=') { mLexer.get().advance(); }
            mLexer.get().skipWhitespace();
            mIndentLevel = mLexer.get().mCurrentIndent;
            if (initalIndent < mLexer.get().mCurrentIndent) {
                mActive = true;
            } else {
                mActive = false;
            }
        }
    }

    Scope(Scope&&) = delete;
    auto operator=(Scope&&) -> Scope& = delete;
    Scope(const Scope&) = delete;
    auto operator=(const Scope&) -> Scope& = delete;

    ~Scope() {
        if (mActive) {
            if (!isIndent()) {
                while (mLastToken.type != Token::ScopeEnd) { get(); }
            } else {
                mLexer.get().skipWhitespace();
                while (mLexer.get().peek() == '=') { mLexer.get().advance(); }
            }
        }
    }

    operator bool() const {
        mLexer.get().skipWhitespace();
        return mActive && (isIndent() ? mLexer.get().mCurrentIndent == mIndentLevel : mLexer.get().peek() != '}');
    }

    auto get() -> Token& {
        if (isIndent()) {
            mLexer.get().skipWhitespace();
            if (mLastLine != mLexer.get().mCurrentLine && mIndentLevel == mLexer.get().mCurrentIndent) {
                mLastLine = mLexer.get().mCurrentLine;
            } else {
                SyntaxError(mLexer.get(), Token{}, "Scope {} expected new line with {} indent!", mName, mIndentLevel);
            }
            mLastToken = mLexer.get().next();
        } else {
            mLastToken = mLexer.get().next();
            if (mLastToken.type == Token::EndOfFile) {
                SyntaxError(mLexer.get(), mLastToken, "Pre-mature end-of-file detected! Scope for {} was not closed!", mName);
            }
        }
        return mLastToken;
    }

private:
    [[nodiscard]] auto isIndent() const -> bool { return mIndentLevel != 0xffff'ffff'ffff'ffffull; }

    mutable std::reference_wrapper<Lexer> mLexer;
    const char* mName;
    Token mLastToken;
    std::size_t mIndentLevel = 0xffff'ffff'ffff'ffffull;
    std::size_t mLastLine = 0;
    bool mActive;
};

[[nodiscard]] static auto GetIdentifier(Lexer& lexer) -> std::string_view {
    const auto token = lexer.next();
    if (!HasValue(token.type)) {
        SyntaxError(lexer, token, "Expected identifier!");
    }
    return token.value;
}

[[nodiscard]] static auto GetString(Lexer& lexer) -> std::string {
    const auto token = lexer.next();
    if (token.type == Token::String) {
        return Unescape(token.value);
    } else if (HasValue(token.type)) {
        return std::string(token.value);
    }
    SyntaxError(lexer, token, "Expected identifier or string!");
}

[[nodiscard]] static auto GetInt(Lexer& lexer) -> std::int32_t {
    const auto token = lexer.next();
    switch (token.type) {
        case Token::IntegerBin:
            return ToS32(token.value, 2);
        case Token::IntegerDec:
            return ToS32(token.value, 10);
        case Token::IntegerHex:
            return ToS32(token.value,16);
        default:
            SyntaxError(lexer, token, "Failed to read integer!");
    }
}

[[nodiscard]] static auto GetUInt(Lexer& lexer) -> std::uint32_t {
    const auto token = lexer.next();
    switch (token.type) {
        case Token::IntegerBin:
            return ToU32(token.value, 2);
        case Token::IntegerDec:
            return ToU32(token.value, 10);
        case Token::IntegerHex:
            return ToU32(token.value,16);
        default:
            SyntaxError(lexer, token, "Failed to read integer!");
    }
}

[[nodiscard]] static auto GetFloat(Lexer& lexer) -> float {
    const auto token = lexer.next();
    if (token.type == Token::Float) {
        return ToF32(token.value);
    } else if (token.type == Token::IntegerDec) {
        return static_cast<float>(ToS32(token.value, 10));
    } else {
        SyntaxError(lexer, token, "Failed to read float!");
    }
}

[[nodiscard]] static auto GetBool(Lexer& lexer) -> bool {
    return GetIdentifier(lexer) == "true";
}

static auto EnsureToken(Lexer& lexer, Token::Type type) -> void {
    const auto token = lexer.next();
    if (token.type != type) {
        SyntaxError(lexer, token, "Unexpected token type! Expected {}", common::ToString(type));
    }
}

static auto ParseMetadata(Database& db, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "Metadata");
    const auto key = scope.get();
    if (key.type == Token::ScopeEnd) {
        return;
    } else if (key.type != Token::Identifier) {
        SyntaxError(lexer, key, "Expected identifier!");
    }
    if (key.value != "ModuleType") {
        SyntaxError(lexer, key, "Unknown metadata key!");
    }
    EnsureToken(lexer, Token::Assign);
    db.setModuleType(common::FromString<ModuleType>(GetString(lexer)));
}

static auto ParseParamDefineSet(std::vector<Param>& defines, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "ParamDefines");
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        const auto value = lexer.next();
        switch (value.type) {
            case Token::Identifier:
                if (value.value == "true") {
                    defines.emplace_back(key.value, ParamType::Bool, true);
                } else if (value.value == "false") {
                    defines.emplace_back(key.value, ParamType::Bool, false);
                } else {
                    defines.emplace_back(key.value, ParamType::String, std::make_unique<std::string>(value.value));
                }
                break;
            case Token::IntegerBin: {
                defines.emplace_back(key.value, ParamType::S32, ToS32(value.value, 2));
                break;
            }
            case Token::IntegerDec: {
                defines.emplace_back(key.value, ParamType::S32, ToS32(value.value, 10));
                break;
            }
            case Token::IntegerHex: {
                defines.emplace_back(key.value, ParamType::Enum, EnumValue(ToU32(value.value, 16)));
                break;
            }
            case Token::Float: {
                defines.emplace_back(key.value, ParamType::F32, ToF32(value.value));
                break;
            }
            case Token::String: {
                defines.emplace_back(key.value, ParamType::String, std::make_unique<std::string>(Unescape(value.value)));
                break;
            }
            case Token::LessThan: {
                const auto null = lexer.next();
                if (null.type != Token::Identifier || null.value != "custom") {
                    SyntaxError(lexer, null, "Expected <custom>!");
                }
                EnsureToken(lexer, Token::GreaterThan);
                defines.emplace_back(key.value, ParamType::Custom, std::unique_ptr<ArrangeParam>());
                break;
            }
            default:
                SyntaxError(lexer, value,  "Failed to parse param define!");
        }
    }
}

static auto ParseParamDefines(ParamDefineTable& pdt, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "ParamDefineTable");
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Expected ParamDefine category identifier!");
        }

        if (key.value == "SystemUserParams") {
            ParseParamDefineSet(pdt.getSystemUserParams(), lexer);
        } else if (key.value == "CustomUserParams") {
            ParseParamDefineSet(pdt.getCustomUserParams(), lexer);
        } else if (key.value == "SystemAssetParams") {
            ParseParamDefineSet(pdt.getSystemAssetParams(), lexer);
        } else if (key.value == "CustomAssetParams") {
            ParseParamDefineSet(pdt.getCustomAssetParams(), lexer);
        } else if (key.value == "TriggerParams") {
            ParseParamDefineSet(pdt.getTriggerParams(), lexer);
        } else {
            SyntaxError(lexer, key, "Unknown ParamDefine category! {}", key.value);
        }
    }
}

[[nodiscard]] static auto ParseRandom(Lexer& lexer) -> std::unique_ptr<RandomTable> {
    auto scope = Scope(lexer, "Random");
    auto random = std::make_unique<RandomTable>();
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        if (key.value == "Type") {
            random->setType(common::FromString<RandomType>(GetIdentifier(lexer)));
        } else if (key.value == "Power") {
            random->setPower(GetFloat(lexer));
        } else if (key.value == "Min") {
            random->setMin(GetFloat(lexer));
        } else if (key.value == "Max") {
            random->setMax(GetFloat(lexer));
        } else {
            SyntaxError(lexer, key, "Unknown random attribute!");
        }
    }
    return random;
}

static auto ParseCurvePoints(std::vector<CurvePoint>& points, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "CurvePoints");
    while (scope) {
        const auto token = scope.get();
        if (token.type != Token::ParenthesisOpen) {
            SyntaxError(lexer, token, "Expected opening parentheses");
        }

        const auto x = GetFloat(lexer);
        EnsureToken(lexer, Token::Comma);
        const auto y = GetFloat(lexer);
        EnsureToken(lexer, Token::ParenthesisClose);
        points.emplace_back(x, y);
    }
}

[[nodiscard]] static auto ParseCurve(Lexer& lexer) -> std::unique_ptr<Curve> {
    auto scope = Scope(lexer, "Curve");
    auto curve = std::make_unique<Curve>();
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        if (key.value == "Type") {
            curve->setType(common::FromString<CurveType>(GetIdentifier(lexer)));
        } else if (key.value == "Property") {
            curve->setPropertyScope(common::FromString<PropertyScope>(GetIdentifier(lexer)));
            if (const auto token = lexer.next(); token.type != Token::Scope) {
                SyntaxError(lexer, token, "Expected :: as a separator!");
            }
            curve->setPropertyName(GetString(lexer));
        } else if (key.value == "Unknown") {
            curve->setUnknown(GetInt(lexer));
        } else if (key.value == "UpdateType") {
            curve->setUpdateType(common::FromString<CurveUpdateMode>(GetIdentifier(lexer)));
        } else if (key.value == "Points") {
            ParseCurvePoints(curve->getPoints(), lexer);
        } else {
            SyntaxError(lexer, key, "Unknown curve attribute!");
        }
    }
    return curve;
}

static auto ParseArrangeGroup(ArrangeGroup& group, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "ArrangeGroup");
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        if (key.value == "LimitType") {
            group.setLimitType(common::FromString<LimitType>(GetIdentifier(lexer)));
        } else if (key.value == "Threshold") {
            group.setThreshold(static_cast<std::int8_t>(GetInt(lexer)));
        } else if (key.value == "IncludeFading") {
            group.setIncludeFading(GetBool(lexer));
        } else {
            SyntaxError(lexer, key, "Unknown arrange group attribute");
        }
    }
}

[[nodiscard]] static auto ParseArrangeParam(Lexer& lexer) -> std::unique_ptr<ArrangeParam> {
    auto scope = Scope(lexer, "ArrangeParam");
    auto param = std::make_unique<ArrangeParam>();
    while (scope) {
        const auto key = scope.get();
        auto& group = param->emplace_back();
        if (key.type == Token::String) {
            group.setName(Unescape(key.value));
        } else if (HasValue(key.type)) {
            group.setName(key.value);
        } else {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        ParseArrangeGroup(group, lexer);
    }
    return param;
}

static auto ParseParams(std::vector<Param>& params, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "Params");
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        const auto value = lexer.next();
        switch (value.type) {
            case Token::Identifier:
                if (value.value == "true") {
                    params.emplace_back(key.value, ParamType::Bool, true);
                } else if (value.value == "false") {
                    params.emplace_back(key.value, ParamType::Bool, false);
                } else if (value.value == "RANDOM") {
                    params.emplace_back(key.value, ParamType::F32, ParseRandom(lexer));
                } else if (value.value == "CURVE") {
                    params.emplace_back(key.value, ParamType::F32, ParseCurve(lexer));
                } else if (value.value == "ARRANGE") {
                    params.emplace_back(key.value, ParamType::Custom, ParseArrangeParam(lexer));
                } else {
                    params.emplace_back(key.value, ParamType::String, std::make_unique<std::string>(value.value));
                }
                break;
            case Token::IntegerBin: {
                params.emplace_back(key.value, ParamType::S32, BitfieldValue(ToU32(value.value, 2)));
                break;
            }
            case Token::IntegerDec: {
                params.emplace_back(key.value, ParamType::S32, ToS32(value.value, 10));
                break;
            }
            case Token::IntegerHex: {
                params.emplace_back(key.value, ParamType::Enum, EnumValue(ToU32(value.value, 16)));
                break;
            }
            case Token::Float: {
                params.emplace_back(key.value, ParamType::F32, ToF32(value.value));
                break;
            }
            case Token::String: {
                params.emplace_back(key.value, ParamType::String, std::make_unique<std::string>(Unescape(value.value)));
                break;
            }
            default:
                SyntaxError(lexer, value,  "Failed to parse param!");
        }
    }
}

static auto ParseLocalProperties(std::set<std::string>& props, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "Properties");
    while (scope) {
        const auto prop = scope.get();
        if (prop.type == Token::String) {
            props.emplace(Unescape(prop.value));
        } else if (HasValue(prop.type)) {
            props.emplace(prop.value);
        } else {
            SyntaxError(lexer, prop, "Failed to parse local property!");
        }
    }
}

static auto ParseActionTrigger(ActionTrigger& trigger, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "ActionTrigger");
    auto foundAsset = false;
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        if (key.value == "Type") {
            trigger.setType(common::FromString<ActionTriggerType>(GetIdentifier(lexer)));
        } else if (key.value == "Start") {
            trigger.setStartFrame(GetInt(lexer));
        } else if (key.value == "End") {
            trigger.setEndFrame(GetInt(lexer));
        } else if (key.value == "PreviousAction") {
            trigger.setPreviousAction(GetString(lexer));
        } else if (key.value == "Unknown1") {
            trigger.setUnknown1(GetUInt(lexer));
        } else if (key.value == "Unknown2") {
            trigger.setUnknown2(static_cast<std::uint16_t>(GetUInt(lexer)));
        } else if (key.value == "Oneshot") {
            trigger.setOneShot(GetIdentifier(lexer) == "true");
        } else if (key.value == "Asset") {
            if (foundAsset) {
                SyntaxError(lexer, key, "Redefinition of trigger asset!");
            }
            foundAsset = true;
            const auto keyname = lexer.next();
            if (!HasValue(keyname.type)) {
                SyntaxError(lexer, keyname, "Invalid asset call table name!");
            }
            EnsureToken(lexer, Token::BracketOpen);
            info.emplace_back(GetUInt(lexer));
            EnsureToken(lexer, Token::BracketClose);
        } else if (key.value == "OverwriteParams") {
            ParseParams(trigger.getOverwriteParams(), lexer);
        } else {
            SyntaxError(lexer, key, "Unknown action trigger attribute!");
        }
    }
    if (!foundAsset) {
        SyntaxError(lexer, Token{}, "Property trigger is missing asset!");
    }
}

static auto ParseActionTriggers(std::vector<ActionTrigger>& triggers, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "ActionTriggers");
    while (scope) {
        const auto guid = scope.get();
        if (guid.type != Token::IntegerHex) {
            SyntaxError(lexer, guid, "Failed to parse action trigger GUID!");
        }

        auto& trigger = triggers.emplace_back();
        trigger.setGUID(ToU32(guid.value, 16));
        ParseActionTrigger(trigger, lexer, info);
    }
}

static auto ParseActions(std::vector<Action>& actions, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "Actions");
    auto isPrefix = false;
    while (scope) {
        const auto action = scope.get();
        if (action.type == Token::String) {
            actions.emplace_back(Unescape(action.value));
        } else if (HasValue(action.type)) {
            actions.emplace_back(action.value);
        } else if (action.type == Token::At) {
            const auto key = GetIdentifier(lexer);
            if (key == "Prefix") {
                isPrefix = true;
            }
            continue;
        } else {
            SyntaxError(lexer, action, "Failed to parse action name!");
        }

        actions.back().setIsPrefix(isPrefix);
        ParseActionTriggers(actions.back().getTriggers(), lexer, info);
        isPrefix = false;
    }
}

static auto ParseActionSlots(std::vector<ActionSlot>& slots, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "ActionSlots");
    while (scope) {
        const auto slot = scope.get();
        if (slot.type == Token::String) {
            slots.emplace_back(Unescape(slot.value));
        } else if (HasValue(slot.type)) {
            slots.emplace_back(slot.value);
        } else {
            SyntaxError(lexer, slot, "Failed to parse action slot name!");
        }

        ParseActions(slots.back().getActions(), lexer, info);
    }
}

static auto ParsePropertyTrigger(PropertyTrigger& trigger, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "PropertyTrigger");
    auto foundAsset = false;
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        if (key.value == "Lazy") {
            trigger.setLazy(GetIdentifier(lexer) == "true");
        } else if (key.value == "Unknown") {
            trigger.setUnknown(static_cast<std::uint16_t>(GetUInt(lexer)));
        } else if (key.value == "Asset") {
            if (foundAsset) {
                SyntaxError(lexer, key, "Redefinition of trigger asset!");
            }
            foundAsset = true;
            const auto keyname = lexer.next();
            if (!HasValue(keyname.type)) {
                SyntaxError(lexer, keyname, "Invalid asset call table name!");
            }
            EnsureToken(lexer, Token::BracketOpen);
            info.emplace_back(GetUInt(lexer));
            EnsureToken(lexer, Token::BracketClose);
        } else if (key.value == "OverwriteParams") {
            ParseParams(trigger.getOverwriteParams(), lexer);
        } else {
            SyntaxError(lexer, key, "Unknown property trigger attribute!");
        }
    }
    if (!foundAsset) {
        SyntaxError(lexer, Token{}, "Property trigger is missing asset!");
    }
}

static auto ParseSwitchCondition(SwitchCondition& cond, Lexer& lexer, std::string_view varName) -> void {
    if (const auto token = lexer.next(); token.type == Token::Identifier && token.value == "_") {
        cond.setCompareType(CompareType::Default);
        return;
    } else if (token.type != Token::LessThan) {
        SyntaxError(lexer, token, "Expected <value>");
    }

    if (const auto id = GetIdentifier(lexer); id != "value") {
        SyntaxError(lexer, Token{ Token::Identifier, id }, "Expected <value> in if clause!");
    }
    EnsureToken(lexer, Token::GreaterThan);

    const auto compareOp = lexer.next();
    switch (compareOp.type) {
        case Token::Equal:
            cond.setCompareType(CompareType::Equal);
            break;
        case Token::NotEqual:
            cond.setCompareType(CompareType::NotEqual);
            break;
        case Token::LessThan:
            cond.setCompareType(CompareType::LessThan);
            break;
        case Token::GreaterThan:
            cond.setCompareType(CompareType::GreaterThan);
            break;
        case Token::LessThanEqual:
            cond.setCompareType(CompareType::LessThanOrEqual);
            break;
        case Token::GreaterThanEqual:
            cond.setCompareType(CompareType::GreaterThanOrEqual);
            break;
        default:
            SyntaxError(lexer, compareOp, "Expected comparison operator!");
    }

    const auto value = lexer.next();
    switch (value.type) {
        case Token::Identifier:
            if (value.value == "true") {
                cond.setValue(true);
            } else if (value.value == "false") {
                cond.setValue(false);
            } else {
                if (value.value != varName) {
                    SyntaxError(lexer, value, "Name does not match variable name!");
                }
                EnsureToken(lexer, Token::Scope);
                cond.setValue(Unescape(GetString(lexer)));
            }
            break;
        case Token::IntegerBin:
            cond.setValue(ToS32(value.value, 2));
            break;
        case Token::IntegerDec:
            cond.setValue(ToS32(value.value, 10));
            break;
        case Token::IntegerHex:
            cond.setValue(ToS32(value.value, 16));
            break;
        case Token::Float:
            cond.setValue(ToF32(value.value));
            break;
        case Token::String: {
            if (Unescape(value.value) != varName) {
                SyntaxError(lexer, value, "Name does not match variable name!");
            }
            EnsureToken(lexer, Token::Scope);
            cond.setValue(Unescape(GetString(lexer)));
            break;
        }
        case Token::LessThan: {
            if (const auto id = GetIdentifier(lexer); id != "action") {
                SyntaxError(lexer, value, "Expected <action>");
            }
            EnsureToken(lexer, Token::GreaterThan);
            EnsureToken(lexer, Token::Scope);
            if (cond.getParentType() != SwitchType::ActionSlot) {
                SyntaxError(lexer, value, "Action conditions must be in an action switch!");
            }
            cond.setValue(Unescape(GetString(lexer)));
            break;
        }
        default:
            SyntaxError(lexer, value, "Expected value for comparison!");
    }
}

static auto ParsePropertyTriggers(Property& prop, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "PropertyTriggers");
    while (scope) {
        const auto token = scope.get();
        if (token.type != Token::Identifier || token.value != "if") {
            SyntaxError(lexer, token, "Expected `if` at the start of property trigger");
        }

        auto& trigger = prop.addTrigger();
        auto& cond = trigger.getCondition();
        cond.setParentType(prop.getScope() == PropertyScope::Global ? SwitchType::GlobalProperty : SwitchType::LocalProperty);

        ParseSwitchCondition(cond, lexer, prop.getName());
        if (cond.getCompareType() == CompareType::Default) {
            SyntaxError(lexer, token, "Property triggers cannot use default conditions!");
        }

        EnsureToken(lexer, Token::Arrow);

        trigger.setGUID(GetUInt(lexer));
        ParsePropertyTrigger(trigger, lexer, info);
    }
}

static auto ParseProperties(std::vector<Property>& props, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "Properties");
    while (scope) {
        const auto s = scope.get();
        if (s.type != Token::Identifier) {
            SyntaxError(lexer, s, "Failed to parse property scope!");
        }
        auto& prop = props.emplace_back();
        prop.setScope(common::FromString<PropertyScope>(s.value));
        EnsureToken(lexer, Token::Scope);
        prop.setName(GetString(lexer));

        ParsePropertyTriggers(prop, lexer, info);
    }
}

static auto ParseAlwaysTrigger(AlwaysTrigger& trigger, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "AlwaysTrigger");
    auto foundAsset = false;
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        if (key.value == "Flags") {
            trigger.setFlags(static_cast<std::uint16_t>(GetUInt(lexer)));
        } else if (key.value == "Unknown") {
            trigger.setUnknown(static_cast<std::uint16_t>(GetUInt(lexer)));
        } else if (key.value == "Asset") {
            if (foundAsset) {
                SyntaxError(lexer, key, "Redefinition of trigger asset!");
            }
            foundAsset = true;
            const auto keyname = lexer.next();
            if (!HasValue(keyname.type)) {
                SyntaxError(lexer, keyname, "Invalid asset call table name!");
            }
            EnsureToken(lexer, Token::BracketOpen);
            info.emplace_back(GetUInt(lexer));
            EnsureToken(lexer, Token::BracketClose);
        } else if (key.value == "OverwriteParams") {
            ParseParams(trigger.getOverwriteParams(), lexer);
        } else {
            SyntaxError(lexer, key, "Unknown always trigger attribute!");
        }
    }
    if (!foundAsset) {
        SyntaxError(lexer, Token{}, "Always trigger is missing asset!");
    }
}

static auto ParseAlwaysTriggers(std::vector<AlwaysTrigger>& triggers, Lexer& lexer, std::vector<std::uint32_t>& info) -> void {
    auto scope = Scope(lexer, "AlwaysTriggers");
    while (scope) {
        const auto token = scope.get();
        auto& trigger = triggers.emplace_back();
        if (token.type == Token::IntegerHex) {
            trigger.setGUID(ToU32(token.value, 16));
        } else if (token.type == Token::IntegerBin) {
            trigger.setGUID(ToU32(token.value, 2));
        } else if (token.type == Token::IntegerDec) {
            trigger.setGUID(ToU32(token.value, 10));
        } else {
            SyntaxError(lexer, token, "Failed to parse GUID for always trigger!");
        }

        ParseAlwaysTrigger(trigger, lexer, info);
    }
}

[[nodiscard]] static auto ParseAssetCallTable(
    const std::string& keyname,
    std::uint32_t guid,
    Lexer& lexer,
    std::unordered_map<std::uint32_t, AssetCallTableHandle>& callTables,
    std::vector<std::pair<std::reference_wrapper<AssetCallTableHandle>, std::uint32_t>>& deferred
) -> AssetCallTableHandle;

[[nodiscard]] static auto ParseCallTableKey(Lexer& lexer) -> std::tuple<std::string, std::uint32_t, bool> {
    const auto key = lexer.next();
    std::string keyname;
    if (key.type == Token::String) {
        keyname = Unescape(key.value);
    } else if (HasValue(key.type)) {
        keyname = key.value;
    } else if (key.type == Token::LessThan) {
        if (const auto id = GetIdentifier(lexer); id == "null") {
            EnsureToken(lexer, Token::GreaterThan);
            return std::make_tuple(std::string(""), 0u, true);
        }
    } else {
        SyntaxError(lexer, key, "Failed to parse to asset call table key!");
    }
    EnsureToken(lexer, Token::BracketOpen);
    const auto guid = GetUInt(lexer);
    EnsureToken(lexer, Token::BracketClose);
    return std::make_tuple(std::move(keyname), guid, false);
}

[[nodiscard]] static auto ParseSwitchVariable(Lexer& lexer) -> std::pair<SwitchType, std::string> {
    const auto token = lexer.next();
    SwitchType type = SwitchType::Null;
    if (token.type == Token::LessThan) {
        if (const auto id = GetIdentifier(lexer); id == "null") {
            EnsureToken(lexer, Token::GreaterThan);
            return std::make_pair(SwitchType::Null, "");
        }
        SyntaxError(lexer, token, "Expected identifier for switch variable!");
    } else if (token.type != Token::Identifier) {
        SyntaxError(lexer, token, "Expected identifier for switch variable!");
    }
    if (token.value == "Local") {
        type = SwitchType::LocalProperty;
    } else if (token.value == "Global") {
        type = SwitchType::GlobalProperty;
    } else if (token.value == "ActionSlot") {
        type = SwitchType::ActionSlot;
    } else {
        SyntaxError(lexer, token, "Unknown property scope!");
    }
    EnsureToken(lexer, Token::Scope);
    return std::make_pair(type, GetString(lexer));
}

[[nodiscard]] static auto ParseEnumValue(Lexer& lexer) -> std::pair<std::string, std::string> {
    const auto name = GetString(lexer);
    EnsureToken(lexer, Token::Scope);
    const auto value = GetString(lexer);
    return std::make_pair(std::move(name), std::move(value));
}

[[nodiscard]] static auto ParseBlendByCondition(Lexer& lexer, const char* type) -> std::pair<float, BlendOp> {
    auto value = 0.f;
    auto op = BlendOp::None;
    EnsureToken(lexer, Token::BracketOpen);
    const auto key1 = GetIdentifier(lexer);
    EnsureToken(lexer, Token::Colon);
    if (key1 == type) {
        value = GetFloat(lexer);
    } else if (key1 == "Op") {
        op = common::FromString<BlendOp>(GetIdentifier(lexer));
    }
    EnsureToken(lexer, Token::Comma);
    const auto key2 = GetIdentifier(lexer);
    EnsureToken(lexer, Token::Colon);
    if (key2 == type) {
        value = GetFloat(lexer);
    } else if (key2 == "Op") {
        op = common::FromString<BlendOp>(GetIdentifier(lexer));
    }
    EnsureToken(lexer, Token::BracketClose);
    return std::make_pair(value, op);
}

static auto ParseGridCases(
    Grid& grid,
    Lexer& lexer,
    std::unordered_map<std::string, std::uint32_t>& gridMapping
) -> void {
    auto scope = Scope(lexer, "GridCases");
    while (scope) {
        const auto token = scope.get();
        if (token.type != Token::ParenthesisOpen) {
            SyntaxError(lexer, token, "Expected opening parenthesis for grid condition");
        }
        const auto [name1, val1] = ParseEnumValue(lexer);
        EnsureToken(lexer, Token::Comma);
        const auto [name2, val2] = ParseEnumValue(lexer);
        EnsureToken(lexer, Token::ParenthesisClose);
        if (name1 != grid.getProperty1Name() || name2 != grid.getProperty2Name()) {
            SyntaxError(lexer, token, "Grid value condition does not match name of variable!");
        }
        if (const auto res = std::find(grid.getValues1().begin(), grid.getValues1().end(), val1); res == grid.getValues1().end()) {
            grid.getValues1().push_back(val1);
        }
        if (const auto res = std::find(grid.getValues2().begin(), grid.getValues2().end(), val2); res == grid.getValues2().end()) {
            grid.getValues2().push_back(val2);
        }
        EnsureToken(lexer, Token::Arrow);
        const auto [keyname, guid, null] = ParseCallTableKey(lexer);
        if (null) {
            continue;
        }
        gridMapping.emplace(val1 + val2, guid);
    }
}

static auto ParseGridChildren(
    Grid& grid,
    std::unordered_map<std::uint32_t, AssetCallTableHandle>& children,
    Lexer& lexer,
    std::unordered_map<std::uint32_t, AssetCallTableHandle>& callTables,
    std::vector<std::pair<std::reference_wrapper<AssetCallTableHandle>, std::uint32_t>>& deferred
) -> void {
    auto scope = Scope(lexer, "GridChildren");
    while (scope) {
        const auto key = scope.get();
        std::string keyname;
        if (key.type == Token::String) {
            keyname = Unescape(key.value);
        } else if (HasValue(key.type)) {
            keyname = key.value;
        } else {
            SyntaxError(lexer, key, "Failed to parse asset call table keyname!");
        }

        EnsureToken(lexer, Token::BracketOpen);
        const auto guid = GetUInt(lexer);
        EnsureToken(lexer, Token::BracketClose);
        if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
            grid.addChild(res);
            children.emplace(res->getGUID(), res);
        } else {
            SyntaxError(lexer, key, "Grid container children cannot be declared without a body! {}[{:#010x}]", keyname, guid);
        }
    }
}

static auto ParseContainer(
    AssetCallTable& act,
    Lexer& lexer,
    std::unordered_map<std::uint32_t, AssetCallTableHandle>& callTables,
    std::vector<std::pair<std::reference_wrapper<AssetCallTableHandle>, std::uint32_t>>& deferred
) -> void {
    switch (act.getType()) {
        case CallTableType::Switch: {
            EnsureToken(lexer, Token::ParenthesisOpen);
            const auto [type, prop] = ParseSwitchVariable(lexer);
            EnsureToken(lexer, Token::ParenthesisClose);
            auto scope = Scope(lexer, "Switch");
            auto& switch_ = static_cast<Switch&>(act);
            switch_.setSwitchType(type);
            switch_.setSwitchVariable(prop);
            while (scope) {
                const auto token = scope.get();
                if (token.type != Token::ParenthesisOpen) {
                    SyntaxError(lexer, token, "Expected opening parenthesis!");
                }
                auto cond = std::make_unique<SwitchCondition>();
                cond->setParentType(switch_.getSwitchType());
                ParseSwitchCondition(*cond, lexer, prop);
                EnsureToken(lexer, Token::ParenthesisClose);
                EnsureToken(lexer, Token::Arrow);
                const auto [keyname, guid, null] = ParseCallTableKey(lexer);
                if (null) {
                    SyntaxError(lexer, Token{}, "Switch case cannot have a null call table!");
                }
                if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
                    switch_.addChild(res);
                    res->setCondition(std::move(cond));
                } else {
                    SyntaxError(lexer, token, "Switch container children cannot be declared without a body! Use a jump container to jump if this is desired");
                }
            }
            break;
        }
        case CallTableType::Random: {
            auto scope = Scope(lexer, "Random");
            auto& random = static_cast<Random&>(act);
            while (scope) {
                const auto token = scope.get();
                float weight;
                if (token.type == Token::Float) {
                    weight = ToF32(token.value);
                } else if (token.type == Token::IntegerDec) {
                    weight = static_cast<float>(ToS32(token.value, 10));
                } else {
                    SyntaxError(lexer, token, "Failed to parse weight for random container child!");
                }
                EnsureToken(lexer, Token::Arrow);
                const auto [keyname, guid, null] = ParseCallTableKey(lexer);
                if (null) {
                    SyntaxError(lexer, Token{}, "Random cannot have a null call table!");
                }
                if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
                    random.addChild(res);
                    auto cond = std::make_unique<RandomCondition>();
                    cond->setWeight(weight);
                    res->setCondition(std::move(cond));
                } else {
                    SyntaxError(lexer, token, "Random container children cannot be declared without a body! Use a jump container to jump if this is desired");
                }
            }
            break;
        }
        case CallTableType::RandomNoRepeat: {
            auto scope = Scope(lexer, "RandomNoRepeat");
            auto& random = static_cast<RandomNoRepeat&>(act);
            while (scope) {
                const auto token = scope.get();
                float weight;
                if (token.type == Token::Float) {
                    weight = ToF32(token.value);
                } else if (token.type == Token::IntegerDec) {
                    weight = static_cast<float>(ToS32(token.value, 10));
                } else {
                    SyntaxError(lexer, token, "Failed to parse weight for random no repeat container child!");
                }
                EnsureToken(lexer, Token::Arrow);
                const auto [keyname, guid, null] = ParseCallTableKey(lexer);
                if (null) {
                    SyntaxError(lexer, Token{}, "Random no repeat cannot have a null call table!");
                }
                if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
                    random.addChild(res);
                    auto cond = std::make_unique<RandomNoRepeatCondition>();
                    cond->setWeight(weight);
                    res->setCondition(std::move(cond));
                } else {
                    SyntaxError(lexer, token, "Random no repeat container children cannot be declared without a body! Use a jump container to jump if this is desired");
                }
            }
            break;
        }
        case CallTableType::Blend: {
            auto scope = Scope(lexer, "Blend");
            auto& blend = static_cast<Blend&>(act);
            while (scope) {
                const auto token = scope.get();
                std::string keyname;
                if (token.type == Token::String) {
                    keyname = Unescape(token.value);
                } else if (HasValue(token.type)) {
                    keyname = token.value;
                } else {
                    SyntaxError(lexer, token, "Failed to parse call table key");
                }
                EnsureToken(lexer, Token::BracketOpen);
                const auto guid = GetUInt(lexer);
                EnsureToken(lexer, Token::BracketClose);
                if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
                    blend.addChild(res);
                } else {
                    SyntaxError(lexer, token, "Blend container children cannot be declared without a body! Use a jump container to jump if this is desired");
                }
            }
            break;
        }
        case CallTableType::BlendBy: {
            EnsureToken(lexer, Token::ParenthesisOpen);
            const auto [type, prop] = ParseSwitchVariable(lexer);
            if (type == SwitchType::ActionSlot) {
                SyntaxError(lexer, Token{}, "Blend by containers do not support actions!");
            } else if (type == SwitchType::Null) {
                SyntaxError(lexer, Token{}, "Blend by containers do not support null conditions!");
            }
            EnsureToken(lexer, Token::ParenthesisClose);
            auto scope = Scope(lexer, "BlendBy");
            auto& blend = static_cast<BlendBy&>(act);
            blend.setBlendPropertyName(prop);
            blend.setBlendPropertyScope(type == SwitchType::GlobalProperty ? PropertyScope::Global : PropertyScope::Local);
            while (scope) {
                const auto token = scope.get();
                if (token.type != Token::ParenthesisOpen) {
                    SyntaxError(lexer, token, "Expected opening parenthesis for blend by condition");
                }
                const auto [min, minOp] = ParseBlendByCondition(lexer, "Min");
                EnsureToken(lexer, Token::Comma);
                const auto [max, maxOp] = ParseBlendByCondition(lexer, "Max");
                EnsureToken(lexer, Token::ParenthesisClose);
                EnsureToken(lexer, Token::Arrow);
                const auto [keyname, guid, null] = ParseCallTableKey(lexer);
                if (null) {
                    SyntaxError(lexer, Token{}, "Blend by cannot have a null call table!");
                }
                if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
                    blend.addChild(res);
                    auto cond = std::make_unique<BlendByCondition>();
                    cond->setValueMin(min);
                    cond->setBlendOpMin(minOp);
                    cond->setValueMax(max);
                    cond->setBlendOpMax(maxOp);
                    res->setCondition(std::move(cond));
                } else {
                    SyntaxError(lexer, token, "Blend by container children cannot be declared without a body! Use a jump container to jump if this is desired");
                }
            }
            break;
        }
        case CallTableType::Sequence: {
            auto scope = Scope(lexer, "Sequence");
            auto& seq = static_cast<Sequence&>(act);
            auto foundForceContinue = false;
            auto forceContinue = 0u;
            while (scope) {
                const auto token = scope.get();
                std::string keyname;
                if (token.type == Token::String) {
                    keyname = Unescape(token.value);
                } else if (HasValue(token.type)) {
                    keyname = token.value;
                } else if (token.type == Token::At) {
                    if (const auto id = GetIdentifier(lexer); id == "ForceContinue") {
                        EnsureToken(lexer, Token::Assign);
                        forceContinue = GetUInt(lexer);
                        foundForceContinue = true;
                        continue;
                    }
                    SyntaxError(lexer, token, "Failed to parse call table key");
                } else {
                    SyntaxError(lexer, token, "Failed to parse call table key");
                }
                EnsureToken(lexer, Token::BracketOpen);
                const auto guid = GetUInt(lexer);
                EnsureToken(lexer, Token::BracketClose);
                if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
                    seq.addChild(res);
                    if (foundForceContinue) {
                        auto cond = std::make_unique<SequenceCondition>();
                        cond->setForceContinue(forceContinue);
                        forceContinue = 0u;
                        res->setCondition(std::move(cond));
                        foundForceContinue = false;
                    }
                } else {
                    SyntaxError(lexer, token, "Sequence container children cannot be declared without a body! Use a jump container to jump if this is desired");
                }
            }
            break;
        }
        case CallTableType::Grid: {
            EnsureToken(lexer, Token::ParenthesisOpen);
            const auto [type1, prop1] = ParseSwitchVariable(lexer);
            if (type1 == SwitchType::ActionSlot) {
                SyntaxError(lexer, Token{}, "Grid containers do not support actions!");
            } else if (type1 == SwitchType::Null) {
                SyntaxError(lexer, Token{}, "Grid containers do not support null conditions!");
            }
            EnsureToken(lexer, Token::Comma);
            const auto [type2, prop2] = ParseSwitchVariable(lexer);
            if (type2 == SwitchType::ActionSlot) {
                SyntaxError(lexer, Token{}, "Grid containers do not support actions!");
            } else if (type2 == SwitchType::Null) {
                SyntaxError(lexer, Token{}, "Grid containers do not support null conditions!");
            }
            EnsureToken(lexer, Token::ParenthesisClose);
            auto scope = Scope(lexer, "Grid");
            auto& grid = static_cast<Grid&>(act);
            grid.setProperty1Name(prop1);
            grid.setProperty1Scope(type1 == SwitchType::GlobalProperty ? PropertyScope::Global : PropertyScope::Local);
            grid.setProperty2Name(prop2);
            grid.setProperty2Scope(type2 == SwitchType::GlobalProperty ? PropertyScope::Global : PropertyScope::Local);
            auto gridMapping = std::unordered_map<std::string, std::uint32_t>{};
            auto children = std::unordered_map<std::uint32_t, AssetCallTableHandle>{};
            while (scope) {
                const auto token = scope.get();
                if (token.type != Token::Identifier) {
                    SyntaxError(lexer, token, "Expected identifier for grid body! Either Cases or Children");
                }

                if (token.value == "Cases") {
                    if (!gridMapping.empty()) {
                        SyntaxError(lexer, token, "Redeclaration of grid cases!");
                    }
                    ParseGridCases(grid, lexer, gridMapping);
                } else if (token.value == "Children") {
                    if (!children.empty()) {
                        SyntaxError(lexer, token, "Redeclaration of grid children!");
                    }
                    ParseGridChildren(grid, children, lexer, callTables, deferred);
                } else {
                    SyntaxError(lexer, token, "Unknown grid attribute!");
                }
            }
            for (const auto& v1 : grid.getValues1()) {
                for (const auto& v2 : grid.getValues2()) {
                    if (const auto res = gridMapping.find(v1 + v2); res == gridMapping.end()) {
                        // default to null
                        grid.getAssetCallTables().emplace_back(AssetCallTableHandle{ nullptr });
                    } else {
                        if (const auto res1 = children.find(res->second); res1 == children.end()) {
                            SyntaxError(lexer, Token{}, "Asset call table {:#010x} does not exist in grid! Use a jump container if the target elsewhere", res->second);
                        } else {
                            grid.getAssetCallTables().emplace_back(res1->second);
                        }
                    }
                }
            }
            break;
        }
        case CallTableType::Jump: {
            auto& jump = static_cast<Jump&>(act);
            EnsureToken(lexer, Token::Arrow);
            const auto [keyname, guid, null] = ParseCallTableKey(lexer);
            if (null) {
                break;
            }
            if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
                jump.addChild(res);
            } else {
                jump.addChild(AssetCallTableHandle{ nullptr });
                deferred.emplace_back(std::ref(jump.getChildren().front()), guid);
            }
            break;
        }
        case CallTableType::Asset: {
            auto& asset = static_cast<Asset&>(act);
            ParseParams(asset.getParams(), lexer);
            break;
        }
        default:
            std::unreachable();
    }
}

static auto ParseAssetCallTable(
    const std::string& keyname,
    std::uint32_t guid,
    Lexer& lexer,
    std::unordered_map<std::uint32_t, AssetCallTableHandle>& callTables,
    std::vector<std::pair<std::reference_wrapper<AssetCallTableHandle>, std::uint32_t>>& deferred
) -> AssetCallTableHandle {
    auto scope = Scope(lexer, "AssetCallTable");
    if (!scope) {
        return AssetCallTableHandle{ nullptr };
    }

    if (callTables.contains(guid)) {
        SyntaxError(lexer, Token{}, "Redefinition of asset call table {}[{:#010x}]!", keyname, guid);
    }

    auto act = AssetCallTableHandle{ nullptr };
    auto emitCount = 1;
    auto oneshot = false;
    auto nopause = false;
    auto userFlags = std::uint8_t(0);
    auto unknown = -1;
    auto children = std::vector<AssetCallTableHandle>{};
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier && key.type != Token::At) {
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        } else if (key.type == Token::At) {
            if (const auto id = GetIdentifier(lexer); id == "Unknown") {
                EnsureToken(lexer, Token::Assign);
                unknown = GetInt(lexer);
                continue;
            }
            SyntaxError(lexer, key, "Failed to read identifier for key-value pair!");
        }

        EnsureToken(lexer, Token::Assign);

        if (key.value == "EmitCount") {
            emitCount = GetInt(lexer);
        } else if (key.value == "Oneshot") {
            oneshot = GetIdentifier(lexer) == "true";
        } else if (key.value == "NoPause") {
            nopause = GetIdentifier(lexer) == "true";
        } else if (key.value == "UserFlags") {
            userFlags = GetUInt(lexer);
        } else if (key.value == "Execute") {
            if (act) {
                SyntaxError(lexer, key, "A call table cannot have multiple execute attributes!");
            }
            const auto type = GetIdentifier(lexer);
            if (type == "Switch") {
                act.set(std::make_shared<Switch>());
                static_cast<Switch*>(act.get())->setUnknown(unknown);
            } else if (type == "Random") {
                act.set(std::make_shared<Random>());
            } else if (type == "RandomNoRepeat") {
                act.set(std::make_shared<RandomNoRepeat>());
            } else if (type == "Blend") {
                act.set(std::make_shared<Blend>());
            } else if (type == "BlendBy") {
                act.set(std::make_shared<BlendBy>());
                static_cast<BlendBy*>(act.get())->setUnknown(unknown);
            } else if (type == "Sequence") {
                act.set(std::make_shared<Sequence>());
            } else if (type == "Grid") {
                act.set(std::make_shared<Grid>());
            } else if (type == "Jump") {
                act.set(std::make_shared<Jump>());
            } else if (type == "Asset") {
                act.set(std::make_shared<Asset>());
            } else {
                SyntaxError(lexer, Token{ Token::Identifier, type }, "Unknown call table type!");
            }
            callTables.emplace(guid, act);
            ParseContainer(*act, lexer, callTables, deferred);
        } else {
            SyntaxError(lexer, key, "Unknown call table attribute!");
        }
    }

    if (!act) {
        SyntaxError(lexer, Token{}, "Asset call table {}[{:#010x}] is missing a body!", keyname, guid);
    }

    act->setKeyName(keyname);
    act->setGUID(guid);
    act->setEmitCount(emitCount);
    act->setOneShot(oneshot);
    act->setNoPause(nopause);
    act->setUserFlags(userFlags);
    return act;
}

static auto ParseAssetCallTables(std::vector<AssetCallTableHandle>& assetCallTables, Lexer& lexer, std::unordered_map<std::uint32_t, AssetCallTableHandle>& callTables) -> void {
    auto scope = Scope(lexer, "AssetCallTables");
    auto deferred = std::vector<std::pair<std::reference_wrapper<AssetCallTableHandle>, std::uint32_t>>{};
    while (scope) {
        const auto key = scope.get();
        std::string keyname;
        if (key.type == Token::String) {
            keyname = Unescape(key.value);
        } else if (HasValue(key.type)) {
            keyname = key.value;
        } else {
            SyntaxError(lexer, key, "Failed to parse asset call table keyname!");
        }

        EnsureToken(lexer, Token::BracketOpen);
        const auto guid = GetUInt(lexer);
        EnsureToken(lexer, Token::BracketClose);
        if (auto res = ParseAssetCallTable(keyname, guid, lexer, callTables, deferred); res) {
            assetCallTables.emplace_back(std::move(res));
        } else {
            SyntaxError(lexer, key, "A top-level call table cannot be declared without a body! {}[{:#010x}]", keyname, guid);
        }
    }

    for (auto& [act, guid] : deferred) {
        if (const auto res = callTables.find(guid); res != callTables.end()) {
            act.get() = res->second;
        } else {
            SyntaxError(lexer, Token{}, "No matching call table for GUID {:#010x}!", guid);
        }
    }
}

static auto ParseUser(User& user, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "User");
    auto triggerCallTableGuids = std::vector<std::uint32_t>{};
    auto assetCallTables = std::unordered_map<std::uint32_t, AssetCallTableHandle>{};
    while (scope) {
        const auto key = scope.get();
        if (key.type != Token::Identifier) {
            SyntaxError(lexer, key, "Expected user section identifier!");
        }

        if (key.value == "Unknown") {
            EnsureToken(lexer, Token::Assign);
            user.setUnknown(GetInt(lexer));
        } else if (key.value == "UserParams") {
            ParseParams(user.getParams(), lexer);
        } else if (key.value == "LocalProperties") {
            ParseLocalProperties(user.getLocalProperties(), lexer);
        } else if (key.value == "ActionSlots") {
            ParseActionSlots(user.getActionSlots(), lexer, triggerCallTableGuids);
        } else if (key.value == "Properties") {
            ParseProperties(user.getProperties(), lexer, triggerCallTableGuids);
        } else if (key.value == "AlwaysTriggers") {
            ParseAlwaysTriggers(user.getAlwaysTriggers(), lexer, triggerCallTableGuids);
        } else if (key.value == "AssetCallTables") {
            ParseAssetCallTables(user.getAssetCallTables(), lexer, assetCallTables);
        } else {
            SyntaxError(lexer, key, "Unknown user section!");
        }
    }

    auto currTriggerIndex = std::size_t(0);
    for (auto& slot : user.getActionSlots()) {
        for (auto& action : slot.getActions()) {
            for (auto& trigger : action.getTriggers()) {
                const auto guid = triggerCallTableGuids[currTriggerIndex++];
                if (const auto res = assetCallTables.find(guid); res != assetCallTables.end()) {
                    trigger.setAssetCallTable(res->second);
                } else {
                    common::AbortWithDetail("ActionTrigger {:#010x} requests a non-existent call table {:#010x}", trigger.getGUID(), guid);
                }
            }
        }
    }
    for (auto& prop : user.getProperties()) {
        for (auto& trigger : prop.getTriggers()) {
            const auto guid = triggerCallTableGuids[currTriggerIndex++];
            if (const auto res = assetCallTables.find(guid); res != assetCallTables.end()) {
                trigger.setAssetCallTable(res->second);
            } else {
                common::AbortWithDetail("PropertyTrigger {:#010x} requests a non-existent call table {:#010x}", trigger.getGUID(), guid);
            }
        }
    }
    for (auto& trigger : user.getAlwaysTriggers()) {
        const auto guid = triggerCallTableGuids[currTriggerIndex++];
        if (const auto res = assetCallTables.find(guid); res != assetCallTables.end()) {
            trigger.setAssetCallTable(res->second);
        } else {
            common::AbortWithDetail("AlwaysTrigger {:#010x} requests a non-existent call table {:#010x}", trigger.getGUID(), guid);
        }
    }
}

static auto ParseUsers(Database& db, Lexer& lexer) -> void {
    auto scope = Scope(lexer, "Users");
    auto seen = std::unordered_map<std::uint32_t, std::reference_wrapper<const User>>{};
    while (scope) {
        const auto hashOrName = scope.get();
        auto user = std::make_unique<User>();
        // TODO: should we support base 10 hashes as well?
        if (!HasValue(hashOrName.type) || hashOrName.type == Token::Float) {
            SyntaxError(lexer, hashOrName, "Expected user hash or name!");
        } else if (hashOrName.type == Token::IntegerHex) {
            user->setHash(ToU32(hashOrName.value, 16));
        } else if (hashOrName.type == Token::String) {
            user->setName(Unescape(hashOrName.value));
        } else {
            user->setName(hashOrName.value);
        }
        const auto hash = user->getHash();
        if (const auto res = seen.find(hash); res != seen.end()) {
            const auto& existing = res->second.get();
            common::AbortWithDetail(
                "Username hash collision detected! {} ({:#010x}) and {} ({:#010x})",
                existing.hasKnownName() ? existing.getName() : "<unknown>", existing.getHash(),
                user->hasKnownName() ? user->getName() : "<unknown>", hash
            );
        }
        seen.emplace(hash, std::cref(*user));
        ParseUser(*user, lexer);
        db.addUser(std::move(user));
    }
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

static auto ValidateAndReorderUserParams(std::vector<Param>& params, const std::vector<Param>& sysDefines, const std::vector<Param>& customDefines) -> void {
    auto ordered = std::vector<Param>{};
    auto process = [&](const Param& def) {
        if (const auto res = std::find_if(params.begin(), params.end(), [&](const auto& p) { return p.getName() == def.getName(); });
            res != params.end()) {
            if (!CheckValidParamTypes(*res, def)) {
                common::AbortWithDetail("Param {} has a non-compatible type (define type: {})", res->getName(), common::ToString(def.getParamType()));
            }
            ordered.emplace_back(std::move(*res));
        } else {
            // add default because all user params must be present
            switch (def.getParamType()) {
                case ParamType::S32:
                    ordered.emplace_back(def.getName(), def.getParamType(), def.getValue<ValueType::S32>());
                    break;
                case ParamType::F32:
                    ordered.emplace_back(def.getName(), def.getParamType(), def.getValue<ValueType::F32>());
                    break;
                case ParamType::Bool:
                    ordered.emplace_back(def.getName(), def.getParamType(), def.getValue<ValueType::Bool>());
                    break;
                case ParamType::Enum:
                    ordered.emplace_back(def.getName(), def.getParamType(), EnumValue(def.getValue<ValueType::Enum>()));
                    break;
                case ParamType::String:
                    ordered.emplace_back(def.getName(), def.getParamType(), std::make_unique<std::string>(*def.getValue<ValueType::String>()));
                    break;
                case ParamType::Custom:
                    ordered.emplace_back(def.getName(), def.getParamType(), std::make_unique<ArrangeParam>());
                    break;
                default:
                    common::AbortWithDetail("Invalid param define type!");
            }
        }
    };
    for (const auto& def : sysDefines) {
        process(def);
    }
    for (const auto& def : customDefines) {
        process(def);
    }
    params.swap(ordered);
}

static auto ValidateAndReorderAssetParams(std::vector<Param>& params, const std::vector<Param>& sysDefines, const std::vector<Param>& customDefines) -> void {
    if (params.empty()) {
        return;
    }
    auto ordered = std::vector<Param>{};
    for (const auto& def : sysDefines) {
        if (const auto res = std::find_if(params.begin(), params.end(), [&](const auto& p) { return p.getName() == def.getName(); });
            res != params.end()) {
            if (!CheckValidParamTypes(*res, def)) {
                common::AbortWithDetail("Param {} has a non-compatible type (define type: {})", res->getName(), common::ToString(def.getParamType()));
            }
            ordered.emplace_back(std::move(*res));
        }
    }
    for (const auto& def : customDefines) {
        if (const auto res = std::find_if(params.begin(), params.end(), [&](const auto& p) { return p.getName() == def.getName(); });
            res != params.end()) {
            if (!CheckValidParamTypes(*res, def)) {
                common::AbortWithDetail("Param {} has a non-compatible type (define type: {})", res->getName(), common::ToString(def.getParamType()));
            }
            ordered.emplace_back(std::move(*res));
        }
    }
    params.swap(ordered);
}

static auto ValidateAndReorderTriggerParams(std::vector<Param>& params, const std::vector<Param>& defines) -> void {
    if (params.empty()) {
        return;
    }
    auto ordered = std::vector<Param>{};
    for (const auto& def : defines) {
        if (const auto res = std::find_if(params.begin(), params.end(), [&](const auto& p) { return p.getName() == def.getName(); });
            res != params.end()) {
            if (!CheckValidParamTypes(*res, def)) {
                common::AbortWithDetail("Param {} has a non-compatible type (define type: {})", res->getName(), common::ToString(def.getParamType()));
            }
            ordered.emplace_back(std::move(*res));
        }
    }
    params.swap(ordered);
}

static auto ValidateAssetCallTable(AssetCallTable& act, const ParamDefineTable& pdt, std::unordered_set<std::uint32_t>& seen, std::unordered_set<std::uint32_t>& active) -> void {
    if (active.contains(act.getGUID())) {
        common::AbortWithDetail("Circular reference detected! {}[{:#010x}]", act.getKeyName(), act.getGUID());
    }

    if (seen.contains(act.getGUID())) {
        return;
    }

    active.insert(act.getGUID());
    seen.insert(act.getGUID());

    if (act.isContainer()) {
        for (auto& child : act.getChildren()) {
            ValidateAssetCallTable(*child, pdt, seen, active);
        }
    } else {
        ValidateAndReorderAssetParams(static_cast<Asset&>(act).getParams(), pdt.getSystemAssetParams(), pdt.getCustomAssetParams());
    }

    active.erase(act.getGUID());
}

auto Database::parse(std::span<const char> text, ParseDataTag) -> void {
    auto lexer = Lexer(text);
    auto foundMetadata = false;
    auto foundParamDefines = false;
    auto foundUsers = false;
    while (true) {
        if (foundMetadata && foundParamDefines && foundUsers) {
            break;
        }

        const auto token = lexer.next();
        if (token.type == Token::EndOfFile) {
            break;
        }

        if (token.type != Token::Identifier) {
            SyntaxError(lexer, token, "Expected an identifier!");
        }

        if (token.value == "Metadata") {
            ParseMetadata(*this, lexer);
            foundMetadata = true;
        } else if (token.value == "ParamDefines") {
            ParseParamDefines(*this->mParamDefineTable, lexer);
            foundParamDefines = true;
        } else if (token.value == "Users") {
            ParseUsers(*this, lexer);
            foundUsers = true;
        } else {
            SyntaxError(lexer, token, "Expected one of [Metadata, ParamDefines, Users]!");
        }
    }

    // validate (there might be more I'm forgetting)
    // - no circular references
    // - reorder params (and validate they have correct values)
    for (auto& user : mUsers) {
        ValidateAndReorderUserParams(user->getParams(), mParamDefineTable->getSystemUserParams(), mParamDefineTable->getCustomUserParams());
        for (auto& slot : user->getActionSlots()) {
            for (auto& action : slot.getActions()) {
                for (auto& trigger : action.getTriggers()) {
                    ValidateAndReorderTriggerParams(trigger.getOverwriteParams(), mParamDefineTable->getTriggerParams());
                }
            }
        }
        for (auto& prop : user->getProperties()) {
            for (auto& trigger : prop.getTriggers()) {
                ValidateAndReorderTriggerParams(trigger.getOverwriteParams(), mParamDefineTable->getTriggerParams());
            }
        }
        for (auto& trigger : user->getAlwaysTriggers()) {
            ValidateAndReorderTriggerParams(trigger.getOverwriteParams(), mParamDefineTable->getTriggerParams());
        }
        auto seen = std::unordered_set<std::uint32_t>{};
        for (auto& act : user->getAssetCallTables()) {
            auto active = std::unordered_set<std::uint32_t>{};
            ValidateAssetCallTable(*act, *mParamDefineTable, seen, active);
        }
    }
}

} // namespace mango