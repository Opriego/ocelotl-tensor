#include "ocelotl/ir/IRGenerator.hpp"

#include "ocelotl/ir/IRVerifier.hpp"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace ocelotl::ir {
namespace {

bool sameType(const sema::TensorType& lhs, const sema::TensorType& rhs)
{
    return lhs.elementType == rhs.elementType && lhs.shape == rhs.shape;
}

} // namespace

IRGenerator::IRGenerator(const sema::SemanticAnalyzer& semanticAnalyzer)
    : semanticAnalyzer_{semanticAnalyzer} {}

Module IRGenerator::generate(const ast::Program& program)
{
    module_ = {};
    values_.clear();
    nextValueId_ = 0;
    nextBlockId_ = 0;

    module_.entry = createBlock("entry");
    currentBlockId_ = module_.entry;

    for (const auto& statement : program.statements) {
        if (!currentBlockId_) {
            throw std::runtime_error{"IR generation encountered unreachable statement"};
        }
        generateStatement(statement);
    }

    if (currentBlockId_ && !currentBlock().terminator) {
        throw std::runtime_error{"IR generation requires every path to return"};
    }

    IRVerifier{}.verify(module_);
    return module_;
}

void IRGenerator::generateStatement(const ast::Statement& statement)
{
    std::visit([this](const auto& node) {
        using Node = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<Node, ast::TensorDecl>) {
            generateTensorDeclaration(node);
        } else if constexpr (std::is_same_v<Node, ast::Assignment>) {
            generateAssignment(node);
        } else if constexpr (std::is_same_v<Node, ast::ReturnStmt>) {
            generateReturn(node);
        } else if constexpr (std::is_same_v<Node, std::shared_ptr<ast::IfStmt>>) {
            if (!node) throw std::runtime_error{"IR generation encountered null if"};
            generateIf(*node);
        }
    }, statement);
}

void IRGenerator::generateTensorDeclaration(const ast::TensorDecl& declaration)
{
    const ValueId value = createValue();
    const sema::TensorType type = lookupType(declaration.name);
    currentBlock().operations.emplace_back(
        TensorDeclOp{value, declaration.name, type});
    if (!values_.emplace(declaration.name, ValueInfo{value, type}).second) {
        throw std::runtime_error{"IR generation encountered duplicate value '" +
                                 declaration.name + "'"};
    }
}

void IRGenerator::generateAssignment(const ast::Assignment& assignment)
{
    values_[assignment.target] = generateExpression(assignment.value);
}

void IRGenerator::generateReturn(const ast::ReturnStmt& returnStatement)
{
    const ValueInfo value = generateExpression(returnStatement.value);
    currentBlock().terminator = ReturnOp{value.value};
    currentBlockId_.reset();
}

void IRGenerator::generateIf(const ast::IfStmt& ifStatement)
{
    const ValueInfo condition = generateExpression(ifStatement.condition);
    if (condition.type.elementType != "i1" || !condition.type.shape.empty()) {
        throw std::runtime_error{"IR generation requires an i1 if condition"};
    }

    const BlockId conditionBlock = *currentBlockId_;
    const BlockId thenBlock = createBlock("if.then");
    const BlockId elseBlock = createBlock("if.else");
    module_.blocks[conditionBlock].terminator =
        CondBranchOp{condition.value, thenBlock, elseBlock};

    const Environment before = values_;

    currentBlockId_ = thenBlock;
    values_ = before;
    for (const auto& statement : ifStatement.thenStatements) {
        if (!currentBlockId_) break;
        generateStatement(statement);
    }
    const std::optional<BlockId> thenEnd = currentBlockId_;
    const Environment thenValues = values_;

    currentBlockId_ = elseBlock;
    values_ = before;
    for (const auto& statement : ifStatement.elseStatements) {
        if (!currentBlockId_) break;
        generateStatement(statement);
    }
    const std::optional<BlockId> elseEnd = currentBlockId_;
    const Environment elseValues = values_;

    if (!thenEnd && !elseEnd) {
        currentBlockId_.reset();
        values_ = before;
        return;
    }

    const BlockId mergeBlock = createBlock("if.end");
    if (thenEnd) module_.blocks[*thenEnd].terminator = BranchOp{mergeBlock};
    if (elseEnd) module_.blocks[*elseEnd].terminator = BranchOp{mergeBlock};
    currentBlockId_ = mergeBlock;

    if (!thenEnd) {
        values_ = elseValues;
        return;
    }
    if (!elseEnd) {
        values_ = thenValues;
        return;
    }

    values_.clear();
    for (const auto& [name, thenValue] : thenValues) {
        const auto elseIterator = elseValues.find(name);
        if (elseIterator == elseValues.end()) continue;
        const ValueInfo& elseValue = elseIterator->second;
        if (!sameType(thenValue.type, elseValue.type)) {
            throw std::runtime_error{"IR branch type mismatch for '" + name + "'"};
        }

        if (thenValue.value == elseValue.value) {
            values_.emplace(name, thenValue);
            continue;
        }

        const ValueId result = createValue();
        currentBlock().operations.emplace_back(PhiOp{
            result,
            thenValue.type,
            {{*thenEnd, thenValue.value}, {*elseEnd, elseValue.value}}
        });
        values_.emplace(name, ValueInfo{result, thenValue.type});
    }
}

IRGenerator::ValueInfo IRGenerator::generateExpression(const ast::Expression& expression)
{
    return std::visit([this](const auto& node) -> ValueInfo {
        using Node = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<Node, ast::IdentifierExpr>) {
            return generateIdentifier(node);
        } else if constexpr (std::is_same_v<Node, ast::IntegerExpr>) {
            return generateInteger(node);
        } else if constexpr (std::is_same_v<Node, ast::FloatExpr>) {
            return generateFloat(node);
        } else if constexpr (std::is_same_v<Node, std::shared_ptr<ast::CallExpr>>) {
            if (!node) throw std::runtime_error{"IR generation encountered null call"};
            return generateCall(*node);
        } else {
            if (!node) throw std::runtime_error{"IR generation encountered null binary expression"};
            return generateBinary(*node);
        }
    }, expression);
}

IRGenerator::ValueInfo IRGenerator::generateIdentifier(
    const ast::IdentifierExpr& identifier) const
{
    const auto found = values_.find(identifier.name);
    if (found == values_.end()) {
        throw std::runtime_error{"IR generation could not resolve identifier '" +
                                 identifier.name + "'"};
    }
    return found->second;
}

IRGenerator::ValueInfo IRGenerator::generateInteger(const ast::IntegerExpr& integer)
{
    const ValueId result = createValue();
    sema::TensorType type{.elementType = "i64", .shape = {}};
    currentBlock().operations.emplace_back(ConstantIntOp{result, integer.value, type});
    return {result, std::move(type)};
}

IRGenerator::ValueInfo IRGenerator::generateFloat(const ast::FloatExpr& floatingPoint)
{
    const ValueId result = createValue();
    sema::TensorType type{.elementType = "f64", .shape = {}};
    currentBlock().operations.emplace_back(
        ConstantFloatOp{result, floatingPoint.value, type});
    return {result, std::move(type)};
}

IRGenerator::ValueInfo IRGenerator::generateBinary(const ast::BinaryExpr& binary)
{
    const ValueInfo lhs = generateExpression(binary.lhs);
    const ValueInfo rhs = generateExpression(binary.rhs);
    if (!sameType(lhs.type, rhs.type)) {
        throw std::runtime_error{"IR binary operand type mismatch"};
    }

    const ValueId result = createValue();
    switch (binary.op) {
    case ast::BinaryOperator::Add:
    case ast::BinaryOperator::Subtract:
    case ast::BinaryOperator::Multiply:
    case ast::BinaryOperator::Divide: {
        const BinaryKind kind =
            binary.op == ast::BinaryOperator::Add ? BinaryKind::Add :
            binary.op == ast::BinaryOperator::Subtract ? BinaryKind::Subtract :
            binary.op == ast::BinaryOperator::Multiply ? BinaryKind::Multiply :
                                                       BinaryKind::Divide;
        currentBlock().operations.emplace_back(
            BinaryOp{result, kind, lhs.value, rhs.value, lhs.type});
        return {result, lhs.type};
    }
    default: {
        CompareKind kind;
        switch (binary.op) {
        case ast::BinaryOperator::Equal: kind = CompareKind::Equal; break;
        case ast::BinaryOperator::NotEqual: kind = CompareKind::NotEqual; break;
        case ast::BinaryOperator::Less: kind = CompareKind::Less; break;
        case ast::BinaryOperator::LessEqual: kind = CompareKind::LessEqual; break;
        case ast::BinaryOperator::Greater: kind = CompareKind::Greater; break;
        case ast::BinaryOperator::GreaterEqual: kind = CompareKind::GreaterEqual; break;
        default: throw std::runtime_error{"unknown comparison operator"};
        }
        currentBlock().operations.emplace_back(
            CompareOp{result, kind, lhs.value, rhs.value, lhs.type});
        return {result, sema::TensorType{.elementType = "i1", .shape = {}}};
    }
    }
}

IRGenerator::ValueInfo IRGenerator::generateCall(const ast::CallExpr& call)
{
    if (call.callee == "relu" && call.arguments.size() == 1) {
        const ValueInfo input = generateExpression(call.arguments[0]);
        const ValueId result = createValue();
        currentBlock().operations.emplace_back(ReluOp{result, input.value, input.type});
        return {result, input.type};
    }
    if (call.callee == "matmul" && call.arguments.size() == 2) {
        const ValueInfo lhs = generateExpression(call.arguments[0]);
        const ValueInfo rhs = generateExpression(call.arguments[1]);
        sema::TensorType type{.elementType = lhs.type.elementType,
                              .shape = {lhs.type.shape.at(0), rhs.type.shape.at(1)}};
        const ValueId result = createValue();
        currentBlock().operations.emplace_back(
            MatMulOp{result, lhs.value, rhs.value, type});
        return {result, std::move(type)};
    }
    throw std::runtime_error{"IR generation encountered unsupported call '" +
                             call.callee + "'"};
}

ValueId IRGenerator::createValue() { return nextValueId_++; }

BlockId IRGenerator::createBlock(std::string name)
{
    const BlockId id = nextBlockId_++;
    module_.blocks.push_back(BasicBlock{id, std::move(name), {}, std::nullopt});
    return id;
}

BasicBlock& IRGenerator::currentBlock()
{
    if (!currentBlockId_) throw std::runtime_error{"no active IR basic block"};
    return module_.blocks.at(*currentBlockId_);
}

const sema::TensorType& IRGenerator::lookupType(const std::string_view name) const
{
    const sema::Symbol* symbol = semanticAnalyzer_.symbols().lookup(name);
    if (!symbol) {
        throw std::runtime_error{"IR generation could not find type for '" +
                                 std::string{name} + "'"};
    }
    return symbol->type;
}

} // namespace ocelotl::ir
