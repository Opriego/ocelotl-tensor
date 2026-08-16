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
            ->isIntegerTy(64)
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
            "define i64 @main()"
        ),
        std::string::npos
    );

    EXPECT_NE(
        llvmIR.find(
            "ret i64 42"
        ),
        std::string::npos
    );
}

TEST(
    LLVMCodeGeneratorTest,
    GeneratesValidFloatingPointFunction
)
{
    constexpr std::string_view source =
        "X = 3.14\n"
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

    llvm::Function* mainFunction =
        llvmModule->getFunction("main");

    ASSERT_NE(
        mainFunction,
        nullptr
    );

    EXPECT_TRUE(
        mainFunction
            ->getReturnType()
            ->isDoubleTy()
    );

    EXPECT_FALSE(
        llvm::verifyModule(
            *llvmModule,
            nullptr
        )
    );
}
