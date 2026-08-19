// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "ocelotl/ast/AST.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ocelotl::sema {

struct TensorType {
    std::string elementType;
    std::vector<std::size_t> shape;
};

struct Symbol {
    std::string name;
    TensorType type;
    ast::SourceLocation location;
};
} // namespace ocelotl::sema
