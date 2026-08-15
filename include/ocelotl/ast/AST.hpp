#pragma once

#include "ocelotl/frontend/Token.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace ocelotl::ast {

using frontend::SourceLocation;

struct TensorType {
    std::string elementType;
    std::vector<std::size_t> shape;
};

struct IdentifierExpr {
    std::string name;
    SourceLocation location;
};

struct IntegerExpr {
    std::int64_t value;
    SourceLocation location;
};

struct FloatExpr {
    double value;
    SourceLocation location;
};

struct CallExpr;

using Expression = std::variant<
    IdentifierExpr,
    IntegerExpr,
    FloatExpr,
    std::shared_ptr<CallExpr>
>;

struct CallExpr {
    std::string callee;
    std::vector<Expression> arguments;
    SourceLocation location;
};

struct TensorDecl {
    std::string name;
    TensorType type;
    SourceLocation location;
};

struct Assignment {
    std::string target;
    Expression value;
    SourceLocation location;
};

struct ReturnStmt {
    Expression value;
    SourceLocation location;
};

using Statement = std::variant<
    TensorDecl,
    Assignment,
    ReturnStmt
>;

struct Program {
    std::vector<Statement> statements;
};

} // namespace ocelotl::ast
