#pragma once

#include "ocelotl/ir/IR.hpp"

#include <stdexcept>
#include <string>

namespace ocelotl::ir {

class IRVerificationError : public std::runtime_error {
public:
    explicit IRVerificationError(const std::string& message)
        : std::runtime_error{message} {}
};

class IRVerifier {
public:
    void verify(const Module& module) const;
};

} // namespace ocelotl::ir
