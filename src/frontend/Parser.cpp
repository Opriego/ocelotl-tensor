#include "ocelotl/frontend/Parser.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace ocelotl::frontend {

Parser::Parser(std::string_view source)
    : lexer_(source),
      current_(lexer_.nextToken())
{
}

void Parser::advance()
{
    current_ = lexer_.nextToken();
}

bool Parser::check(TokenKind kind) const noexcept
{
    return current_.kind == kind;
}

Token Parser::consume(
    TokenKind kind,
    std::string_view expectation
)
{
    if (!check(kind)) {
        std::ostringstream message;

        message
            << "expected "
            << expectation
            << ", found '"
            << current_.lexeme
            << "'";

        throwParseError(current_, message.str());
    }

    Token token = current_;
    advance();

    return token;
}

void Parser::throwParseError(
    const Token& token,
    std::string_view message
) const
{
    std::ostringstream error;

    error
        << token.location.line
        << ':'
        << token.location.column
        << ": parse error: "
        << message;

    throw std::runtime_error(error.str());
}

ast::Program Parser::parseProgram()
{
    ast::Program program;

    while (!check(TokenKind::EndOfFile)) {
        program.statements.emplace_back(
            parseStatement()
        );
    }

    return program;
}

ast::Statement Parser::parseStatement()
{
    switch (current_.kind) {
    case TokenKind::KwTensor:
        return parseTensorDeclaration();

    case TokenKind::KwReturn:
        return parseReturnStatement();

    case TokenKind::Identifier:
        return parseAssignment();

    case TokenKind::Unknown:
        throwParseError(
            current_,
            "unknown token '" + current_.lexeme + "'"
        );

    default:
        throwParseError(
            current_,
            "expected statement"
        );
    }
}

ast::TensorDecl Parser::parseTensorDeclaration()
{
    const Token tensorToken =
        consume(
            TokenKind::KwTensor,
            "'tensor'"
        );

    const Token name =
        consume(
            TokenKind::Identifier,
            "tensor name"
        );

    consume(
        TokenKind::Colon,
        "':'"
    );

    ast::TensorType type = parseTensorType();

    return ast::TensorDecl{
        name.lexeme,
        std::move(type),
        tensorToken.location
    };
}

ast::TensorType Parser::parseTensorType()
{
    const Token elementType =
        consume(
            TokenKind::Identifier,
            "element type"
        );

    consume(
        TokenKind::LeftBracket,
        "'['"
    );

    std::vector<std::size_t> dimensions;

    const Token firstDimension =
        consume(
            TokenKind::IntegerLiteral,
            "tensor dimension"
        );

    try {
        dimensions.push_back(
            static_cast<std::size_t>(
                std::stoull(firstDimension.lexeme)
            )
        );
    } catch (const std::exception&) {
        throwParseError(
            firstDimension,
            "invalid tensor dimension"
        );
    }

    while (check(TokenKind::Comma)) {
        advance();

        const Token dimension =
            consume(
                TokenKind::IntegerLiteral,
                "tensor dimension"
            );

        try {
            dimensions.push_back(
                static_cast<std::size_t>(
                    std::stoull(dimension.lexeme)
                )
            );
        } catch (const std::exception&) {
            throwParseError(
                dimension,
                "invalid tensor dimension"
            );
        }
    }

    consume(
        TokenKind::RightBracket,
        "']'"
    );

    return ast::TensorType{
        elementType.lexeme,
        std::move(dimensions)
    };
}

ast::Assignment Parser::parseAssignment()
{
    const Token target =
        consume(
            TokenKind::Identifier,
            "assignment target"
        );

    consume(
        TokenKind::Equal,
        "'='"
    );

    ast::Expression value = parseExpression();

    return ast::Assignment{
        target.lexeme,
        std::move(value),
        target.location
    };
}

ast::ReturnStmt Parser::parseReturnStatement()
{
    const Token returnToken =
        consume(
            TokenKind::KwReturn,
            "'return'"
        );

    ast::Expression value = parseExpression();

    return ast::ReturnStmt{
        std::move(value),
        returnToken.location
    };
}

ast::Expression Parser::parseExpression()
{
    switch (current_.kind) {
    case TokenKind::Identifier:
        return parseIdentifierOrCall();

    case TokenKind::IntegerLiteral: {
        const Token token = current_;
        advance();

        try {
            return ast::IntegerExpr{
                std::stoll(token.lexeme),
                token.location
            };
        } catch (const std::exception&) {
            throwParseError(
                token,
                "invalid integer literal"
            );
        }
    }

    case TokenKind::FloatLiteral: {
        const Token token = current_;
        advance();

        try {
            return ast::FloatExpr{
                std::stod(token.lexeme),
                token.location
            };
        } catch (const std::exception&) {
            throwParseError(
                token,
                "invalid floating-point literal"
            );
        }
    }

    default:
        throwParseError(
            current_,
            "expected expression"
        );
    }
}

ast::Expression Parser::parseIdentifierOrCall()
{
    const Token identifier =
        consume(
            TokenKind::Identifier,
            "identifier"
        );

    // A plain identifier expression:
    //
    //     return A
    //
    // versus a call:
    //
    //     relu(A)
    //
    if (!check(TokenKind::LeftParen)) {
        return ast::IdentifierExpr{
            identifier.lexeme,
            identifier.location
        };
    }

    advance(); // consume '('

    auto call = std::make_shared<ast::CallExpr>();

    call->callee = identifier.lexeme;
    call->location = identifier.location;

    if (!check(TokenKind::RightParen)) {
        call->arguments.emplace_back(
            parseExpression()
        );

        while (check(TokenKind::Comma)) {
            advance();

            call->arguments.emplace_back(
                parseExpression()
            );
        }
    }

    consume(
        TokenKind::RightParen,
        "')'"
    );

    return call;
}

} // namespace ocelotl::frontend
