// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

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

    case TokenKind::KwIf:
        return parseIfStatement();

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

std::shared_ptr<ast::IfStmt> Parser::parseIfStatement()
{
    const Token ifToken = consume(TokenKind::KwIf, "'if'");
    ast::Expression condition = parseExpression();
    std::vector<ast::Statement> thenStatements = parseBlock();

    consume(TokenKind::KwElse, "'else'");
    std::vector<ast::Statement> elseStatements = parseBlock();

    auto statement = std::make_shared<ast::IfStmt>();
    statement->condition = std::move(condition);
    statement->thenStatements = std::move(thenStatements);
    statement->elseStatements = std::move(elseStatements);
    statement->location = ifToken.location;
    return statement;
}

std::vector<ast::Statement> Parser::parseBlock()
{
    consume(TokenKind::LeftBrace, "'{'");
    std::vector<ast::Statement> statements;

    while (!check(TokenKind::RightBrace)) {
        if (check(TokenKind::EndOfFile)) {
            throwParseError(current_, "expected '}'");
        }
        statements.emplace_back(parseStatement());
    }

    consume(TokenKind::RightBrace, "'}'");
    return statements;
}

ast::Expression Parser::parseExpression()
{
    return parseComparison();
}

ast::Expression Parser::parseComparison()
{
    ast::Expression expression = parseAdditive();

    while (check(TokenKind::EqualEqual) || check(TokenKind::BangEqual) ||
           check(TokenKind::Less) || check(TokenKind::LessEqual) ||
           check(TokenKind::Greater) || check(TokenKind::GreaterEqual)) {
        const Token operatorToken = current_;
        advance();
        expression = makeBinary(
            std::move(expression),
            operatorToken,
            parseAdditive()
        );
    }

    return expression;
}

ast::Expression Parser::parseAdditive()
{
    ast::Expression expression = parseMultiplicative();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        const Token operatorToken = current_;
        advance();
        expression = makeBinary(
            std::move(expression),
            operatorToken,
            parseMultiplicative()
        );
    }
    return expression;
}

ast::Expression Parser::parseMultiplicative()
{
    ast::Expression expression = parsePrimary();
    while (check(TokenKind::Star) || check(TokenKind::Slash)) {
        const Token operatorToken = current_;
        advance();
        expression = makeBinary(
            std::move(expression),
            operatorToken,
            parsePrimary()
        );
    }
    return expression;
}

ast::Expression Parser::parsePrimary()
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

    case TokenKind::LeftParen: {
        advance();
        ast::Expression expression = parseExpression();
        consume(TokenKind::RightParen, "')'");
        return expression;
    }

    default:
        throwParseError(
            current_,
            "expected expression"
        );
    }
}

ast::Expression Parser::makeBinary(
    ast::Expression lhs,
    const Token& operatorToken,
    ast::Expression rhs
)
{
    ast::BinaryOperator operation;
    switch (operatorToken.kind) {
    case TokenKind::Plus: operation = ast::BinaryOperator::Add; break;
    case TokenKind::Minus: operation = ast::BinaryOperator::Subtract; break;
    case TokenKind::Star: operation = ast::BinaryOperator::Multiply; break;
    case TokenKind::Slash: operation = ast::BinaryOperator::Divide; break;
    case TokenKind::EqualEqual: operation = ast::BinaryOperator::Equal; break;
    case TokenKind::BangEqual: operation = ast::BinaryOperator::NotEqual; break;
    case TokenKind::Less: operation = ast::BinaryOperator::Less; break;
    case TokenKind::LessEqual: operation = ast::BinaryOperator::LessEqual; break;
    case TokenKind::Greater: operation = ast::BinaryOperator::Greater; break;
    case TokenKind::GreaterEqual: operation = ast::BinaryOperator::GreaterEqual; break;
    default:
        throwParseError(operatorToken, "expected binary operator");
    }

    auto expression = std::make_shared<ast::BinaryExpr>();
    expression->op = operation;
    expression->lhs = std::move(lhs);
    expression->rhs = std::move(rhs);
    expression->location = operatorToken.location;
    return expression;
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
