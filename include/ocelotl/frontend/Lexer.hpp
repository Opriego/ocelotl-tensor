#pragma once

#include "ocelotl/frontend/Token.hpp"

#include <string_view>

namespace ocelotl::frontend {

class Lexer {
public:
    explicit Lexer(std::string_view source);

    Token nextToken();

private:
    [[nodiscard]] bool isAtEnd() const noexcept;

    char peek() const noexcept;
    char advance() noexcept;

    void skipWhitespace() noexcept;

    Token makeToken(TokenKind kind, std::size_t startOffset,
                    SourceLocation startLocation);

    Token lexIdentifierOrKeyword();
    Token lexNumber();

    std::string_view source_;
    std::size_t current_{0};

    SourceLocation location_;
};

} // namespace ocelotl::frontend
