// Copyright (C) 2026 Oscar Priego Verdugo
// SPDX-License-Identifier: GPL-3.0-only

#include "ocelotl/codegen/llvm/TargetCodeGenerator.hpp"

#include <llvm/Config/llvm-config.h>
#if LLVM_VERSION_MAJOR >= 16
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#endif
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace ocelotl::codegen {
namespace {

void initializeTargets() {
  static std::once_flag once;

  std::call_once(once, [] {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
  });
}

} // namespace

TargetCodeGenerator::TargetCodeGenerator(TargetConfiguration configuration)
    : configuration_{std::move(configuration)} {
  initializeTargets();

  if (configuration_.triple.empty()) {
    configuration_.triple = llvm::sys::getDefaultTargetTriple();
  } else {
    configuration_.triple = llvm::Triple::normalize(configuration_.triple);
  }

  std::string lookupError;
  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(configuration_.triple, lookupError);

  if (target == nullptr) {
    throw std::runtime_error{"unable to select target '" +
                             configuration_.triple + "': " + lookupError};
  }

  llvm::TargetOptions options;
  targetMachine_.reset(target->createTargetMachine(
      configuration_.triple, configuration_.cpu, configuration_.features,
      options,
#if LLVM_VERSION_MAJOR >= 16
      std::nullopt
#else
      llvm::None
#endif
      ));

  if (targetMachine_ == nullptr) {
    throw std::runtime_error{"unable to create target machine for '" +
                             configuration_.triple + "'"};
  }
}

TargetCodeGenerator::~TargetCodeGenerator() = default;
TargetCodeGenerator::TargetCodeGenerator(TargetCodeGenerator &&) noexcept =
    default;
TargetCodeGenerator &
TargetCodeGenerator::operator=(TargetCodeGenerator &&) noexcept = default;

const TargetConfiguration &TargetCodeGenerator::configuration() const noexcept {
  return configuration_;
}

void TargetCodeGenerator::configureModule(llvm::Module &module) const {
  module.setTargetTriple(configuration_.triple);
  module.setDataLayout(targetMachine_->createDataLayout());
}

void TargetCodeGenerator::verifyModule(const llvm::Module &module) const {
  std::string diagnostic;
  llvm::raw_string_ostream stream{diagnostic};

  if (llvm::verifyModule(module, &stream)) {
    stream.flush();
    throw std::runtime_error{"cannot emit invalid LLVM module: " + diagnostic};
  }
}

void TargetCodeGenerator::emit(llvm::Module &module,
                               const NativeOutputKind outputKind,
                               const std::string &outputPath) const {
  configureModule(module);
  verifyModule(module);

  std::error_code error;
  llvm::raw_fd_ostream output{outputPath, error, llvm::sys::fs::OF_None};

  if (error) {
    throw std::runtime_error{"could not open output file '" + outputPath +
                             "': " + error.message()};
  }

  llvm::legacy::PassManager passManager;
  const llvm::CodeGenFileType fileType =
#if LLVM_VERSION_MAJOR >= 18
      outputKind == NativeOutputKind::Assembly
          ? llvm::CodeGenFileType::AssemblyFile
          : llvm::CodeGenFileType::ObjectFile;
#else
      outputKind == NativeOutputKind::Assembly
          ? llvm::CGFT_AssemblyFile
          : llvm::CGFT_ObjectFile;
#endif

  if (targetMachine_->addPassesToEmitFile(passManager, output, nullptr,
                                          fileType, false)) {
    throw std::runtime_error{"target '" + configuration_.triple +
                             "' cannot emit the requested file type"};
  }

  passManager.run(module);
  output.flush();

  if (output.has_error()) {
    throw std::runtime_error{"failed while writing output file '" + outputPath +
                             "'"};
  }
}

} // namespace ocelotl::codegen
