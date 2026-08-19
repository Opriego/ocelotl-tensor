#include "ocelotl/frontend/Parser.hpp"
#include "ocelotl/ir/IR.hpp"
#include "ocelotl/ir/IRGenerator.hpp"
#include "ocelotl/ir/IRVerifier.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <variant>

using namespace ocelotl;

namespace {

ir::Module generate(const std::string_view source)
{
    const ast::Program program = frontend::Parser{source}.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    return ir::IRGenerator{analyzer}.generate(program);
}

} // namespace

TEST(IRGeneratorTest, GeneratesSingleBlockScalarProgram)
{
    const ir::Module module = generate("X = 42\nreturn X\n");
    ASSERT_EQ(module.blocks.size(), 1U);
    ASSERT_EQ(module.blocks[0].operations.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<ir::ConstantIntOp>(
        module.blocks[0].operations[0]));
    EXPECT_TRUE(std::holds_alternative<ir::ReturnOp>(
        *module.blocks[0].terminator));
}

TEST(IRGeneratorTest, GeneratesArithmeticAndComparisonOperations)
{
    const ir::Module module = generate(
        "X = 4 + 3 * 2\n"
        "C = X >= 10\n"
        "if C { Y = X / 2 } else { Y = X - 1 }\n"
        "return Y\n");

    bool sawMultiply = false;
    bool sawAdd = false;
    bool sawCompare = false;
    for (const auto& operation : module.blocks[0].operations) {
        if (const auto* binary = std::get_if<ir::BinaryOp>(&operation)) {
            sawMultiply |= binary->kind == ir::BinaryKind::Multiply;
            sawAdd |= binary->kind == ir::BinaryKind::Add;
        }
        sawCompare |= std::holds_alternative<ir::CompareOp>(operation);
    }
    EXPECT_TRUE(sawMultiply);
    EXPECT_TRUE(sawAdd);
    EXPECT_TRUE(sawCompare);
}

TEST(IRGeneratorTest, GeneratesConditionalCFGAndPhi)
{
    const ir::Module module = generate(
        "X = 12\n"
        "if X > 10 { Y = X + 1 } else { Y = X - 1 }\n"
        "return Y\n");

    ASSERT_EQ(module.blocks.size(), 4U);
    EXPECT_TRUE(std::holds_alternative<ir::CondBranchOp>(
        *module.blocks[0].terminator));
    EXPECT_TRUE(std::holds_alternative<ir::BranchOp>(
        *module.blocks[1].terminator));
    EXPECT_TRUE(std::holds_alternative<ir::BranchOp>(
        *module.blocks[2].terminator));

    ASSERT_FALSE(module.blocks[3].operations.empty());
    const auto* phi = std::get_if<ir::PhiOp>(&module.blocks[3].operations[0]);
    ASSERT_NE(phi, nullptr);
    EXPECT_EQ(phi->incoming.size(), 2U);
    EXPECT_EQ(phi->type.elementType, "i64");
}

TEST(IRGeneratorTest, GeneratesNestedConditionalCFG)
{
    const ir::Module module = generate(
        "X = 5\n"
        "if X > 0 {"
        "  if X < 10 { Y = X + 1 } else { Y = X - 1 }"
        "} else { Y = 0 }\n"
        "return Y\n");

    EXPECT_GE(module.blocks.size(), 7U);
    EXPECT_NO_THROW(ir::IRVerifier{}.verify(module));
}

TEST(IRGeneratorTest, PreservesTensorOperationsInBlockIR)
{
    const ir::Module module = generate(
        "tensor A: f32[2,2]\n"
        "return 0\n");
    ASSERT_EQ(module.blocks.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<ir::TensorDeclOp>(
        module.blocks[0].operations[0]));
}
