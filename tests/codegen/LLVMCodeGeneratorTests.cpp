// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/codegen/llvm/LLVMCodeGenerator.hpp"
#include "ocelotl/frontend/Parser.hpp"
#include "ocelotl/ir/IRGenerator.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <gtest/gtest.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>

#include <memory>
#include <string>
#include <string_view>

using namespace ocelotl;

namespace {

struct CompilationFixture {
    ast::Program program;
    sema::SemanticAnalyzer semanticAnalyzer;

    explicit CompilationFixture(
        const std::string_view source
    )
        : program{
            frontend::Parser{source}
                .parseProgram()
        }
    {
        semanticAnalyzer.analyze(program);
    }
};

} // namespace

TEST(
    LLVMCodeGeneratorTest,
    GeneratesValidIntegerMainFunction
)
{
    constexpr std::string_view source =
        "X = 42\n"
        "return X\n";

    CompilationFixture fixture{
        source
    };

    ir::IRGenerator irGenerator{
        fixture.semanticAnalyzer
    };

    const ir::Module irModule =
        irGenerator.generate(
            fixture.program
        );

    codegen::LLVMCodeGenerator generator;

    std::unique_ptr<llvm::Module>
        llvmModule =
            generator.generate(irModule);

    ASSERT_NE(
        llvmModule,
        nullptr
    );

    EXPECT_FALSE(
        llvm::verifyModule(
            *llvmModule,
            nullptr
        )
    );

    llvm::Function* mainFunction =
        llvmModule->getFunction("main");

    ASSERT_NE(
        mainFunction,
        nullptr
    );

    EXPECT_TRUE(
        mainFunction
            ->getReturnType()
            ->isIntegerTy(32)
    );
}

TEST(
    LLVMCodeGeneratorTest,
    EmitsIntegerLLVMIR
)
{
    constexpr std::string_view source =
        "X = 42\n"
        "return X\n";

    CompilationFixture fixture{
        source
    };

    ir::IRGenerator irGenerator{
        fixture.semanticAnalyzer
    };

    const ir::Module irModule =
        irGenerator.generate(
            fixture.program
        );

    codegen::LLVMCodeGenerator generator;

    const auto llvmModule =
        generator.generate(irModule);

    const std::string llvmIR =
        generator.emitToString(
            *llvmModule
        );

    EXPECT_NE(
        llvmIR.find(
            "define i32 @main()"
        ),
        std::string::npos
    );

    EXPECT_NE(
        llvmIR.find(
            "trunc i64 42 to i32"
        ),
        std::string::npos
    );
    EXPECT_NE(llvmIR.find("ret i32 %exit.status"), std::string::npos);
    EXPECT_EQ(llvmIR.find("ocelotl_rt_v1_"), std::string::npos);
}

TEST(
    LLVMCodeGeneratorTest,
    PreservesFloatingPointLoweringWithIntegerProgramStatus
)
{
    constexpr std::string_view source =
        "X = 3.14\n"
        "Y = X + 1.0\n"
        "return 0\n";

    CompilationFixture fixture{
        source
    };

    ir::IRGenerator irGenerator{
        fixture.semanticAnalyzer
    };

    const ir::Module irModule =
        irGenerator.generate(
            fixture.program
        );

    codegen::LLVMCodeGenerator generator;

    const auto llvmModule =
        generator.generate(irModule);

    llvm::Function* mainFunction =
        llvmModule->getFunction("main");

    ASSERT_NE(
        mainFunction,
        nullptr
    );

    EXPECT_TRUE(
        mainFunction
            ->getReturnType()
            ->isIntegerTy(32)
    );

    EXPECT_NE(
        generator.emitToString(*llvmModule).find("fadd double"),
        std::string::npos
    );

    EXPECT_FALSE(
        llvm::verifyModule(
            *llvmModule,
            nullptr
        )
    );
}

TEST(LLVMCodeGeneratorTest, LowersConditionalBranchesArithmeticAndPhi)
{
    constexpr std::string_view source =
        "X = 12\n"
        "if X > 10 { Y = X + 1 } else { Y = X - 1 }\n"
        "return Y\n";
    CompilationFixture fixture{source};
    ir::IRGenerator irGenerator{fixture.semanticAnalyzer};
    const ir::Module irModule = irGenerator.generate(fixture.program);
    codegen::LLVMCodeGenerator generator;
    const auto llvmModule = generator.generate(irModule);
    const std::string output = generator.emitToString(*llvmModule);

    EXPECT_NE(output.find("icmp sgt i64"), std::string::npos);
    EXPECT_NE(output.find("br i1"), std::string::npos);
    EXPECT_NE(output.find("add i64"), std::string::npos);
    EXPECT_NE(output.find("sub i64"), std::string::npos);
    EXPECT_NE(output.find("phi i64"), std::string::npos);
    EXPECT_FALSE(llvm::verifyModule(*llvmModule, nullptr));
}

TEST(LLVMCodeGeneratorTest, LowersFalseBranchProgram)
{
    constexpr std::string_view source =
        "X = 2\n"
        "if X > 10 { Y = X + 1 } else { Y = X - 1 }\n"
        "return Y\n";
    CompilationFixture fixture{source};
    ir::IRGenerator irGenerator{fixture.semanticAnalyzer};
    codegen::LLVMCodeGenerator generator;
    const auto llvmModule = generator.generate(
        irGenerator.generate(fixture.program));
    EXPECT_FALSE(llvm::verifyModule(*llvmModule, nullptr));
}

TEST(LLVMCodeGeneratorTest, LowersNestedIf)
{
    constexpr std::string_view source =
        "X = 5\n"
        "if X > 0 { if X < 10 { Y = X + 2 } else { Y = X - 2 } } "
        "else { Y = 1 } return Y";
    CompilationFixture fixture{source};
    ir::IRGenerator irGenerator{fixture.semanticAnalyzer};
    codegen::LLVMCodeGenerator generator;
    const auto llvmModule = generator.generate(
        irGenerator.generate(fixture.program));
    const std::string output = generator.emitToString(*llvmModule);
    EXPECT_NE(output.find("phi i64"), std::string::npos);
    EXPECT_FALSE(llvm::verifyModule(*llvmModule, nullptr));
}

TEST(LLVMCodeGeneratorTest, LowersReturnsInBothBranches)
{
    constexpr std::string_view source =
        "X = 12\n"
        "if X > 10 { return X + 1 } else { return X - 1 }\n";
    CompilationFixture fixture{source};
    ir::IRGenerator irGenerator{fixture.semanticAnalyzer};
    codegen::LLVMCodeGenerator generator;
    const auto llvmModule = generator.generate(
        irGenerator.generate(fixture.program));
    EXPECT_FALSE(llvm::verifyModule(*llvmModule, nullptr));
    EXPECT_EQ(llvmModule->getFunction("main")->size(), 3U);
}

TEST(LLVMCodeGeneratorTest, LowersFloatingPointControlFlow)
{
    constexpr std::string_view source =
        "X = 1.5\n"
        "if X <= 2.0 { Y = X * 2.0 } else { Y = X / 2.0 }\n"
        "return 0\n";
    CompilationFixture fixture{source};
    ir::IRGenerator irGenerator{fixture.semanticAnalyzer};
    codegen::LLVMCodeGenerator generator;
    const auto llvmModule = generator.generate(
        irGenerator.generate(fixture.program));
    const std::string output = generator.emitToString(*llvmModule);
    EXPECT_NE(output.find("fcmp ole double"), std::string::npos);
    EXPECT_NE(output.find("fmul double"), std::string::npos);
    EXPECT_NE(output.find("fdiv double"), std::string::npos);
    EXPECT_NE(output.find("phi double"), std::string::npos);
}

TEST(LLVMCodeGeneratorTest, RejectsMalformedFloatingPointReturnIR)
{
    const sema::TensorType f64{.elementType = "f64", .shape = {}};
    const ir::Module malformed{
        0,
        {{0, "entry", {{ir::ConstantFloatOp{0, 1.5, f64}}},
          ir::ReturnOp{0}}}
    };
    codegen::LLVMCodeGenerator generator;

    EXPECT_THROW(
        static_cast<void>(generator.generate(malformed)),
        std::runtime_error
    );
}

TEST(LLVMCodeGeneratorTest, TruncatesProgramStatusToLow32Bits)
{
    CompilationFixture fixture{"return 4294967338"};
    ir::IRGenerator irGenerator{fixture.semanticAnalyzer};
    codegen::LLVMCodeGenerator generator;
    const auto llvmModule = generator.generate(
        irGenerator.generate(fixture.program));
    const std::string output = generator.emitToString(*llvmModule);

    EXPECT_NE(output.find("trunc i64 4294967338 to i32"), std::string::npos);
    EXPECT_FALSE(llvm::verifyModule(*llvmModule, nullptr));
}

TEST(LLVMCodeGeneratorTest, LowersTensorStorageToRuntimeABI)
{
    constexpr std::string_view source =
        "tensor Scratch: f64[4,4]\n"
        "return 7\n";
    CompilationFixture fixture{source};
    ir::IRGenerator irGenerator{fixture.semanticAnalyzer};
    codegen::LLVMCodeGenerator generator;
    const auto llvmModule = generator.generate(
        irGenerator.generate(fixture.program));
    const std::string output = generator.emitToString(*llvmModule);

    EXPECT_NE(output.find("declare ptr @ocelotl_rt_v1_alloc(i64, i64)"),
              std::string::npos);
    EXPECT_NE(output.find("call ptr @ocelotl_rt_v1_alloc(i64 128, i64 64)"),
              std::string::npos);
    EXPECT_NE(output.find("call void @ocelotl_rt_v1_free(ptr"),
              std::string::npos);
    EXPECT_FALSE(llvm::verifyModule(*llvmModule, nullptr));
}
