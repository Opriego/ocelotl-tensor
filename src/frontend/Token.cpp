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

    case TokenKind::Colon:
        return ":";

    case TokenKind::Comma:
        return ",";

    case TokenKind::Equal:
        return "=";

    case TokenKind::LeftBracket:
        return "[";

    case TokenKind::RightBracket:
        return "]";

    case TokenKind::LeftParen:
        return "(";

    case TokenKind::RightParen:
        return ")";

    case TokenKind::Unknown:
        return "unknown";
    }

    return "unknown";
}

} // namespace ocelotl::frontend
