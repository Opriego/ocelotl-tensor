#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ocelotl::frontend {

struct SourceLocation {
    std::size_t line{1};
    std::size_t column{1};
    std::size_t offset{0};
};

enum class TokenKind {
    EndOfFile,

    Identifier,

    IntegerLiteral,
    FloatLiteral,

    KwTensor,
    KwReturn,

    Colon,
    Comma,
    Equal,

    LeftBracket,
    RightBracket,
    LeftParen,
    RightParen,

    Unknown
};

struct Token {
    TokenKind kind{TokenKind::Unknown};
    std::string lexeme;
    SourceLocation location;
};

[[nodiscard]]
std::string_view toString(TokenKind kind) noexcept;

} // namespace ocelotl::frontend
