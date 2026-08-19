// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

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
struct BinaryExpr;

enum class BinaryOperator {
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

using Expression = std::variant<
    IdentifierExpr,
    IntegerExpr,
    FloatExpr,
    std::shared_ptr<CallExpr>,
    std::shared_ptr<BinaryExpr>
>;

struct CallExpr {
    std::string callee;
    std::vector<Expression> arguments;
    SourceLocation location;
};

struct BinaryExpr {
    BinaryOperator op;
    Expression lhs;
    Expression rhs;
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

struct IfStmt;

using Statement = std::variant<
    TensorDecl,
    Assignment,
    ReturnStmt,
    std::shared_ptr<IfStmt>
>;

struct IfStmt {
    Expression condition;
    std::vector<Statement> thenStatements;
    std::vector<Statement> elseStatements;
    SourceLocation location;
};

struct Program {
    std::vector<Statement> statements;
};

} // namespace ocelotl::ast
