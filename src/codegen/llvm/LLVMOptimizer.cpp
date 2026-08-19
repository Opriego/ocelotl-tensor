#include "ocelotl/codegen/llvm/LLVMOptimizer.hpp"

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>

#include <stdexcept>
#include <string>

namespace ocelotl::codegen {
namespace {

llvm::OptimizationLevel toLLVMLevel(const OptimizationLevel level)
{
    switch (level) {
    case OptimizationLevel::O0: return llvm::OptimizationLevel::O0;
    case OptimizationLevel::O1: return llvm::OptimizationLevel::O1;
    case OptimizationLevel::O2: return llvm::OptimizationLevel::O2;
    case OptimizationLevel::O3: return llvm::OptimizationLevel::O3;
    }
    throw std::runtime_error{"unknown LLVM optimization level"};
}

void verify(const llvm::Module& module, const char* phase)
{
    std::string diagnostic;
    llvm::raw_string_ostream stream{diagnostic};
    if (llvm::verifyModule(module, &stream)) {
        stream.flush();
        throw std::runtime_error{
            std::string{"invalid LLVM module "} + phase +
            " optimization: " + diagnostic
        };
    }
}

} // namespace

void LLVMOptimizer::optimize(
    llvm::Module& module,
    const OptimizationLevel level
) const
{
    verify(module, "before");

    // Ocelotl defines O0 as preserving the generated LLVM IR without running
    // transformation passes. Verify the module, then leave it unchanged.
    if (level == OptimizationLevel::O0) {
        return;
    }

    llvm::LoopAnalysisManager loopAnalyses;
    llvm::FunctionAnalysisManager functionAnalyses;
    llvm::CGSCCAnalysisManager cgsccAnalyses;
    llvm::ModuleAnalysisManager moduleAnalyses;

    llvm::PassBuilder passBuilder;
    passBuilder.registerLoopAnalyses(loopAnalyses);
    passBuilder.registerFunctionAnalyses(functionAnalyses);
    passBuilder.registerCGSCCAnalyses(cgsccAnalyses);
    passBuilder.registerModuleAnalyses(moduleAnalyses);
    passBuilder.crossRegisterProxies(
        loopAnalyses,
        functionAnalyses,
        cgsccAnalyses,
        moduleAnalyses
    );

    llvm::ModulePassManager pipeline =
        passBuilder.buildPerModuleDefaultPipeline(toLLVMLevel(level));
    pipeline.run(module, moduleAnalyses);

    verify(module, "after");
}

} // namespace ocelotl::codegen
