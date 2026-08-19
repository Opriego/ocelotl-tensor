// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "ocelotl/frontend/Token.hpp"

#include <string_view>

namespace ocelotl::frontend {

class Lexer {
public:
    explicit Lexer(std::string_view source);

    [[nodiscard]]
    Token nextToken();

private:
    [[nodiscard]]
    bool isAtEnd() const noexcept;

    [[nodiscard]]
    char peek() const noexcept;

    [[nodiscard]]
    char peekNext() const noexcept;

    char advance() noexcept;

    void skipWhitespace() noexcept;

    [[nodiscard]]
    Token makeToken(
        TokenKind kind,
        std::size_t startOffset,
        SourceLocation startLocation
    ) const;

    [[nodiscard]]
    Token lexIdentifierOrKeyword();

    [[nodiscard]]
    Token lexNumber();

    std::string_view source_;
    std::size_t current_{0};

    SourceLocation location_;
};

} // namespace ocelotl::frontend
