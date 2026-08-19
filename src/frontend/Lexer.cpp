// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/frontend/Lexer.hpp"

#include <cctype>

namespace ocelotl::frontend {

Lexer::Lexer(std::string_view source)
    : source_(source)
{
}

bool Lexer::isAtEnd() const noexcept
{
    return current_ >= source_.size();
}

char Lexer::peek() const noexcept
{
    if (isAtEnd()) {
        return '\0';
    }

    return source_[current_];
}

char Lexer::peekNext() const noexcept
{
    if (current_ + 1 >= source_.size()) {
        return '\0';
    }

    return source_[current_ + 1];
}

char Lexer::advance() noexcept
{
    if (isAtEnd()) {
        return '\0';
    }

    const char current = source_[current_++];

    ++location_.offset;

    if (current == '\n') {
        ++location_.line;
        location_.column = 1;
    } else {
        ++location_.column;
    }

    return current;
}

void Lexer::skipWhitespace() noexcept
{
    while (!isAtEnd()) {
        switch (peek()) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            advance();
            break;

        default:
            return;
        }
    }
}

Token Lexer::makeToken(
    TokenKind kind,
    std::size_t startOffset,
    SourceLocation startLocation
) const
{
    return Token{
        kind,
        std::string{
            source_.substr(
                startOffset,
                current_ - startOffset
            )
        },
        startLocation
    };
}

Token Lexer::lexIdentifierOrKeyword()
{
    const auto startOffset = current_;
    const auto startLocation = location_;

    while (
        std::isalnum(
            static_cast<unsigned char>(peek())
        ) ||
        peek() == '_'
    ) {
        advance();
    }

    const auto lexeme =
        source_.substr(
            startOffset,
            current_ - startOffset
        );

    TokenKind kind = TokenKind::Identifier;

    if (lexeme == "tensor") {
        kind = TokenKind::KwTensor;
    } else if (lexeme == "return") {
        kind = TokenKind::KwReturn;
    } else if (lexeme == "if") {
        kind = TokenKind::KwIf;
    } else if (lexeme == "else") {
        kind = TokenKind::KwElse;
    }

    return makeToken(
        kind,
        startOffset,
        startLocation
    );
}

Token Lexer::lexNumber()
{
    const auto startOffset = current_;
    const auto startLocation = location_;

    while (
        std::isdigit(
            static_cast<unsigned char>(peek())
        )
    ) {
        advance();
    }

    TokenKind kind = TokenKind::IntegerLiteral;

    if (
        peek() == '.' &&
        std::isdigit(
            static_cast<unsigned char>(peekNext())
        )
    ) {
        kind = TokenKind::FloatLiteral;

        advance();

        while (
            std::isdigit(
                static_cast<unsigned char>(peek())
            )
        ) {
            advance();
        }
    }

    return makeToken(
        kind,
        startOffset,
        startLocation
    );
}

Token Lexer::nextToken()
{
    skipWhitespace();

    if (isAtEnd()) {
        return Token{
            TokenKind::EndOfFile,
            "",
            location_
        };
    }

    const auto startOffset = current_;
    const auto startLocation = location_;

    const char current = peek();

    if (
        std::isalpha(
            static_cast<unsigned char>(current)
        ) ||
        current == '_'
    ) {
        return lexIdentifierOrKeyword();
    }

    if (
        std::isdigit(
            static_cast<unsigned char>(current)
        )
    ) {
        return lexNumber();
    }

    advance();

    switch (current) {
    case ':':
        return makeToken(
            TokenKind::Colon,
            startOffset,
            startLocation
        );

    case ',':
        return makeToken(
            TokenKind::Comma,
            startOffset,
            startLocation
        );

    case '=':
        if (peek() == '=') {
            advance();
            return makeToken(TokenKind::EqualEqual, startOffset, startLocation);
        }
        return makeToken(
            TokenKind::Equal,
            startOffset,
            startLocation
        );

    case '!':
        if (peek() == '=') {
            advance();
            return makeToken(TokenKind::BangEqual, startOffset, startLocation);
        }
        return makeToken(TokenKind::Unknown, startOffset, startLocation);

    case '+':
        return makeToken(TokenKind::Plus, startOffset, startLocation);

    case '-':
        return makeToken(TokenKind::Minus, startOffset, startLocation);

    case '*':
        return makeToken(TokenKind::Star, startOffset, startLocation);

    case '/':
        return makeToken(TokenKind::Slash, startOffset, startLocation);

    case '<':
        if (peek() == '=') {
            advance();
            return makeToken(TokenKind::LessEqual, startOffset, startLocation);
        }
        return makeToken(TokenKind::Less, startOffset, startLocation);

    case '>':
        if (peek() == '=') {
            advance();
            return makeToken(TokenKind::GreaterEqual, startOffset, startLocation);
        }
        return makeToken(TokenKind::Greater, startOffset, startLocation);

    case '[':
        return makeToken(
            TokenKind::LeftBracket,
            startOffset,
            startLocation
        );

    case ']':
        return makeToken(
            TokenKind::RightBracket,
            startOffset,
            startLocation
        );

    case '(':
        return makeToken(
            TokenKind::LeftParen,
            startOffset,
            startLocation
        );

    case ')':
        return makeToken(
            TokenKind::RightParen,
            startOffset,
            startLocation
        );

    case '{':
        return makeToken(TokenKind::LeftBrace, startOffset, startLocation);

    case '}':
        return makeToken(TokenKind::RightBrace, startOffset, startLocation);

    default:
        return makeToken(
            TokenKind::Unknown,
            startOffset,
            startLocation
        );
    }
}

} // namespace ocelotl::frontend
