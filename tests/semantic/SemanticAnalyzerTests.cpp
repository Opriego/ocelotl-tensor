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
        "return A\n";

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
        "return C\n";

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
        "return D\n";

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
        "f64"
    );

    EXPECT_TRUE(
        symbol->type.shape.empty()
    );
}


