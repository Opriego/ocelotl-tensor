// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/frontend/Token.hpp"

namespace ocelotl::frontend {

std::string_view toString(TokenKind kind) noexcept
{
    switch (kind) {
    case TokenKind::EndOfFile:
        return "eof";

    case TokenKind::Identifier:
        return "identifier";

    case TokenKind::IntegerLiteral:
        return "integer_literal";

    case TokenKind::FloatLiteral:
        return "float_literal";

    case TokenKind::KwTensor:
        return "tensor";

    case TokenKind::KwReturn:
        return "return";

    case TokenKind::KwIf:
        return "if";

    case TokenKind::KwElse:
        return "else";

    case TokenKind::Colon:
        return ":";

    case TokenKind::Comma:
        return ",";

    case TokenKind::Equal:
        return "=";

    case TokenKind::EqualEqual:
        return "==";

    case TokenKind::BangEqual:
        return "!=";

    case TokenKind::Plus:
        return "+";

    case TokenKind::Minus:
        return "-";

    case TokenKind::Star:
        return "*";

    case TokenKind::Slash:
        return "/";

    case TokenKind::Less:
        return "<";

    case TokenKind::LessEqual:
        return "<=";

    case TokenKind::Greater:
        return ">";

    case TokenKind::GreaterEqual:
        return ">=";

    case TokenKind::LeftBracket:
        return "[";

    case TokenKind::RightBracket:
        return "]";

    case TokenKind::LeftParen:
        return "(";

    case TokenKind::RightParen:
        return ")";

    case TokenKind::LeftBrace:
        return "{";

    case TokenKind::RightBrace:
        return "}";

    case TokenKind::Unknown:
        return "unknown";
    }

    return "unknown";
}

} // namespace ocelotl::frontend
