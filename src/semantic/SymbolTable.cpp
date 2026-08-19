// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/semantic/SymbolTable.hpp"

#include <utility>

namespace ocelotl::sema {

bool SymbolTable::declare(Symbol symbol)
{
    const auto [iterator, inserted] =
        symbols_.emplace(symbol.name, std::move(symbol));

    return inserted;
}

const Symbol* SymbolTable::lookup(
    const std::string_view name
) const noexcept
{
    const auto iterator =
        symbols_.find(std::string{name});

    if (iterator == symbols_.end()) {
        return nullptr;
    }

    return &iterator->second;
}

bool SymbolTable::contains(
    const std::string_view name
) const noexcept
{
    return lookup(name) != nullptr;
}

std::size_t SymbolTable::size() const noexcept
{
    return symbols_.size();
}

const std::unordered_map<std::string, Symbol>&
SymbolTable::entries() const noexcept
{
    return symbols_;
}

} // namespace ocelotl::sema
