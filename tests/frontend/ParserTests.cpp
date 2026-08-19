#include "ocelotl/ast/AST.hpp"
#include "ocelotl/frontend/Parser.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

using namespace ocelotl;

namespace {

// Helper used by negative parser tests.
//
// Parser::parseProgram() is [[nodiscard]], which is appropriate because
// silently discarding a successfully parsed AST is generally a bug.
//
// Negative tests intentionally expect parsing to fail, so we explicitly
// bind the return value. If parsing unexpectedly succeeds, the value is
// still handled correctly and no [[nodiscard]] warning is generated.
void parseExpectingFailure(frontend::Parser& parser)
{
    [[maybe_unused]] const auto program = parser.parseProgram();
}

} // namespace

TEST(ParserTest, ParsesTensorDeclaration)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]";

    frontend::Parser parser{source};
    const ast::Program program = parser.parseProgram();

    ASSERT_EQ(program.statements.size(), 1);

    ASSERT_TRUE(
        std::holds_alternative<ast::TensorDecl>(
            program.statements[0]
        )
    );

    const auto& declaration =
        std::get<ast::TensorDecl>(
            program.statements[0]
        );

    EXPECT_EQ(declaration.name, "A");
    EXPECT_EQ(declaration.type.elementType, "f32");

    ASSERT_EQ(declaration.type.shape.size(), 2);

    EXPECT_EQ(declaration.type.shape[0], 1024);
    EXPECT_EQ(declaration.type.shape[1], 1024);

    EXPECT_EQ(declaration.location.line, 1);
    EXPECT_EQ(declaration.location.column, 1);
}

TEST(ParserTest, ParsesAssignment)
{
    constexpr std::string_view source =
        "C = A";

    frontend::Parser parser{source};
    const ast::Program program = parser.parseProgram();

    ASSERT_EQ(program.statements.size(), 1);

    ASSERT_TRUE(
        std::holds_alternative<ast::Assignment>(
            program.statements[0]
        )
    );

    const auto& assignment =
        std::get<ast::Assignment>(
            program.statements[0]
        );

    EXPECT_EQ(assignment.target, "C");

    ASSERT_TRUE(
        std::holds_alternative<ast::IdentifierExpr>(
            assignment.value
        )
    );

    const auto& value =
        std::get<ast::IdentifierExpr>(
            assignment.value
        );

    EXPECT_EQ(value.name, "A");
}

TEST(ParserTest, ParsesCallExpression)
{
    constexpr std::string_view source =
        "C = matmul(A, B)";

    frontend::Parser parser{source};
    const ast::Program program = parser.parseProgram();

    ASSERT_EQ(program.statements.size(), 1);

    ASSERT_TRUE(
        std::holds_alternative<ast::Assignment>(
            program.statements[0]
        )
    );

    const auto& assignment =
        std::get<ast::Assignment>(
            program.statements[0]
        );

    ASSERT_TRUE(
        std::holds_alternative<
            std::shared_ptr<ast::CallExpr>
        >(assignment.value)
    );

    const auto& call =
        std::get<std::shared_ptr<ast::CallExpr>>(
            assignment.value
        );

    ASSERT_NE(call, nullptr);

    EXPECT_EQ(call->callee, "matmul");

    ASSERT_EQ(call->arguments.size(), 2);

    ASSERT_TRUE(
        std::holds_alternative<ast::IdentifierExpr>(
            call->arguments[0]
        )
    );

    ASSERT_TRUE(
        std::holds_alternative<ast::IdentifierExpr>(
            call->arguments[1]
        )
    );

    const auto& lhs =
        std::get<ast::IdentifierExpr>(
            call->arguments[0]
        );

    const auto& rhs =
        std::get<ast::IdentifierExpr>(
            call->arguments[1]
        );

    EXPECT_EQ(lhs.name, "A");
    EXPECT_EQ(rhs.name, "B");
}

TEST(ParserTest, ParsesCompleteProgram)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,1024]\n"
        "tensor B: f32[1024,1024]\n"
        "C = matmul(A, B)\n"
        "D = relu(C)\n"
        "return D\n";

    frontend::Parser parser{source};
    const ast::Program program = parser.parseProgram();

    ASSERT_EQ(program.statements.size(), 5);

    EXPECT_TRUE(
        std::holds_alternative<ast::TensorDecl>(
            program.statements[0]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<ast::TensorDecl>(
            program.statements[1]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<ast::Assignment>(
            program.statements[2]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<ast::Assignment>(
            program.statements[3]
        )
    );

    EXPECT_TRUE(
        std::holds_alternative<ast::ReturnStmt>(
            program.statements[4]
        )
    );

    const auto& returnStatement =
        std::get<ast::ReturnStmt>(
            program.statements[4]
        );

    ASSERT_TRUE(
        std::holds_alternative<ast::IdentifierExpr>(
            returnStatement.value
        )
    );

    EXPECT_EQ(
        std::get<ast::IdentifierExpr>(
            returnStatement.value
        ).name,
        "D"
    );
}

TEST(ParserTest, RejectsTensorDeclarationWithoutColon)
{
    constexpr std::string_view source =
        "tensor A f32[1024,1024]";

    frontend::Parser parser{source};

    EXPECT_THROW(
        parseExpectingFailure(parser),
        std::runtime_error
    );
}

TEST(ParserTest, RejectsTensorDeclarationWithMissingDimension)
{
    constexpr std::string_view source =
        "tensor A: f32[1024,]";

    frontend::Parser parser{source};

    EXPECT_THROW(
        parseExpectingFailure(parser),
        std::runtime_error
    );
}

TEST(ParserTest, RejectsAssignmentWithoutEqual)
{
    constexpr std::string_view source =
        "C matmul(A, B)";

    frontend::Parser parser{source};

    EXPECT_THROW(
        parseExpectingFailure(parser),
        std::runtime_error
    );
}

TEST(ParserTest, RejectsCallWithoutClosingParenthesis)
{
    constexpr std::string_view source =
        "C = matmul(A, B";

    frontend::Parser parser{source};

    EXPECT_THROW(
        parseExpectingFailure(parser),
        std::runtime_error
    );
}

TEST(ParserTest, RejectsUnknownToken)
{
    constexpr std::string_view source =
        "@";

    frontend::Parser parser{source};

    EXPECT_THROW(
        parseExpectingFailure(parser),
        std::runtime_error
    );
}

TEST(ParserTest, RejectsReturnWithoutExpression)
{
    constexpr std::string_view source =
        "return";

    frontend::Parser parser{source};

    EXPECT_THROW(
        parseExpectingFailure(parser),
        std::runtime_error
    );
}

TEST(ParserTest, ReportsLocationForMissingColon)
{
    constexpr std::string_view source =
        "tensor A f32[1024,1024]";

    frontend::Parser parser{source};

    try {
        [[maybe_unused]] const auto program =
            parser.parseProgram();

        FAIL() << "Expected parser to throw";
    }
    catch (const std::runtime_error& error) {
        const std::string message = error.what();

        EXPECT_NE(
            message.find("1:10"),
            std::string::npos
        );

        EXPECT_NE(
            message.find("expected ':'"),
            std::string::npos
        );
    }
}

TEST(ParserTest, ReportsMissingClosingParenthesis)
{
    constexpr std::string_view source =
        "C = matmul(A, B";

    frontend::Parser parser{source};

    try {
        [[maybe_unused]] const auto program =
            parser.parseProgram();

        FAIL() << "Expected parser to throw";
    }
    catch (const std::runtime_error& error) {
        const std::string message = error.what();

        EXPECT_NE(
            message.find("expected ')'"),
            std::string::npos
        );
    }
}

TEST(ParserTest, ParsesArithmeticWithPrecedence)
{
    const ast::Program program = frontend::Parser{"X = 1 + 2 * 3"}.parseProgram();
    const auto& assignment = std::get<ast::Assignment>(program.statements[0]);
    const auto root = std::get<std::shared_ptr<ast::BinaryExpr>>(assignment.value);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->op, ast::BinaryOperator::Add);

    const auto rhs = std::get<std::shared_ptr<ast::BinaryExpr>>(root->rhs);
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->op, ast::BinaryOperator::Multiply);
}

TEST(ParserTest, ParsesIfElseAndComparison)
{
    const ast::Program program = frontend::Parser{
        "if 4 > 2 { X = 1 } else { X = 0 }\nreturn X"}.parseProgram();
    ASSERT_EQ(program.statements.size(), 2U);
    const auto statement =
        std::get<std::shared_ptr<ast::IfStmt>>(program.statements[0]);
    ASSERT_NE(statement, nullptr);
    EXPECT_EQ(statement->thenStatements.size(), 1U);
    EXPECT_EQ(statement->elseStatements.size(), 1U);
    const auto condition =
        std::get<std::shared_ptr<ast::BinaryExpr>>(statement->condition);
    ASSERT_NE(condition, nullptr);
    EXPECT_EQ(condition->op, ast::BinaryOperator::Greater);
}

TEST(ParserTest, ParsesNestedIf)
{
    EXPECT_NO_THROW({
        [[maybe_unused]] const ast::Program program = frontend::Parser{
            "if 1 == 1 { if 2 != 3 { X = 1 } else { X = 2 } } "
            "else { X = 3 } return X"}.parseProgram();
    });
}

TEST(ParserTest, RejectsIfWithoutElse)
{
    frontend::Parser parser{"if 1 == 1 { X = 1 }"};
    EXPECT_THROW(parseExpectingFailure(parser), std::runtime_error);
}
