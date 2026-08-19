// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/ir/IR.hpp"

#include <type_traits>

namespace ocelotl::ir {

ValueId resultOf(const Operation& operation)
{
    return std::visit([](const auto& op) { return op.result; }, operation);
}

const sema::TensorType& typeOf(const Operation& operation)
{
    return std::visit([](const auto& op) -> const sema::TensorType& {
        using Op = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<Op, CompareOp>) {
            static const sema::TensorType booleanType{
                .elementType = "i1", .shape = {}};
            return booleanType;
        } else {
            return op.type;
        }
    }, operation);
}

} // namespace ocelotl::ir
