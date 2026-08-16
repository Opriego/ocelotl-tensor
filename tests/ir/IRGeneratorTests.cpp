#include "ocelotl/frontend/Parser.hpp"
#include "ocelotl/ir/IR.hpp"
#include "ocelotl/ir/IRGenerator.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <gtest/gtest.h>

#include <string_view>
#include <variant>

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
    IRGeneratorTest,
    GeneratesTensorDeclaration
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,512]\n"
        "return A\n";

    CompilationFixture fixture{source};

    ir::IRGenerator generator{
        fixture.semanticAnalyzer
    };

    const ir::Module module =
        generator.generate(
            fixture.program
        );

    ASSERT_EQ(
        module.operations.size(),
        2
    );

    ASSERT_TRUE(
        std::holds_alternative<
            ir::TensorDeclOp
        >(
            module.operations[0]
        )
    );

    const auto& declaration =
        std::get<ir::TensorDeclOp>(
            module.operations[0]
        );

    EXPECT_EQ(
        declaration.result,
        0
    );

    EXPECT_EQ(
        declaration.name,
        "A"
    );

    EXPECT_EQ(
        declaration.type.elementType,
        "f32"
    );

    ASSERT_EQ(
        declaration.type.shape.size(),
        2
    );

    EXPECT_EQ(
        declaration.type.shape[0],
        1024
    );

    EXPECT_EQ(
        declaration.type.shape[1],
        512
    );
}

TEST(
    IRGeneratorTest,
    GeneratesReturn
)
{
    constexpr std::string_view source =
        "tensor A: f32[2,2]\n"
        "return A\n";

    CompilationFixture fixture{source};

    ir::IRGenerator generator{
        fixture.semanticAnalyzer
    };

    const ir::Module module =
        generator.generate(
            fixture.program
        );

    ASSERT_EQ(
        module.operations.size(),
        2
    );

    ASSERT_TRUE(
        std::holds_alternative<
            ir::ReturnOp
        >(
            module.operations[1]
        )
    );

    const auto& returnOp =
        std::get<ir::ReturnOp>(
            module.operations[1]
        );

    EXPECT_EQ(
        returnOp.value,
        0
    );
}

TEST(
    IRGeneratorTest,
    GeneratesMatMul
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,512]\n"
        "tensor B: f32[512,256]\n"
        "C = matmul(A, B)\n"
        "return C\n";

    CompilationFixture fixture{source};

    ir::IRGenerator generator{
        fixture.semanticAnalyzer
    };

    const ir::Module module =
        generator.generate(
            fixture.program
        );

    ASSERT_EQ(
        module.operations.size(),
        4
    );

    ASSERT_TRUE(
        std::holds_alternative<
            ir::MatMulOp
        >(
            module.operations[2]
        )
    );

    const auto& matmul =
        std::get<ir::MatMulOp>(
            module.operations[2]
        );

    EXPECT_EQ(matmul.result, 2);

    EXPECT_EQ(matmul.lhs, 0);
    EXPECT_EQ(matmul.rhs, 1);

    EXPECT_EQ(
        matmul.type.elementType,
        "f32"
    );

    ASSERT_EQ(
        matmul.type.shape.size(),
        2
    );

    EXPECT_EQ(
        matmul.type.shape[0],
        1024
    );

    EXPECT_EQ(
        matmul.type.shape[1],
        256
    );
}

TEST(
    IRGeneratorTest,
    GeneratesRelu
)
{
    constexpr std::string_view source =
        "tensor A: f32[4,4]\n"
        "B = relu(A)\n"
        "return B\n";

    CompilationFixture fixture{source};

    ir::IRGenerator generator{
        fixture.semanticAnalyzer
    };

    const ir::Module module =
        generator.generate(
            fixture.program
        );

    ASSERT_EQ(
        module.operations.size(),
        3
    );

    ASSERT_TRUE(
        std::holds_alternative<
            ir::ReluOp
        >(
            module.operations[1]
        )
    );

    const auto& relu =
        std::get<ir::ReluOp>(
            module.operations[1]
        );

    EXPECT_EQ(
        relu.result,
        1
    );

    EXPECT_EQ(
        relu.input,
        0
    );

    EXPECT_EQ(
        relu.type.elementType,
        "f32"
    );

    ASSERT_EQ(
        relu.type.shape.size(),
        2
    );

    EXPECT_EQ(
        relu.type.shape[0],
        4
    );

    EXPECT_EQ(
        relu.type.shape[1],
        4
    );
}

TEST(
    IRGeneratorTest,
    GeneratesCompleteTensorPipeline
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,512]\n"
        "tensor B: f32[512,256]\n"
        "C = matmul(A, B)\n"
        "D = relu(C)\n"
        "return D\n";

    CompilationFixture fixture{source};

    ir::IRGenerator generator{
        fixture.semanticAnalyzer
    };

    const ir::Module module =
        generator.generate(
            fixture.program
        );

    ASSERT_EQ(
        module.operations.size(),
        5
    );

    EXPECT_TRUE(
        std::holds_alternative<
            ir::TensorDeclOp
        >(
            module.operations[0]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<
            ir::TensorDeclOp
        >(
            module.operations[1]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<
            ir::MatMulOp
        >(
            module.operations[2]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<
            ir::ReluOp
        >(
            module.operations[3]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<
            ir::ReturnOp
        >(
            module.operations[4]
        )
    );

    const auto& matmul =
        std::get<ir::MatMulOp>(
            module.operations[2]
        );

    const auto& relu =
        std::get<ir::ReluOp>(
            module.operations[3]
        );

    const auto& returnOp =
        std::get<ir::ReturnOp>(
            module.operations[4]
        );

    EXPECT_EQ(matmul.result, 2);
    EXPECT_EQ(relu.result, 3);
    EXPECT_EQ(returnOp.value, 3);
}
