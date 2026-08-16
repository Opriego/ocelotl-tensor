#pragma once

#include "ocelotl/semantic/Symbol.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace ocelotl::ir {

using ValueId = std::size_t;

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

struct ReturnOp {
    ValueId value;
};

using Operation = std::variant<
    TensorDeclOp,
    ConstantIntOp,
    ConstantFloatOp,
    MatMulOp,
    ReluOp,
    ReturnOp
>;

struct Module {
    std::vector<Operation> operations;
};

} // namespace ocelotl::ir
