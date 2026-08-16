#include "ocelotl/codegen/llvm/LLVMCodeGenerator.hpp"
#include "ocelotl/frontend/Lexer.hpp"
#include "ocelotl/frontend/Parser.hpp"
#include "ocelotl/ir/IRGenerator.hpp"
#include "ocelotl/semantic/SemanticAnalyzer.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[nodiscard]]
std::string readFile(const std::string& path)
{
    std::ifstream input{path};

    if (!input) {
        throw std::runtime_error{
            "could not open input file '" + path + "'"
        };
    }

    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
}

void printUsage(const char* executable)
{
    std::cerr
        << "Usage:\n"
        << "  " << executable
        << " <source-file> --emit-tokens\n"
        << "  " << executable
        << " <source-file> --emit-llvm\n";
}

void emitTokens(std::string_view source)
{
    ocelotl::frontend::Lexer lexer{source};

    while (true) {
        const auto token = lexer.nextToken();

        std::cout
            << token.location.line
            << ':'
            << token.location.column
            << "  "
            << ocelotl::frontend::toString(token.kind);

        if (!token.lexeme.empty()) {
            std::cout
                << "  ["
                << token.lexeme
                << ']';
        }

        std::cout << '\n';

        if (
            token.kind ==
            ocelotl::frontend::TokenKind::EndOfFile
        ) {
            break;
        }
    }
}

void emitLLVM(std::string_view source)
{
    ocelotl::frontend::Parser parser{source};

    const ocelotl::ast::Program program =
        parser.parseProgram();

    ocelotl::sema::SemanticAnalyzer semanticAnalyzer;
    semanticAnalyzer.analyze(program);

    ocelotl::ir::IRGenerator irGenerator{
        semanticAnalyzer
    };

    const ocelotl::ir::Module irModule =
        irGenerator.generate(program);

    ocelotl::codegen::LLVMCodeGenerator llvmGenerator;

    const auto llvmModule =
        llvmGenerator.generate(irModule);

    std::cout
        << llvmGenerator.emitToString(
            *llvmModule
        );
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string sourcePath =
        argv[1];

    const std::string_view option =
        argv[2];

    try {
        const std::string source =
            readFile(sourcePath);

        if (option == "--emit-tokens") {
            emitTokens(source);
            return 0;
        }

        if (option == "--emit-llvm") {
            emitLLVM(source);
            return 0;
        }

        std::cerr
            << "error: unknown option '"
            << option
            << "'\n";

        printUsage(argv[0]);

        return 1;
    }
    catch (const std::exception& error) {
        std::cerr
            << "error: "
            << error.what()
            << '\n';

        return 1;
    }
}
