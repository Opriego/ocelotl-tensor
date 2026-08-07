#include "ocelotl/frontend/Lexer.hpp"
#include "ocelotl/frontend/Token.hpp"

#include <iostream>
#include <string_view>

int main()
{
    using namespace ocelotl::frontend;

    constexpr std::string_view source = R"(
tensor A: f32[1024, 1024]
return A
)";

    Lexer lexer{source};

    for (;;) {
        const Token token = lexer.nextToken();

        std::cout
            << token.location.line
            << ':'
            << token.location.column
            << "  "
            << toString(token.kind);

        if (!token.lexeme.empty()) {
            std::cout << "  [" << token.lexeme << ']';
        }

        std::cout << '\n';

        if (token.kind == TokenKind::EndOfFile) {
            break;
        }
    }

    return 0;
}
