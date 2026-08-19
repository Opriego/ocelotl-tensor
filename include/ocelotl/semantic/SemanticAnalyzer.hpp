#pragma once

#include "ocelotl/ast/AST.hpp"
#include "ocelotl/semantic/SymbolTable.hpp"

#include <stdexcept>
#include <string>
#include <optional>

namespace ocelotl::sema {

class SemanticError : public std::runtime_error {
public:
    explicit SemanticError(
        const std::string& message
    )
        : std::runtime_error{message}
    {
    }
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer() = default;

    void analyze(const ast::Program& program);

    [[nodiscard]]
    const SymbolTable& symbols() const noexcept;

private:
    void analyzeStatement(
        const ast::Statement& statement
    );

    void analyzeTensorDeclaration(
        const ast::TensorDecl& declaration
    );

    void analyzeAssignment(
        const ast::Assignment& assignment
    );

    void analyzeReturn(
        const ast::ReturnStmt& returnStatement
    );

    void analyzeIf(const ast::IfStmt& ifStatement);

    [[nodiscard]]
    TensorType analyzeExpression(
        const ast::Expression& expression
    );

    [[nodiscard]]
    TensorType analyzeIdentifier(
        const ast::IdentifierExpr& identifier
    );

    [[nodiscard]]
    TensorType analyzeCall(
        const ast::CallExpr& call
    );

    [[nodiscard]]
    TensorType analyzeBinary(const ast::BinaryExpr& binary);

    SymbolTable symbols_;
    std::optional<TensorType> returnType_;
};

} // namespace ocelotl::sema
