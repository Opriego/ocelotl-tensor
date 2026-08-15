class Parser {
public:
    explicit Parser(std::string_view source);

    ast::Program parseProgram();

private:
    ast::Statement parseStatement();

    ast::TensorDecl parseTensorDeclaration();
    ast::Assignment parseAssignment();
    ast::ReturnStmt parseReturnStatement();

    ast::TensorType parseTensorType();

    ast::Expression parseExpression();
    ast::Expression parseCallExpression();

    Token consume(TokenKind expected);
    bool match(TokenKind kind);

    Lexer lexer_;
    Token current_;
};
