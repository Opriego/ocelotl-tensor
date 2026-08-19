#include "ocelotl/frontend/Parser.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using namespace ocelotl;

namespace {

ast::Program parse(
    const std::string_view source
)
{
    frontend::Parser parser{source};
    return parser.parseProgram();
}

} // namespace

TEST(
    SemanticAnalyzerTest,
    RegistersTensorDeclaration
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    const sema::Symbol* symbol =
        analyzer.symbols().lookup("A");

    ASSERT_NE(symbol, nullptr);

    EXPECT_EQ(symbol->name, "A");
    EXPECT_EQ(
        symbol->type.elementType,
        "f32"
    );

    ASSERT_EQ(
        symbol->type.shape.size(),
        2
    );

    EXPECT_EQ(
        symbol->type.shape[0],
        1024
    );

    EXPECT_EQ(
        symbol->type.shape[1],
        1024
    );
}

TEST(
    SemanticAnalyzerTest,
    RejectsDuplicateTensorDeclaration
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]\n"
        "tensor A: f32[1024,1024]\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    EXPECT_THROW(
        analyzer.analyze(program),
        sema::SemanticError
    );
}

TEST(
    SemanticAnalyzerTest,
    ResolvesDeclaredIdentifier
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]\n"
        "return 0\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    EXPECT_NO_THROW(
        analyzer.analyze(program)
    );
}

TEST(
    SemanticAnalyzerTest,
    RejectsUnknownIdentifier
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]\n"
        "return X\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    EXPECT_THROW(
        analyzer.analyze(program),
        sema::SemanticError
    );
}

TEST(
    SemanticAnalyzerTest,
    ValidatesCallArguments
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]\n"
        "tensor B: f32[1024,1024]\n"
        "C = matmul(A, B)\n"
        "return 0\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    EXPECT_NO_THROW(
        analyzer.analyze(program)
    );

    EXPECT_TRUE(
        analyzer.symbols().contains("A")
    );

    EXPECT_TRUE(
        analyzer.symbols().contains("B")
    );

    EXPECT_TRUE(
        analyzer.symbols().contains("C")
    );
}

TEST(
    SemanticAnalyzerTest,
    RejectsUnknownCallArgument
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]\n"
        "C = matmul(A, X)\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    EXPECT_THROW(
        analyzer.analyze(program),
        sema::SemanticError
    );
}

TEST(
    SemanticAnalyzerTest,
    AcceptsCompleteProgram
)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]\n"
        "tensor B: f32[1024,1024]\n"
        "C = matmul(A, B)\n"
        "D = relu(C)\n"
        "return 0\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    EXPECT_NO_THROW(
        analyzer.analyze(program)
    );

    EXPECT_TRUE(
        analyzer.symbols().contains("A")
    );

    EXPECT_TRUE(
        analyzer.symbols().contains("B")
    );

    EXPECT_TRUE(
        analyzer.symbols().contains("C")
    );

    EXPECT_TRUE(
        analyzer.symbols().contains("D")
    );

    EXPECT_EQ(
        analyzer.symbols().size(),
        4
    );
}

TEST(
    SemanticAnalyzerTest,
    ReportsDuplicateSymbolName
)
{
    constexpr std::string_view source =
        "tensor A: f32[2,2]\n"
        "tensor A: f32[2,2]\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    try {
        analyzer.analyze(program);

        FAIL()
            << "Expected semantic analysis to fail";
    }
    catch (const sema::SemanticError& error) {
        const std::string message =
            error.what();

        EXPECT_NE(
            message.find("A"),
            std::string::npos
        );

        EXPECT_NE(
            message.find("redeclaration"),
            std::string::npos
        );
    }
}

TEST(
    SemanticAnalyzerTest,
    ReportsUnknownIdentifierName
)
{
    constexpr std::string_view source =
        "return MissingTensor\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;

    try {
        analyzer.analyze(program);

        FAIL()
            << "Expected semantic analysis to fail";
    }
    catch (const sema::SemanticError& error) {
        const std::string message =
            error.what();

        EXPECT_NE(
            message.find("MissingTensor"),
            std::string::npos
        );

        EXPECT_NE(
            message.find("undeclared"),
            std::string::npos
        );
    }
}

TEST(
    SemanticAnalyzerTest,
    InfersIntegerLiteralType
)
{
    constexpr std::string_view source =
        "X = 42\n"
        "return X\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    const sema::Symbol* symbol =
        analyzer.symbols().lookup("X");

    ASSERT_NE(symbol, nullptr);

    EXPECT_EQ(
        symbol->type.elementType,
        "i64"
    );

    EXPECT_TRUE(
        symbol->type.shape.empty()
    );
}

TEST(
    SemanticAnalyzerTest,
    InfersFloatLiteralType
)
{
    constexpr std::string_view source =
        "X = 3.14\n"
        "return 0\n";

    const ast::Program program =
        parse(source);

    sema::SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    const sema::Symbol* symbol =
        analyzer.symbols().lookup("X");

    ASSERT_NE(symbol, nullptr);

    EXPECT_EQ(
        symbol->type.elementType,
        "f64"
    );

    EXPECT_TRUE(
        symbol->type.shape.empty()
    );
}

TEST(SemanticAnalyzerTest, AcceptsIntegerArithmeticAndComparison)
{
    const ast::Program program = parse(
        "X = 4 + 2 * 3\nC = X >= 10\n"
        "if C { Y = X + 1 } else { Y = X - 1 }\nreturn Y");
    sema::SemanticAnalyzer analyzer;
    EXPECT_NO_THROW(analyzer.analyze(program));
    ASSERT_NE(analyzer.symbols().lookup("C"), nullptr);
    EXPECT_EQ(analyzer.symbols().lookup("C")->type.elementType, "i1");
    ASSERT_NE(analyzer.symbols().lookup("Y"), nullptr);
    EXPECT_EQ(analyzer.symbols().lookup("Y")->type.elementType, "i64");
}

TEST(SemanticAnalyzerTest, AcceptsFloatingPointArithmeticAndComparison)
{
    const ast::Program program = parse(
        "X = 3.0 / 2.0\n"
        "if X < 2.0 { Y = X + 1.0 } else { Y = X - 1.0 }\n"
        "return 0");
    sema::SemanticAnalyzer analyzer;
    EXPECT_NO_THROW(analyzer.analyze(program));
}

TEST(SemanticAnalyzerTest, AcceptsIntegerProgramStatus)
{
    const ast::Program program = parse("return 42");
    sema::SemanticAnalyzer analyzer;
    EXPECT_NO_THROW(analyzer.analyze(program));
}

TEST(SemanticAnalyzerTest, RejectsFloatingPointProgramStatusWithLocation)
{
    const ast::Program program = parse("\nreturn 1.5");
    sema::SemanticAnalyzer analyzer;

    try {
        analyzer.analyze(program);
        FAIL() << "Expected semantic analysis to reject an f64 program status";
    } catch (const sema::SemanticError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("2:1"), std::string::npos);
        EXPECT_NE(message.find("scalar i64"), std::string::npos);
    }
}

TEST(SemanticAnalyzerTest, RejectsBooleanProgramStatus)
{
    const ast::Program program = parse("return 1 == 1");
    sema::SemanticAnalyzer analyzer;
    EXPECT_THROW(analyzer.analyze(program), sema::SemanticError);
}

TEST(SemanticAnalyzerTest, RejectsShapedProgramStatus)
{
    const ast::Program program = parse(
        "tensor Result: f64[2,2]\nreturn Result");
    sema::SemanticAnalyzer analyzer;
    EXPECT_THROW(analyzer.analyze(program), sema::SemanticError);
}

TEST(SemanticAnalyzerTest, PreservesInconsistentReturnDiagnostic)
{
    const ast::Program program = parse(
        "if 1 == 1 { return 1 } else { return 1.5 }");
    sema::SemanticAnalyzer analyzer;

    try {
        analyzer.analyze(program);
        FAIL() << "Expected inconsistent return types to be rejected";
    } catch (const sema::SemanticError& error) {
        EXPECT_NE(
            std::string{error.what()}.find("return type mismatch"),
            std::string::npos
        );
    }
}

TEST(SemanticAnalyzerTest, RejectsNonBooleanIfCondition)
{
    const ast::Program program = parse(
        "if 42 { X = 1 } else { X = 0 }\nreturn X");
    sema::SemanticAnalyzer analyzer;
    EXPECT_THROW(analyzer.analyze(program), sema::SemanticError);
}

TEST(SemanticAnalyzerTest, RejectsMixedArithmeticTypes)
{
    const ast::Program program = parse("X = 1 + 2.0\nreturn X");
    sema::SemanticAnalyzer analyzer;
    EXPECT_THROW(analyzer.analyze(program), sema::SemanticError);
}

TEST(SemanticAnalyzerTest, DoesNotExposeOneSidedBranchBinding)
{
    const ast::Program program = parse(
        "if 1 == 1 { X = 1 } else { Y = 2 }\nreturn X");
    sema::SemanticAnalyzer analyzer;
    EXPECT_THROW(analyzer.analyze(program), sema::SemanticError);
}
