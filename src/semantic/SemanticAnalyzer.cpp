// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>
#include <variant>

namespace ocelotl::sema {

namespace {

[[nodiscard]]
std::string makeLocationPrefix(
    const ast::SourceLocation& location
)
{
    std::ostringstream stream;

    stream
        << location.line
        << ':'
        << location.column
        << ": ";

    return stream.str();
}

} // namespace

void SemanticAnalyzer::analyze(
    const ast::Program& program
)
{
    symbols_ = {};
    returnType_.reset();

    for (const auto& statement : program.statements) {
        analyzeStatement(statement);
    }
}

const SymbolTable&
SemanticAnalyzer::symbols() const noexcept
{
    return symbols_;
}

void SemanticAnalyzer::analyzeStatement(
    const ast::Statement& statement
)
{
    std::visit(
        [this](const auto& node) {
            using Node =
                std::decay_t<decltype(node)>;

            if constexpr (
                std::is_same_v<Node, ast::TensorDecl>
            ) {
                analyzeTensorDeclaration(node);
            }
            else if constexpr (
                std::is_same_v<Node, ast::Assignment>
            ) {
                analyzeAssignment(node);
            }
            else if constexpr (
                std::is_same_v<Node, ast::ReturnStmt>
            ) {
                analyzeReturn(node);
            }
            else if constexpr (
                std::is_same_v<Node, std::shared_ptr<ast::IfStmt>>
            ) {
                if (!node) {
                    throw SemanticError{"encountered null if statement"};
                }
                analyzeIf(*node);
            }
        },
        statement
    );
}

void SemanticAnalyzer::analyzeTensorDeclaration(
    const ast::TensorDecl& declaration
)
{
    Symbol symbol{
        .name = declaration.name,
        .type = TensorType{
            .elementType = declaration.type.elementType,
            .shape = declaration.type.shape
        },
        .location = declaration.location
    };

    if (!symbols_.declare(std::move(symbol))) {
        throw SemanticError{
            makeLocationPrefix(declaration.location)
            + "redeclaration of symbol '"
            + declaration.name
            + "'"
        };
    }
}

void SemanticAnalyzer::analyzeAssignment(
    const ast::Assignment& assignment
)
{
    /*
     * Analyze the RHS first.
     *
     * This guarantees that:
     *
     *     C = matmul(A, X)
     *
     * does not introduce C if X is semantically invalid.
     */
    const TensorType inferredType =
        analyzeExpression(assignment.value);

    /*
     * For the current language model, assignment can introduce a
     * computed symbol:
     *
     *     C = matmul(A, B)
     *
     * If C already exists, we currently require the inferred type
     * to match its declared type.
     */
    if (const Symbol* existing =
            symbols_.lookup(assignment.target);
        existing != nullptr) {

        if (existing->type.elementType !=
            inferredType.elementType) {

            throw SemanticError{
                "assignment type mismatch for symbol '"
                + assignment.target
                + "'"
            };
        }

        if (existing->type.shape !=
            inferredType.shape) {

            throw SemanticError{
                "assignment shape mismatch for symbol '"
                + assignment.target
                + "'"
            };
        }

        return;
    }

    Symbol symbol{
        .name = assignment.target,
        .type = inferredType,
        .location = {}
    };

    const bool inserted =
        symbols_.declare(std::move(symbol));

    if (!inserted) {
        throw SemanticError{
            "failed to introduce symbol '"
            + assignment.target
            + "'"
        };
    }
}

void SemanticAnalyzer::analyzeReturn(
    const ast::ReturnStmt& returnStatement
)
{
    const TensorType type = analyzeExpression(returnStatement.value);
    if (returnType_ &&
        (returnType_->elementType != type.elementType ||
         returnType_->shape != type.shape)) {
        throw SemanticError{"return type mismatch"};
    }
    if (type.elementType != "i64" || !type.shape.empty()) {
        throw SemanticError{
            makeLocationPrefix(returnStatement.location) +
            "top-level return must have scalar i64 type"
        };
    }
    returnType_ = type;
}

void SemanticAnalyzer::analyzeIf(const ast::IfStmt& ifStatement)
{
    const TensorType conditionType = analyzeExpression(ifStatement.condition);
    if (conditionType.elementType != "i1" || !conditionType.shape.empty()) {
        throw SemanticError{
            makeLocationPrefix(ifStatement.location) +
            "if condition must have type i1"
        };
    }

    const SymbolTable before = symbols_;

    for (const auto& statement : ifStatement.thenStatements) {
        analyzeStatement(statement);
    }
    const SymbolTable thenSymbols = symbols_;

    symbols_ = before;
    for (const auto& statement : ifStatement.elseStatements) {
        analyzeStatement(statement);
    }
    const SymbolTable elseSymbols = symbols_;

    symbols_ = before;
    for (const auto& [name, thenSymbol] : thenSymbols.entries()) {
        if (before.contains(name)) {
            continue;
        }

        const Symbol* elseSymbol = elseSymbols.lookup(name);
        if (elseSymbol == nullptr) {
            continue;
        }
        if (thenSymbol.type.elementType != elseSymbol->type.elementType ||
            thenSymbol.type.shape != elseSymbol->type.shape) {
            throw SemanticError{
                "branch type mismatch for symbol '" + name + "'"
            };
        }
        static_cast<void>(symbols_.declare(thenSymbol));
    }
}

TensorType SemanticAnalyzer::analyzeExpression(
    const ast::Expression& expression
)
{
    return std::visit(
        [this](const auto& node) -> TensorType {
            using Node =
                std::decay_t<decltype(node)>;

            if constexpr (
                std::is_same_v<
                    Node,
                    ast::IdentifierExpr
                >
            ) {
                return analyzeIdentifier(node);
            }
            else if constexpr (
                std::is_same_v<
                    Node,
                    ast::IntegerExpr
                >
            ) {
                /*
                 * Integer literals are represented as scalar values.
                 *
                 * An empty shape denotes rank 0.
                 */
                return TensorType{
                    .elementType = "i64",
                    .shape = {}
                };
            }
            else if constexpr (
                std::is_same_v<
                    Node,
                    ast::FloatExpr
                >
            ) {
                /*
                 * Floating-point literals are represented as scalar
                 * values. For now we use f64 as the default literal
                 * type.
                 */
                return TensorType{
                    .elementType = "f64",
                    .shape = {}
                };
            }
            else if constexpr (
                std::is_same_v<
                    Node,
                    std::shared_ptr<ast::CallExpr>
                >
            ) {
                if (!node) {
                    throw SemanticError{
                        "encountered null call expression"
                    };
                }

                return analyzeCall(*node);
            }
            else if constexpr (
                std::is_same_v<Node, std::shared_ptr<ast::BinaryExpr>>
            ) {
                if (!node) {
                    throw SemanticError{"encountered null binary expression"};
                }
                return analyzeBinary(*node);
            }
        },
        expression
    );
}

TensorType SemanticAnalyzer::analyzeBinary(const ast::BinaryExpr& binary)
{
    const TensorType lhs = analyzeExpression(binary.lhs);
    const TensorType rhs = analyzeExpression(binary.rhs);

    if (!lhs.shape.empty() || !rhs.shape.empty()) {
        throw SemanticError{"scalar binary operator requires scalar operands"};
    }
    if (lhs.elementType != rhs.elementType) {
        throw SemanticError{"binary operator requires matching operand types"};
    }
    if (lhs.elementType != "i64" && lhs.elementType != "f64") {
        throw SemanticError{"binary operator requires numeric operands"};
    }

    switch (binary.op) {
    case ast::BinaryOperator::Add:
    case ast::BinaryOperator::Subtract:
    case ast::BinaryOperator::Multiply:
    case ast::BinaryOperator::Divide:
        return lhs;

    case ast::BinaryOperator::Equal:
    case ast::BinaryOperator::NotEqual:
    case ast::BinaryOperator::Less:
    case ast::BinaryOperator::LessEqual:
    case ast::BinaryOperator::Greater:
    case ast::BinaryOperator::GreaterEqual:
        return TensorType{.elementType = "i1", .shape = {}};
    }

    throw SemanticError{"unknown binary operator"};
}

TensorType SemanticAnalyzer::analyzeIdentifier(
    const ast::IdentifierExpr& identifier
)
{
    const Symbol* symbol =
        symbols_.lookup(identifier.name);

    if (symbol == nullptr) {
        throw SemanticError{
            "use of undeclared identifier '"
            + identifier.name
            + "'"
        };
    }

    return symbol->type;
}

TensorType SemanticAnalyzer::analyzeCall(
    const ast::CallExpr& call
)
{
    if (call.callee == "relu") {
        if (call.arguments.size() != 1) {
            throw SemanticError{
                "relu expects exactly 1 argument"
            };
        }

        /*
         * ReLU preserves both element type and shape.
         */
        return analyzeExpression(
            call.arguments[0]
        );
    }

    if (call.callee == "matmul") {
        if (call.arguments.size() != 2) {
            throw SemanticError{
                "matmul expects exactly 2 arguments"
            };
        }

        const TensorType lhs =
            analyzeExpression(
                call.arguments[0]
            );

        const TensorType rhs =
            analyzeExpression(
                call.arguments[1]
            );

        /*
         * Current matmul implementation supports only matrices.
         *
         * Later we can extend this to batched matrix
         * multiplication.
         */
        if (lhs.shape.size() != 2 ||
            rhs.shape.size() != 2) {

            throw SemanticError{
                "matmul requires rank-2 tensors"
            };
        }

        if (lhs.elementType != rhs.elementType) {
            throw SemanticError{
                "matmul requires matching element types"
            };
        }

        /*
         * Matrix multiplication:
         *
         * lhs = [M, K]
         * rhs = [K, N]
         *
         * result = [M, N]
         */
        if (lhs.shape[1] != rhs.shape[0]) {
            std::ostringstream message;

            message
                << "incompatible tensor dimensions for matmul: "
                << "lhs is ["
                << lhs.shape[0]
                << ','
                << lhs.shape[1]
                << "], rhs is ["
                << rhs.shape[0]
                << ','
                << rhs.shape[1]
                << ']';

            throw SemanticError{
                message.str()
            };
        }

        return TensorType{
            .elementType = lhs.elementType,
            .shape = {
                lhs.shape[0],
                rhs.shape[1]
            }
        };
    }

    throw SemanticError{
        "unknown operation '"
        + call.callee
        + "'"
    };
}

} // namespace ocelotl::sema
