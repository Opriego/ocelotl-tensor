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
    KwIf,
    KwElse,

    Colon,
    Comma,
    Equal,
    EqualEqual,
    BangEqual,

    Plus,
    Minus,
    Star,
    Slash,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    LeftBracket,
    RightBracket,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,

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
