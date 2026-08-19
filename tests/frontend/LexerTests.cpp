#include "ocelotl/frontend/Lexer.hpp"
#include "ocelotl/frontend/Token.hpp"

#include <gtest/gtest.h>

#include <string_view>

using namespace ocelotl::frontend;

TEST(LexerTest, TokenizesTensorDeclaration)
{
    constexpr std::string_view source =
        "tensor A: f32[1024, 1024]";

    Lexer lexer{source};

    Token token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::KwTensor);
    EXPECT_EQ(token.lexeme, "tensor");
    EXPECT_EQ(token.location.line, 1);
    EXPECT_EQ(token.location.column, 1);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "A");
    EXPECT_EQ(token.location.column, 8);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Colon);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "f32");

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::LeftBracket);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(token.lexeme, "1024");

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Comma);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(token.lexeme, "1024");

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::RightBracket);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::EndOfFile);
}

TEST(LexerTest, TokenizesIdentifiersAndKeywords)
{
    constexpr std::string_view source =
        "tensor return foo foo_bar";

    Lexer lexer{source};

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::KwTensor);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::KwReturn);

    Token token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "foo");

    token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "foo_bar");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, TokenizesIntegerAndFloatLiterals)
{
    constexpr std::string_view source =
        "12 42 3.14 0.5";

    Lexer lexer{source};

    Token token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(token.lexeme, "12");

    token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(token.lexeme, "42");

    token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
    EXPECT_EQ(token.lexeme, "3.14");

    token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
    EXPECT_EQ(token.lexeme, "0.5");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, TokenizesPunctuation)
{
    constexpr std::string_view source =
        ": , = [ ] ( )";

    Lexer lexer{source};

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Colon);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Comma);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Equal);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::LeftBracket);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::RightBracket);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::LeftParen);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::RightParen);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, ProducesUnknownTokenForInvalidCharacter)
{
    constexpr std::string_view source = "@";

    Lexer lexer{source};

    Token token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Unknown);
    EXPECT_EQ(token.lexeme, "@");
    EXPECT_EQ(token.location.line, 1);
    EXPECT_EQ(token.location.column, 1);

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, TracksSourceLocationsAcrossLines)
{
    constexpr std::string_view source =
        "tensor A\n"
        "return A\n";

    Lexer lexer{source};

    Token token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::KwTensor);
    EXPECT_EQ(token.location.line, 1);
    EXPECT_EQ(token.location.column, 1);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "A");
    EXPECT_EQ(token.location.line, 1);
    EXPECT_EQ(token.location.column, 8);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::KwReturn);
    EXPECT_EQ(token.location.line, 2);
    EXPECT_EQ(token.location.column, 1);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "A");
    EXPECT_EQ(token.location.line, 2);
    EXPECT_EQ(token.location.column, 8);

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::EndOfFile);
    EXPECT_EQ(token.location.line, 3);
    EXPECT_EQ(token.location.column, 1);
}

TEST(LexerTest, TokenizesIdentifiersWithUnderscoresAndDigits)
{
    constexpr std::string_view source =
        "_tensor foo123 foo_bar_42";

    Lexer lexer{source};

    Token token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "_tensor");

    token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "foo123");

    token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "foo_bar_42");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, TreatsTrailingDotAfterIntegerAsSeparateUnknownToken)
{
    constexpr std::string_view source = "123.";

    Lexer lexer{source};

    Token token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(token.lexeme, "123");

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Unknown);
    EXPECT_EQ(token.lexeme, ".");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, TokenizesMultipleDotsWithoutSilentlyAcceptingInvalidFloat)
{
    constexpr std::string_view source = "123.45.67";

    Lexer lexer{source};

    Token token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
    EXPECT_EQ(token.lexeme, "123.45");

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Unknown);
    EXPECT_EQ(token.lexeme, ".");

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(token.lexeme, "67");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, SkipsWhitespace)
{
    constexpr std::string_view source =
        " \t\r\n"
        "tensor\tA\r\n"
        "return   A";

    Lexer lexer{source};

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::KwTensor);

    Token token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "A");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::KwReturn);

    token = lexer.nextToken();
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "A");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, ProducesEofForEmptyInput)
{
    constexpr std::string_view source = "";

    Lexer lexer{source};

    const Token token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::EndOfFile);
    EXPECT_TRUE(token.lexeme.empty());
    EXPECT_EQ(token.location.line, 1);
    EXPECT_EQ(token.location.column, 1);
}

TEST(LexerTest, RepeatedEofIsStable)
{
    constexpr std::string_view source = "";

    Lexer lexer{source};

    const Token first = lexer.nextToken();
    const Token second = lexer.nextToken();

    EXPECT_EQ(first.kind, TokenKind::EndOfFile);
    EXPECT_EQ(second.kind, TokenKind::EndOfFile);

    EXPECT_EQ(first.location.line, second.location.line);
    EXPECT_EQ(first.location.column, second.location.column);
    EXPECT_EQ(first.location.offset, second.location.offset);
}

TEST(LexerTest, DoesNotMatchKeywordPrefixes)
{
    constexpr std::string_view source =
        "tensorValue returnValue";

    Lexer lexer{source};

    Token token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "tensorValue");

    token = lexer.nextToken();

    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "returnValue");

    EXPECT_EQ(lexer.nextToken().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, TokenizesControlFlowAndScalarOperators)
{
    Lexer lexer{"if A >= 1 { B = A + 2 * 3 } else { B = A != 0 }"};
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::KwIf);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Identifier);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::GreaterEqual);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::LeftBrace);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Identifier);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Equal);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Identifier);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Plus);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Star);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::IntegerLiteral);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::RightBrace);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::KwElse);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::LeftBrace);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Identifier);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Equal);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::Identifier);
    EXPECT_EQ(lexer.nextToken().kind, TokenKind::BangEqual);
}

TEST(LexerTest, TracksByteOffsets)
{
    constexpr std::string_view source =
        "tensor A";

    Lexer lexer{source};

    Token token = lexer.nextToken();

    EXPECT_EQ(token.location.offset, 0);

    token = lexer.nextToken();

    EXPECT_EQ(token.location.offset, 7);

    token = lexer.nextToken();

    EXPECT_EQ(token.location.offset, 8);
}
