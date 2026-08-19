// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/codegen/llvm/LLVMCodeGenerator.hpp"
#include "ocelotl/codegen/llvm/LLVMOptimizer.hpp"
#include "ocelotl/frontend/Parser.hpp"
#include "ocelotl/ir/IRGenerator.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <gtest/gtest.h>

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>

#include <memory>
#include <string_view>

using namespace ocelotl;

namespace {

struct LoweredModule {
    codegen::LLVMCodeGenerator generator;
    std::unique_ptr<llvm::Module> module;
};

std::unique_ptr<LoweredModule> lower(std::string_view source)
{
    const ast::Program program = frontend::Parser{source}.parseProgram();
    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    ir::IRGenerator irGenerator{analyzer};
    auto lowered = std::make_unique<LoweredModule>();
    lowered->module = lowered->generator.generate(irGenerator.generate(program));
    return lowered;
}

std::size_t countBinaryInstructions(const llvm::Function& function)
{
    std::size_t count = 0;
    for (const auto& block : function) {
        for (const auto& instruction : block) {
            count += llvm::isa<llvm::BinaryOperator>(instruction);
        }
    }
    return count;
}

std::size_t countTruncInstructions(const llvm::Function& function)
{
    std::size_t count = 0;
    for (const auto& block : function) {
        for (const auto& instruction : block) {
            count += llvm::isa<llvm::TruncInst>(instruction);
        }
    }
    return count;
}

const llvm::ConstantInt* returnedInteger(const llvm::Function& function)
{
    if (function.size() != 1U) return nullptr;
    const auto* returnInstruction =
        llvm::dyn_cast<llvm::ReturnInst>(function.front().getTerminator());
    return returnInstruction == nullptr
        ? nullptr
        : llvm::dyn_cast<llvm::ConstantInt>(returnInstruction->getReturnValue());
}

constexpr std::string_view optimizationSource =
    "Dead = 40 + 2\n"
    "X = 12 + 0\n"
    "if X > 10 { Y = X + 1 } else { Y = X - 1 }\n"
    "return Y\n";

} // namespace

TEST(LLVMOptimizerTest, O0PreservesInspectableLoweringStructure)
{
    auto lowered = lower(optimizationSource);
    llvm::Function* function = lowered->module->getFunction("main");
    ASSERT_NE(function, nullptr);
    const std::size_t blocksBefore = function->size();
    const std::size_t binaryBefore = countBinaryInstructions(*function);
    ASSERT_EQ(countTruncInstructions(*function), 1U);

    codegen::LLVMOptimizer{}.optimize(
        *lowered->module, codegen::OptimizationLevel::O0);

    EXPECT_EQ(function->size(), blocksBefore);
    EXPECT_EQ(countBinaryInstructions(*function), binaryBefore);
    EXPECT_EQ(countTruncInstructions(*function), 1U);
    EXPECT_FALSE(llvm::verifyModule(*lowered->module, nullptr));
}

TEST(LLVMOptimizerTest, O1FoldsConstantsEliminatesDeadCodeAndSimplifiesCFG)
{
    auto lowered = lower(optimizationSource);
    llvm::Function* function = lowered->module->getFunction("main");
    ASSERT_NE(function, nullptr);
    ASSERT_GT(function->size(), 1U);
    ASSERT_GT(countBinaryInstructions(*function), 0U);

    codegen::LLVMOptimizer{}.optimize(
        *lowered->module, codegen::OptimizationLevel::O1);

    EXPECT_EQ(function->size(), 1U);
    EXPECT_EQ(countBinaryInstructions(*function), 0U);
    const llvm::ConstantInt* returned = returnedInteger(*function);
    ASSERT_NE(returned, nullptr);
    EXPECT_EQ(returned->getBitWidth(), 32U);
    EXPECT_EQ(returned->getSExtValue(), 13);
}

TEST(LLVMOptimizerTest, O2AndO3PreserveProgramResult)
{
    for (const auto level : {
             codegen::OptimizationLevel::O2,
             codegen::OptimizationLevel::O3}) {
        auto lowered = lower(optimizationSource);
        codegen::LLVMOptimizer{}.optimize(*lowered->module, level);
        const llvm::ConstantInt* returned =
            returnedInteger(*lowered->module->getFunction("main"));
        ASSERT_NE(returned, nullptr);
        EXPECT_EQ(returned->getSExtValue(), 13);
        EXPECT_FALSE(llvm::verifyModule(*lowered->module, nullptr));
    }
}

TEST(LLVMOptimizerTest, RemovesRedundantArithmetic)
{
    auto lowered = lower("X = 9\nY = X + 0\nZ = Y * 1\nreturn Z\n");
    llvm::Function* function = lowered->module->getFunction("main");
    ASSERT_EQ(countBinaryInstructions(*function), 2U);

    codegen::LLVMOptimizer{}.optimize(
        *lowered->module, codegen::OptimizationLevel::O2);

    EXPECT_EQ(countBinaryInstructions(*function), 0U);
    ASSERT_NE(returnedInteger(*function), nullptr);
    EXPECT_EQ(returnedInteger(*function)->getSExtValue(), 9);
}
