// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

namespace llvm {
class Module;
} // namespace llvm

namespace ocelotl::codegen {

enum class OptimizationLevel {
    O0,
    O1,
    O2,
    O3
};

class LLVMOptimizer {
public:
    void optimize(llvm::Module& module, OptimizationLevel level) const;
};

} // namespace ocelotl::codegen
