#include "ocelotl/codegen/llvm/LLVMCodeGenerator.hpp"
#include "ocelotl/codegen/llvm/LLVMOptimizer.hpp"
#include "ocelotl/codegen/llvm/TargetCodeGenerator.hpp"
#include "ocelotl/frontend/Lexer.hpp"
#include "ocelotl/frontend/Parser.hpp"
#include "ocelotl/ir/IRGenerator.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

enum class EmitMode { None, Tokens, LLVMBeforeOptimization, LLVM, Assembly, Object };

struct CommandLineOptions {
  std::string sourcePath;
  std::string outputPath;
  EmitMode emitMode{EmitMode::None};
  ocelotl::codegen::OptimizationLevel optimizationLevel{
      ocelotl::codegen::OptimizationLevel::O0};
  bool optimizationLevelSpecified{false};
  ocelotl::codegen::TargetConfiguration target;
};

[[nodiscard]] std::string readFile(const std::string &path) {
  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error{"could not open input file '" + path + "'"};
  }
  return {std::istreambuf_iterator<char>{input},
          std::istreambuf_iterator<char>{}};
}

void printUsage(const char *executable) {
  std::cerr << "Usage: " << executable
            << " <source-file> <emit-option> [options]\n"
            << "\nEmit options:\n"
            << "  --emit-tokens       Print lexical tokens\n"
            << "  --emit-llvm-before-opt\n"
            << "                      Emit LLVM IR before optimization\n"
            << "  --emit-llvm         Emit LLVM IR after optimization\n"
            << "  --emit-asm          Emit target assembly\n"
            << "  --emit-obj          Emit a relocatable object file\n"
            << "\nOptions:\n"
            << "  -o <path>           Write output to path\n"
            << "  -O0|-O1|-O2|-O3    Select LLVM optimization level (default: -O0)\n"
            << "  --target=<triple>   Select target triple (default: host)\n"
            << "  --cpu=<cpu>         Select target CPU\n"
            << "  --features=<list>   Select target features\n";
}

void setEmitMode(CommandLineOptions &options, const EmitMode mode) {
  if (options.emitMode != EmitMode::None) {
    throw std::runtime_error{"multiple emit options were specified"};
  }
  options.emitMode = mode;
}

[[nodiscard]] CommandLineOptions parseCommandLine(const int argc, char **argv) {
  CommandLineOptions options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--emit-tokens")
      setEmitMode(options, EmitMode::Tokens);
    else if (argument == "--emit-llvm-before-opt")
      setEmitMode(options, EmitMode::LLVMBeforeOptimization);
    else if (argument == "--emit-llvm")
      setEmitMode(options, EmitMode::LLVM);
    else if (argument == "--emit-asm")
      setEmitMode(options, EmitMode::Assembly);
    else if (argument == "--emit-obj")
      setEmitMode(options, EmitMode::Object);
    else if (argument == "-O0" || argument == "-O1" ||
             argument == "-O2" || argument == "-O3") {
      if (options.optimizationLevelSpecified)
        throw std::runtime_error{"multiple optimization levels were specified"};
      options.optimizationLevelSpecified = true;
      if (argument == "-O0")
        options.optimizationLevel = ocelotl::codegen::OptimizationLevel::O0;
      else if (argument == "-O1")
        options.optimizationLevel = ocelotl::codegen::OptimizationLevel::O1;
      else if (argument == "-O2")
        options.optimizationLevel = ocelotl::codegen::OptimizationLevel::O2;
      else
        options.optimizationLevel = ocelotl::codegen::OptimizationLevel::O3;
    }
    else if (argument == "-o") {
      if (++index >= argc)
        throw std::runtime_error{"-o requires an output path"};
      options.outputPath = argv[index];
    } else if (argument.starts_with("--target="))
      options.target.triple = argument.substr(9);
    else if (argument.starts_with("--cpu="))
      options.target.cpu = argument.substr(6);
    else if (argument.starts_with("--features="))
      options.target.features = argument.substr(11);
    else if (argument.starts_with('-'))
      throw std::runtime_error{"unknown option '" + std::string{argument} +
                               "'"};
    else if (options.sourcePath.empty())
      options.sourcePath = argument;
    else
      throw std::runtime_error{"multiple input files were specified"};
  }

  if (options.sourcePath.empty())
    throw std::runtime_error{"no input file was specified"};
  if (options.emitMode == EmitMode::None)
    throw std::runtime_error{"no emit option was specified"};
  if (options.emitMode == EmitMode::Tokens &&
      (!options.target.triple.empty() || !options.target.cpu.empty() ||
       !options.target.features.empty() || options.optimizationLevelSpecified)) {
    throw std::runtime_error{
        "target and optimization options do not apply to token emission"};
  }
  return options;
}

[[nodiscard]] std::string defaultOutputPath(const CommandLineOptions &options) {
  if (!options.outputPath.empty())
    return options.outputPath;
  if (options.emitMode == EmitMode::LLVM ||
      options.emitMode == EmitMode::LLVMBeforeOptimization ||
      options.emitMode == EmitMode::Tokens)
    return "-";

  std::filesystem::path path{options.sourcePath};
  path.replace_extension(options.emitMode == EmitMode::Assembly ? ".s" : ".o");
  return path.string();
}

void emitTokens(std::string_view source, llvm::raw_ostream &output) {
  ocelotl::frontend::Lexer lexer{source};
  while (true) {
    const auto token = lexer.nextToken();
    output << token.location.line << ':' << token.location.column << "  "
           << ocelotl::frontend::toString(token.kind);
    if (!token.lexeme.empty())
      output << "  [" << token.lexeme << ']';
    output << '\n';
    if (token.kind == ocelotl::frontend::TokenKind::EndOfFile)
      break;
  }
}

[[nodiscard]] std::unique_ptr<llvm::Module>
compileToLLVM(std::string_view source,
              ocelotl::codegen::LLVMCodeGenerator &llvmGenerator) {
  ocelotl::frontend::Parser parser{source};
  const ocelotl::ast::Program program = parser.parseProgram();
  ocelotl::sema::SemanticAnalyzer semanticAnalyzer;
  semanticAnalyzer.analyze(program);
  ocelotl::ir::IRGenerator irGenerator{semanticAnalyzer};
  return llvmGenerator.generate(irGenerator.generate(program));
}

void writeTextOutput(const std::string &path, const std::string &value) {
  if (path == "-") {
    llvm::outs() << value;
    return;
  }
  std::error_code error;
  llvm::raw_fd_ostream output{path, error, llvm::sys::fs::OF_Text};
  if (error)
    throw std::runtime_error{"could not open output file '" + path +
                             "': " + error.message()};
  output << value;
  output.flush();
  if (output.has_error())
    throw std::runtime_error{"failed while writing output file '" + path + "'"};
}

} // namespace

int main(int argc, char **argv) {
  try {
    const CommandLineOptions options = parseCommandLine(argc, argv);
    const std::string source = readFile(options.sourcePath);
    const std::string outputPath = defaultOutputPath(options);

    if (options.emitMode == EmitMode::Tokens) {
      if (outputPath == "-") {
        emitTokens(source, llvm::outs());
      } else {
        std::error_code error;
        llvm::raw_fd_ostream output{outputPath, error, llvm::sys::fs::OF_Text};
        if (error)
          throw std::runtime_error{"could not open output file '" + outputPath +
                                   "': " + error.message()};
        emitTokens(source, output);
      }
      return 0;
    }

    ocelotl::codegen::LLVMCodeGenerator llvmGenerator;
    auto module = compileToLLVM(source, llvmGenerator);

    if (options.emitMode != EmitMode::LLVMBeforeOptimization) {
      ocelotl::codegen::LLVMOptimizer{}.optimize(
          *module, options.optimizationLevel);
    }

    if (options.emitMode == EmitMode::LLVM ||
        options.emitMode == EmitMode::LLVMBeforeOptimization) {
      if (!options.target.triple.empty() || !options.target.cpu.empty() ||
          !options.target.features.empty()) {
        ocelotl::codegen::TargetCodeGenerator targetGenerator{options.target};
        targetGenerator.configureModule(*module);
        targetGenerator.verifyModule(*module);
      }
      writeTextOutput(outputPath, llvmGenerator.emitToString(*module));
      return 0;
    }

    ocelotl::codegen::TargetCodeGenerator targetGenerator{options.target};
    targetGenerator.emit(*module,
                         options.emitMode == EmitMode::Assembly
                             ? ocelotl::codegen::NativeOutputKind::Assembly
                             : ocelotl::codegen::NativeOutputKind::Object,
                         outputPath);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    printUsage(argv[0]);
    return 1;
  }
}
