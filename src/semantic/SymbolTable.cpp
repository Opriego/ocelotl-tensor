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

} // namespace ocelotl::sema
