#include "ocelotl/frontend/Token.hpp"

#include <iostream>

int main()
{
    using namespace ocelotl::frontend;

    Token token{
        TokenKind::KwTensor,
        "tensor",
        {1, 1, 0}
    };

    std::cout
        << "Token: "
        << toString(token.kind)
        << " ["
        << token.lexeme
        << "] at "
        << token.location.line
        << ":"
        << token.location.column
        << '\n';

    return 0;
}
