#pragma once

#include "ocelotl/ast/AST.hpp"
#include "ocelotl/ir/IR.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ocelotl::ir {

class IRGenerator {
public:
    explicit IRGenerator(const sema::SemanticAnalyzer& semanticAnalyzer);
    [[nodiscard]] Module generate(const ast::Program& program);

private:
    struct ValueInfo {
        ValueId value;
        sema::TensorType type;
    };
    using Environment = std::unordered_map<std::string, ValueInfo>;

    void generateStatement(const ast::Statement& statement);
    void generateTensorDeclaration(const ast::TensorDecl& declaration);
    void generateAssignment(const ast::Assignment& assignment);
    void generateReturn(const ast::ReturnStmt& returnStatement);
    void generateIf(const ast::IfStmt& ifStatement);

    [[nodiscard]] ValueInfo generateExpression(const ast::Expression& expression);
    [[nodiscard]] ValueInfo generateIdentifier(const ast::IdentifierExpr& identifier) const;
    [[nodiscard]] ValueInfo generateCall(const ast::CallExpr& call);
    [[nodiscard]] ValueInfo generateBinary(const ast::BinaryExpr& binary);
    [[nodiscard]] ValueInfo generateInteger(const ast::IntegerExpr& integer);
    [[nodiscard]] ValueInfo generateFloat(const ast::FloatExpr& floatingPoint);

    [[nodiscard]] ValueId createValue();
    [[nodiscard]] BlockId createBlock(std::string name);
    [[nodiscard]] BasicBlock& currentBlock();
    [[nodiscard]] const sema::TensorType& lookupType(std::string_view name) const;

    const sema::SemanticAnalyzer& semanticAnalyzer_;
    Module module_;
    ValueId nextValueId_{0};
    BlockId nextBlockId_{0};
    std::optional<BlockId> currentBlockId_;
    Environment values_;
};

} // namespace ocelotl::ir
