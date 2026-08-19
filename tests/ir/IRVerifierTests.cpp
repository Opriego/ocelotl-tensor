#include "ocelotl/ir/IRVerifier.hpp"

#include <gtest/gtest.h>

using namespace ocelotl;

namespace {

const sema::TensorType i64{.elementType = "i64", .shape = {}};
const sema::TensorType f64{.elementType = "f64", .shape = {}};
} // namespace

TEST(IRVerifierTest, RejectsBlockWithoutTerminator)
{
    ir::Module module{0, {{0, "entry", {}, std::nullopt}}};
    EXPECT_THROW(ir::IRVerifier{}.verify(module), ir::IRVerificationError);
}

TEST(IRVerifierTest, RejectsBranchToNonexistentBlock)
{
    ir::Module module{0, {{0, "entry", {{ir::ConstantIntOp{0, 1, i64}}},
                                         ir::BranchOp{99}}}};
    EXPECT_THROW(ir::IRVerifier{}.verify(module), ir::IRVerificationError);
}

TEST(IRVerifierTest, RejectsUseOfNonexistentValue)
{
    ir::Module module{0, {{0, "entry", {}, ir::ReturnOp{42}}}};
    EXPECT_THROW(ir::IRVerifier{}.verify(module), ir::IRVerificationError);
}

TEST(IRVerifierTest, RejectsPhiWithIncorrectPredecessor)
{
    ir::Module module{
        0,
        {
            {0, "entry", {{ir::ConstantIntOp{0, 1, i64}}}, ir::BranchOp{1}},
            {1, "merge", {{ir::PhiOp{1, i64, {{7, 0}}}}}, ir::ReturnOp{1}}
        }
    };
    EXPECT_THROW(ir::IRVerifier{}.verify(module), ir::IRVerificationError);
}

TEST(IRVerifierTest, RejectsPhiOperandTypeMismatch)
{
    ir::Module module{
        0,
        {
            {0, "entry", {{ir::ConstantFloatOp{0, 1.0, f64}}}, ir::BranchOp{1}},
            {1, "merge", {{ir::PhiOp{1, i64, {{0, 0}}}}}, ir::ReturnOp{1}}
        }
    };
    EXPECT_THROW(ir::IRVerifier{}.verify(module), ir::IRVerificationError);
}

TEST(IRVerifierTest, RejectsNonBooleanBranchCondition)
{
    ir::Module module{
        0,
        {
            {0, "entry", {{ir::ConstantIntOp{0, 1, i64}}},
             ir::CondBranchOp{0, 1, 2}},
            {1, "true", {{ir::ConstantIntOp{1, 1, i64}}}, ir::ReturnOp{1}},
            {2, "false", {{ir::ConstantIntOp{2, 0, i64}}}, ir::ReturnOp{2}}
        }
    };
    EXPECT_THROW(ir::IRVerifier{}.verify(module), ir::IRVerificationError);
}
