#pragma once

#include <llvm/Support/CodeGen.h>

#include <memory>
#include <string>

namespace llvm {
class Module;
class TargetMachine;
} // namespace llvm

namespace ocelotl::codegen {

struct TargetConfiguration {
  std::string triple;
  std::string cpu;
  std::string features;
};

enum class NativeOutputKind { Assembly, Object };

class TargetCodeGenerator {
public:
  explicit TargetCodeGenerator(TargetConfiguration configuration = {});
  ~TargetCodeGenerator();

  TargetCodeGenerator(const TargetCodeGenerator &) = delete;
  TargetCodeGenerator &operator=(const TargetCodeGenerator &) = delete;
  TargetCodeGenerator(TargetCodeGenerator &&) noexcept;
  TargetCodeGenerator &operator=(TargetCodeGenerator &&) noexcept;

  [[nodiscard]]
  const TargetConfiguration &configuration() const noexcept;

  void configureModule(llvm::Module &module) const;
  void verifyModule(const llvm::Module &module) const;

  void emit(llvm::Module &module, NativeOutputKind outputKind,
            const std::string &outputPath) const;

private:
  TargetConfiguration configuration_;
  std::unique_ptr<llvm::TargetMachine> targetMachine_;
};

} // namespace ocelotl::codegen
