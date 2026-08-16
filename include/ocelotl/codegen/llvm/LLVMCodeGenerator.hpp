#pragma once

#include "ocelotl/ir/IR.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <memory>
#include <string>

namespace ocelotl::codegen {

class LLVMCodeGenerator {
public:
    LLVMCodeGenerator();

    [[nodiscard]]
    std::unique_ptr<llvm::Module>
    generate(const ir::Module& module);

    [[nodiscard]]
    std::string emitToString(
        const llvm::Module& module
    ) const;

private:
    llvm::LLVMContext context_;
};

} // namespace ocelotl::codegen
