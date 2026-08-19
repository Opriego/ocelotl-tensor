#pragma once

#include "ocelotl/ast/AST.hpp"
#include "ocelotl/frontend/Lexer.hpp"
#include "ocelotl/frontend/Token.hpp"

#include <string_view>

namespace ocelotl::frontend {

class Parser {
public:
    explicit Parser(std::string_view source);

    [[nodiscard]]
    ast::Program parseProgram();

private:
    void advance();

    [[nodiscard]]
    bool check(TokenKind kind) const noexcept;

    Token consume(TokenKind kind, std::string_view expectation);

    [[nodiscard]]
    ast::Statement parseStatement();

    [[nodiscard]]
    ast::TensorDecl parseTensorDeclaration();

    [[nodiscard]]
    ast::Assignment parseAssignment();

    [[nodiscard]]
    ast::ReturnStmt parseReturnStatement();

    [[nodiscard]]
    std::shared_ptr<ast::IfStmt> parseIfStatement();

    [[nodiscard]]
    std::vector<ast::Statement> parseBlock();

    [[nodiscard]]
    ast::TensorType parseTensorType();

    [[nodiscard]]
    ast::Expression parseExpression();

    [[nodiscard]] ast::Expression parseComparison();
    [[nodiscard]] ast::Expression parseAdditive();
    [[nodiscard]] ast::Expression parseMultiplicative();
    [[nodiscard]] ast::Expression parsePrimary();

    [[nodiscard]] ast::Expression makeBinary(
        ast::Expression lhs,
        const Token& operatorToken,
        ast::Expression rhs
    );

    [[nodiscard]]
    ast::Expression parseIdentifierOrCall();

    [[noreturn]]
    void throwParseError(
        const Token& token,
        std::string_view message
    ) const;

    Lexer lexer_;
    Token current_;
};

} // namespace ocelotl::frontend
