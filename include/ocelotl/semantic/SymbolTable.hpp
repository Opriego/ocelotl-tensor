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

private:
    std::unordered_map<std::string, Symbol> symbols_;
};

} // namespace ocelotl::sema
