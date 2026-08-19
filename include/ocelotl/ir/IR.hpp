// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "ocelotl/semantic/Symbol.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ocelotl::ir {

using ValueId = std::size_t;
using BlockId = std::size_t;

enum class BinaryKind { Add, Subtract, Multiply, Divide };
enum class CompareKind { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

struct TensorDeclOp {
    ValueId result;
    std::string name;
    sema::TensorType type;
};

struct ConstantIntOp {
    ValueId result;
    std::int64_t value;
    sema::TensorType type;
};

struct ConstantFloatOp {
    ValueId result;
    double value;
    sema::TensorType type;
};

struct BinaryOp {
    ValueId result;
    BinaryKind kind;
    ValueId lhs;
    ValueId rhs;
    sema::TensorType type;
};

struct CompareOp {
    ValueId result;
    CompareKind kind;
    ValueId lhs;
    ValueId rhs;
    sema::TensorType operandType;
};

struct PhiIncoming {
    BlockId predecessor;
    ValueId value;
};

struct PhiOp {
    ValueId result;
    sema::TensorType type;
    std::vector<PhiIncoming> incoming;
};

struct MatMulOp {
    ValueId result;
    ValueId lhs;
    ValueId rhs;
    sema::TensorType type;
};

struct ReluOp {
    ValueId result;
    ValueId input;
    sema::TensorType type;
};

using Operation = std::variant<TensorDeclOp, ConstantIntOp, ConstantFloatOp,
                               BinaryOp, CompareOp, PhiOp, MatMulOp, ReluOp>;

struct BranchOp { BlockId target; };

struct CondBranchOp {
    ValueId condition;
    BlockId trueTarget;
    BlockId falseTarget;
};

struct ReturnOp { ValueId value; };

using Terminator = std::variant<BranchOp, CondBranchOp, ReturnOp>;

struct BasicBlock {
    BlockId id;
    std::string name;
    std::vector<Operation> operations;
    std::optional<Terminator> terminator;
};

struct Module {
    BlockId entry{0};
    std::vector<BasicBlock> blocks;
};

[[nodiscard]] ValueId resultOf(const Operation& operation);
[[nodiscard]] const sema::TensorType& typeOf(const Operation& operation);

} // namespace ocelotl::ir
