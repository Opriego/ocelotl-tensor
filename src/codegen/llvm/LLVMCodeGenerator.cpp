#include "ocelotl/codegen/llvm/LLVMCodeGenerator.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <variant>

namespace ocelotl::codegen {

LLVMCodeGenerator::LLVMCodeGenerator() = default;

std::unique_ptr<llvm::Module>
LLVMCodeGenerator::generate(
    const ir::Module& module
)
{
    auto llvmModule =
        std::make_unique<llvm::Module>(
            "ocelotl_module",
            context_
        );

    llvm::IRBuilder<> builder{context_};

    /*
     * Current milestone:
     *
     * Every Ocelotl source program is lowered into a single
     * LLVM function named "main".
     *
     * We determine its return type from the ReturnOp operand.
     */

    const ir::ReturnOp* returnOperation = nullptr;

    for (const auto& operation : module.operations) {
        if (const auto* returnOp =
                std::get_if<ir::ReturnOp>(&operation)) {

            returnOperation = returnOp;
        }
    }

    if (returnOperation == nullptr) {
        throw std::runtime_error{
            "LLVM code generation requires a return operation"
        };
    }

    /*
     * First determine the LLVM type of the returned IR value.
     */
    llvm::Type* returnType = nullptr;

    for (const auto& operation : module.operations) {
        if (const auto* integer =
                std::get_if<ir::ConstantIntOp>(
                    &operation
                )) {

            if (integer->result ==
                returnOperation->value) {

                returnType =
                    llvm::Type::getInt64Ty(
                        context_
                    );

                break;
            }
        }

        if (const auto* floatingPoint =
                std::get_if<ir::ConstantFloatOp>(
                    &operation
                )) {

            if (floatingPoint->result ==
                returnOperation->value) {

                returnType =
                    llvm::Type::getDoubleTy(
                        context_
                    );

                break;
            }
        }
    }

    if (returnType == nullptr) {
        throw std::runtime_error{
            "LLVM code generation does not yet support "
            "the returned Ocelotl IR value type"
        };
    }

    auto* functionType =
        llvm::FunctionType::get(
            returnType,
            false
        );

    auto* function =
        llvm::Function::Create(
            functionType,
            llvm::Function::ExternalLinkage,
            "main",
            llvmModule.get()
        );

    auto* entry =
        llvm::BasicBlock::Create(
            context_,
            "entry",
            function
        );

    builder.SetInsertPoint(entry);

    /*
     * Map Ocelotl SSA-like ValueIds to LLVM Values.
     */
    std::unordered_map<
        ir::ValueId,
        llvm::Value*
    > values;

    for (const auto& operation : module.operations) {

        if (const auto* integer =
                std::get_if<ir::ConstantIntOp>(
                    &operation
                )) {

            llvm::Value* value =
                llvm::ConstantInt::get(
                    llvm::Type::getInt64Ty(
                        context_
                    ),
                    static_cast<std::uint64_t>(
                        integer->value
                    ),
                    true
                );

            values.emplace(
                integer->result,
                value
            );

            continue;
        }

        if (const auto* floatingPoint =
                std::get_if<ir::ConstantFloatOp>(
                    &operation
                )) {

            llvm::Value* value =
                llvm::ConstantFP::get(
                    llvm::Type::getDoubleTy(
                        context_
                    ),
                    floatingPoint->value
                );

            values.emplace(
                floatingPoint->result,
                value
            );

            continue;
        }

        if (const auto* returnOp =
                std::get_if<ir::ReturnOp>(
                    &operation
                )) {

            const auto iterator =
                values.find(
                    returnOp->value
                );

            if (iterator == values.end()) {
                throw std::runtime_error{
                    "LLVM code generation could not resolve "
                    "the return value"
                };
            }

            builder.CreateRet(
                iterator->second
            );

            continue;
        }

        /*
         * Tensor operations intentionally remain unsupported in
         * this first backend milestone.
         */
        if (
            std::holds_alternative<
                ir::TensorDeclOp
            >(operation) ||
            std::holds_alternative<
                ir::MatMulOp
            >(operation) ||
            std::holds_alternative<
                ir::ReluOp
            >(operation)
        ) {
            throw std::runtime_error{
                "tensor LLVM lowering is not implemented yet"
            };
        }
    }

    /*
     * LLVM itself verifies structural correctness of the generated
     * module before we return it.
     */
    std::string verificationError;
    llvm::raw_string_ostream errorStream{
        verificationError
    };

    if (llvm::verifyModule(
            *llvmModule,
            &errorStream
        )) {

        errorStream.flush();

        throw std::runtime_error{
            "generated invalid LLVM IR: "
            + verificationError
        };
    }

    return llvmModule;
}

std::string
LLVMCodeGenerator::emitToString(
    const llvm::Module& module
) const
{
    std::string output;

    llvm::raw_string_ostream stream{
        output
    };

    module.print(
        stream,
        nullptr
    );

    stream.flush();

    return output;
}

} // namespace ocelotl::codegen
