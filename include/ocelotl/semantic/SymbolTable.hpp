// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "ocelotl/semantic/Symbol.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace ocelotl::sema {

class SymbolTable {
public:
    [[nodiscard]]
    bool declare(Symbol symbol);

    [[nodiscard]]
    const Symbol* lookup(std::string_view name) const noexcept;

    [[nodiscard]]
    bool contains(std::string_view name) const noexcept;

    [[nodiscard]]
    std::size_t size() const noexcept;

    [[nodiscard]]
    const std::unordered_map<std::string, Symbol>& entries() const noexcept;

private:
    std::unordered_map<std::string, Symbol> symbols_;
};

} // namespace ocelotl::sema
