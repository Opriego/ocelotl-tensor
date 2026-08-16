#pragma once

#include "ocelotl/ast/AST.hpp"
#include "ocelotl/ir/IR.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ocelotl::ir {

class IRGenerator {
public:
    explicit IRGenerator(
        const sema::SemanticAnalyzer& semanticAnalyzer
    );

    [[nodiscard]]
    Module generate(
        const ast::Program& program
    );

private:
    [[nodiscard]]
    ValueId generateExpression(
        const ast::Expression& expression
    );

    void generateStatement(
        const ast::Statement& statement
    );

    void generateTensorDeclaration(
        const ast::TensorDecl& declaration
    );

    void generateAssignment(
        const ast::Assignment& assignment
    );

    void generateReturn(
        const ast::ReturnStmt& returnStatement
    );

    [[nodiscard]]
    ValueId generateIdentifier(
        const ast::IdentifierExpr& identifier
    ) const;

    [[nodiscard]]
    ValueId generateCall(
        const ast::CallExpr& call
    );

    [[nodiscard]]
    ValueId generateInteger(
        const ast::IntegerExpr& integer
    );

    [[nodiscard]]
    ValueId generateFloat(
        const ast::FloatExpr& floatingPoint
    );

    [[nodiscard]]
    ValueId createValue();

    [[nodiscard]]
    const sema::TensorType& lookupType(
        std::string_view name
    ) const;

    const sema::SemanticAnalyzer& semanticAnalyzer_;

    Module module_;

    ValueId nextValueId_{0};

    std::unordered_map<std::string, ValueId> values_;
};

} // namespace ocelotl::ir
