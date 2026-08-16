#include "ocelotl/ir/IRGenerator.hpp"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace ocelotl::ir {

IRGenerator::IRGenerator(
    const sema::SemanticAnalyzer& semanticAnalyzer
)
    : semanticAnalyzer_{semanticAnalyzer}
{
}

Module IRGenerator::generate(
    const ast::Program& program
)
{
    /*
     * Allow one IRGenerator instance to be reused safely.
     */
    module_.operations.clear();
    values_.clear();
    nextValueId_ = 0;

    for (const auto& statement : program.statements) {
        generateStatement(statement);
    }

    return module_;
}

void IRGenerator::generateStatement(
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
                generateTensorDeclaration(node);
            }
            else if constexpr (
                std::is_same_v<Node, ast::Assignment>
            ) {
                generateAssignment(node);
            }
            else if constexpr (
                std::is_same_v<Node, ast::ReturnStmt>
            ) {
                generateReturn(node);
            }
        },
        statement
    );
}

void IRGenerator::generateTensorDeclaration(
    const ast::TensorDecl& declaration
)
{
    const ValueId value =
        createValue();

    const sema::TensorType& type =
        lookupType(declaration.name);

    module_.operations.emplace_back(
        TensorDeclOp{
            .result = value,
            .name = declaration.name,
            .type = type
        }
    );

    const auto [iterator, inserted] =
        values_.emplace(
            declaration.name,
            value
        );

    if (!inserted) {
        throw std::runtime_error{
            "IR generation encountered duplicate value '"
            + declaration.name
            + "'"
        };
    }

    static_cast<void>(iterator);
}

void IRGenerator::generateAssignment(
    const ast::Assignment& assignment
)
{
    /*
     * Generate the RHS first.
     *
     * For:
     *
     *     C = matmul(A, B)
     *
     * generateExpression() creates the MatMul operation and returns
     * the resulting SSA-like ValueId.
     */
    const ValueId result =
        generateExpression(assignment.value);

    /*
     * Bind the source-level name to the generated IR value.
     *
     * Assignment currently behaves like an immutable SSA-style
     * binding in the IR layer.
     */
    values_[assignment.target] = result;
}

void IRGenerator::generateReturn(
    const ast::ReturnStmt& returnStatement
)
{
    const ValueId value =
        generateExpression(
            returnStatement.value
        );

    module_.operations.emplace_back(
        ReturnOp{
            .value = value
        }
    );
}

ValueId IRGenerator::generateExpression(
    const ast::Expression& expression
)
{
    return std::visit(
        [this](const auto& node) -> ValueId {
            using Node =
                std::decay_t<decltype(node)>;

            if constexpr (
                std::is_same_v<
                    Node,
                    ast::IdentifierExpr
                >
            ) {
                return generateIdentifier(node);
            }
            else if constexpr (
                std::is_same_v<
                    Node,
                    ast::IntegerExpr
                >
            ) {
                return generateInteger(node);
            }
            else if constexpr (
                std::is_same_v<
                    Node,
                    ast::FloatExpr
                >
            ) {
                return generateFloat(node);
            }
            else if constexpr (
                std::is_same_v<
                    Node,
                    std::shared_ptr<ast::CallExpr>
                >
            ) {
                if (!node) {
                    throw std::runtime_error{
                        "IR generation encountered null call expression"
                    };
                }

                return generateCall(*node);
            }
        },
        expression
    );
}

ValueId IRGenerator::generateIdentifier(
    const ast::IdentifierExpr& identifier
) const
{
    const auto iterator =
        values_.find(identifier.name);

    if (iterator == values_.end()) {
        throw std::runtime_error{
            "IR generation could not resolve identifier '"
            + identifier.name
            + "'"
        };
    }

    return iterator->second;
}

ValueId IRGenerator::generateInteger(
    const ast::IntegerExpr& integer
)
{
    const ValueId result =
        createValue();

    module_.operations.emplace_back(
        ConstantIntOp{
            .result = result,
            .value = integer.value,
            .type = sema::TensorType{
                .elementType = "i64",
                .shape = {}
            }
        }
    );

    return result;
}

ValueId IRGenerator::generateFloat(
    const ast::FloatExpr& floatingPoint
)
{
    const ValueId result =
        createValue();

    module_.operations.emplace_back(
        ConstantFloatOp{
            .result = result,
            .value = floatingPoint.value,
            .type = sema::TensorType{
                .elementType = "f64",
                .shape = {}
            }
        }
    );

    return result;
}

ValueId IRGenerator::generateCall(
    const ast::CallExpr& call
)
{
    if (call.callee == "relu") {
        if (call.arguments.size() != 1) {
            throw std::runtime_error{
                "IR generation expected relu to have 1 argument"
            };
        }

        const ValueId input =
            generateExpression(
                call.arguments[0]
            );

        const ValueId result =
            createValue();

        /*
         * Semantic analysis already validated and inferred the
         * resulting type. ReLU preserves its input type.
         *
         * Since arguments ultimately refer to already analyzed
         * values, retrieve the type using the resulting assignment
         * symbol when possible is unnecessary here. We derive it
         * from the operand below using the source expression.
         */
        sema::TensorType type;

        const auto& argument =
            call.arguments[0];

        std::visit(
            [this, &type](
                const auto& argumentNode
            ) {
                using Node =
                    std::decay_t<
                        decltype(argumentNode)
                    >;

                if constexpr (
                    std::is_same_v<
                        Node,
                        ast::IdentifierExpr
                    >
                ) {
                    type =
                        lookupType(
                            argumentNode.name
                        );
                }
                else if constexpr (
                    std::is_same_v<
                        Node,
                        ast::IntegerExpr
                    >
                ) {
                    type = sema::TensorType{
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
                    type = sema::TensorType{
                        .elementType = "f64",
                        .shape = {}
                    };
                }
                else {
                    throw std::runtime_error{
                        "nested relu calls are not yet supported "
                        "during IR type reconstruction"
                    };
                }
            },
            argument
        );

        module_.operations.emplace_back(
            ReluOp{
                .result = result,
                .input = input,
                .type = std::move(type)
            }
        );

        return result;
    }

    if (call.callee == "matmul") {
        if (call.arguments.size() != 2) {
            throw std::runtime_error{
                "IR generation expected matmul to have 2 arguments"
            };
        }

        const ValueId lhs =
            generateExpression(
                call.arguments[0]
            );

        const ValueId rhs =
            generateExpression(
                call.arguments[1]
            );

        /*
         * At this stage matmul operands should normally be
         * identifiers. Semantic analysis has already validated
         * their rank and compatible dimensions.
         */
        const auto* lhsIdentifier =
            std::get_if<ast::IdentifierExpr>(
                &call.arguments[0]
            );

        const auto* rhsIdentifier =
            std::get_if<ast::IdentifierExpr>(
                &call.arguments[1]
            );

        if (lhsIdentifier == nullptr ||
            rhsIdentifier == nullptr) {

            throw std::runtime_error{
                "matmul currently requires identifier operands "
                "during IR generation"
            };
        }

        const sema::TensorType& lhsType =
            lookupType(lhsIdentifier->name);

        const sema::TensorType& rhsType =
            lookupType(rhsIdentifier->name);

        if (lhsType.shape.size() != 2 ||
            rhsType.shape.size() != 2) {

            throw std::runtime_error{
                "invalid matmul operands reached IR generation"
            };
        }

        sema::TensorType resultType{
            .elementType =
                lhsType.elementType,

            .shape = {
                lhsType.shape[0],
                rhsType.shape[1]
            }
        };

        const ValueId result =
            createValue();

        module_.operations.emplace_back(
            MatMulOp{
                .result = result,
                .lhs = lhs,
                .rhs = rhs,
                .type = std::move(resultType)
            }
        );

        return result;
    }

    throw std::runtime_error{
        "IR generation encountered unknown operation '"
        + call.callee
        + "'"
    };
}

ValueId IRGenerator::createValue()
{
    return nextValueId_++;
}

const sema::TensorType&
IRGenerator::lookupType(
    const std::string_view name
) const
{
    const sema::Symbol* symbol =
        semanticAnalyzer_
            .symbols()
            .lookup(name);

    if (symbol == nullptr) {
        throw std::runtime_error{
            "IR generation could not find semantic type for '"
            + std::string{name}
            + "'"
        };
    }

    return symbol->type;
}

} // namespace ocelotl::ir
