#include "ocelotl/codegen/llvm/LLVMCodeGenerator.hpp"

#include "ocelotl/ir/IRVerifier.hpp"
#include "ocelotl/runtime/v1/runtime.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/NoFolder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <variant>

namespace ocelotl::codegen {
namespace {

llvm::Type* lowerType(llvm::LLVMContext& context, const sema::TensorType& type)
{
    if (!type.shape.empty()) return nullptr;
    if (type.elementType == "i1") return llvm::Type::getInt1Ty(context);
    if (type.elementType == "i64") return llvm::Type::getInt64Ty(context);
    if (type.elementType == "f64") return llvm::Type::getDoubleTy(context);
    return nullptr;
}

std::uint64_t tensorStorageSize(const sema::TensorType& type)
{
    const std::uint64_t elementSize =
        type.elementType == "f32" ? 4U :
        type.elementType == "f64" || type.elementType == "i64" ? 8U : 0U;
    if (elementSize == 0 || type.shape.empty()) {
        throw std::runtime_error{"unsupported tensor storage type"};
    }

    std::uint64_t size = elementSize;
    for (const std::size_t dimension : type.shape) {
        if (dimension == 0 ||
            dimension > std::numeric_limits<std::uint64_t>::max() / size) {
            throw std::runtime_error{"tensor storage size overflows runtime ABI"};
        }
        size *= static_cast<std::uint64_t>(dimension);
    }
    return size;
}

} // namespace

LLVMCodeGenerator::LLVMCodeGenerator() = default;

std::unique_ptr<llvm::Module> LLVMCodeGenerator::generate(const ir::Module& module)
{
    ir::IRVerifier{}.verify(module);

    auto llvmModule = std::make_unique<llvm::Module>("ocelotl_module", context_);

    const sema::TensorType* returnType = nullptr;
    bool needsRuntime = false;
    std::unordered_map<ir::ValueId, const sema::TensorType*> valueTypes;
    for (const auto& block : module.blocks) {
        for (const auto& operation : block.operations) {
            valueTypes.emplace(ir::resultOf(operation), &ir::typeOf(operation));
            needsRuntime = needsRuntime ||
                std::holds_alternative<ir::TensorDeclOp>(operation);
        }
    }
    for (const auto& block : module.blocks) {
        if (const auto* returnOp = std::get_if<ir::ReturnOp>(&*block.terminator)) {
            returnType = valueTypes.at(returnOp->value);
            break;
        }
    }

    if (returnType == nullptr || returnType->elementType != "i64" ||
        !returnType->shape.empty()) {
        throw std::runtime_error{
            "LLVM executable lowering requires scalar i64 return values"};
    }

    auto* function = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context_), false),
        llvm::Function::ExternalLinkage,
        "main",
        llvmModule.get()
    );

    auto* pointerType = llvm::PointerType::getUnqual(context_);
    std::optional<llvm::FunctionCallee> allocate;
    std::optional<llvm::FunctionCallee> release;
    if (needsRuntime) {
        allocate = llvmModule->getOrInsertFunction(
            OCELOTL_RT_V1_ALLOC_NAME,
            llvm::FunctionType::get(
                pointerType,
                {llvm::Type::getInt64Ty(context_),
                 llvm::Type::getInt64Ty(context_)},
                false));
        release = llvmModule->getOrInsertFunction(
            OCELOTL_RT_V1_FREE_NAME,
            llvm::FunctionType::get(
                llvm::Type::getVoidTy(context_), {pointerType}, false));
    }

    std::unordered_map<ir::BlockId, llvm::BasicBlock*> blocks;
    for (const auto& block : module.blocks) {
        blocks.emplace(
            block.id,
            llvm::BasicBlock::Create(context_, block.name, function)
        );
    }

    llvm::IRBuilder<llvm::NoFolder> builder{context_};
    std::unordered_map<ir::ValueId, llvm::Value*> values;
    std::unordered_map<ir::ValueId, llvm::AllocaInst*> cleanupSlots;

    builder.SetInsertPoint(blocks.at(module.entry));
    for (const auto& block : module.blocks) {
        for (const auto& operation : block.operations) {
            if (const auto* tensor = std::get_if<ir::TensorDeclOp>(&operation)) {
                auto* slot = builder.CreateAlloca(pointerType, nullptr,
                                                  tensor->name + ".cleanup");
                builder.CreateStore(llvm::ConstantPointerNull::get(pointerType), slot);
                cleanupSlots.emplace(tensor->result, slot);
            }
        }
    }

    for (const auto& block : module.blocks) {
        builder.SetInsertPoint(blocks.at(block.id));

        for (const auto& operation : block.operations) {
            std::visit([&](const auto& op) {
                using Op = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<Op, ir::TensorDeclOp>) {
                    llvm::Value* storage = builder.CreateCall(
                        *allocate,
                        {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_),
                                                tensorStorageSize(op.type)),
                         llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 64)},
                        op.name + ".storage");
                    builder.CreateStore(storage, cleanupSlots.at(op.result));
                    values.emplace(op.result, storage);
                } else if constexpr (std::is_same_v<Op, ir::ConstantIntOp>) {
                    values.emplace(op.result, llvm::ConstantInt::get(
                        llvm::Type::getInt64Ty(context_),
                        static_cast<std::uint64_t>(op.value), true));
                } else if constexpr (std::is_same_v<Op, ir::ConstantFloatOp>) {
                    values.emplace(op.result, llvm::ConstantFP::get(
                        llvm::Type::getDoubleTy(context_), op.value));
                } else if constexpr (std::is_same_v<Op, ir::BinaryOp>) {
                    llvm::Value* lhs = values.at(op.lhs);
                    llvm::Value* rhs = values.at(op.rhs);
                    const bool floating = op.type.elementType == "f64";
                    llvm::Value* result = nullptr;
                    switch (op.kind) {
                    case ir::BinaryKind::Add:
                        result = floating ? builder.CreateFAdd(lhs, rhs, "add")
                                          : builder.CreateAdd(lhs, rhs, "add");
                        break;
                    case ir::BinaryKind::Subtract:
                        result = floating ? builder.CreateFSub(lhs, rhs, "sub")
                                          : builder.CreateSub(lhs, rhs, "sub");
                        break;
                    case ir::BinaryKind::Multiply:
                        result = floating ? builder.CreateFMul(lhs, rhs, "mul")
                                          : builder.CreateMul(lhs, rhs, "mul");
                        break;
                    case ir::BinaryKind::Divide:
                        result = floating ? builder.CreateFDiv(lhs, rhs, "div")
                                          : builder.CreateSDiv(lhs, rhs, "div");
                        break;
                    }
                    values.emplace(op.result, result);
                } else if constexpr (std::is_same_v<Op, ir::CompareOp>) {
                    llvm::Value* lhs = values.at(op.lhs);
                    llvm::Value* rhs = values.at(op.rhs);
                    const bool floating = op.operandType.elementType == "f64";
                    llvm::Value* result = nullptr;
                    if (floating) {
                        llvm::CmpInst::Predicate predicate;
                        switch (op.kind) {
                        case ir::CompareKind::Equal: predicate = llvm::CmpInst::FCMP_OEQ; break;
                        case ir::CompareKind::NotEqual: predicate = llvm::CmpInst::FCMP_UNE; break;
                        case ir::CompareKind::Less: predicate = llvm::CmpInst::FCMP_OLT; break;
                        case ir::CompareKind::LessEqual: predicate = llvm::CmpInst::FCMP_OLE; break;
                        case ir::CompareKind::Greater: predicate = llvm::CmpInst::FCMP_OGT; break;
                        case ir::CompareKind::GreaterEqual: predicate = llvm::CmpInst::FCMP_OGE; break;
                        }
                        result = builder.CreateFCmp(predicate, lhs, rhs, "cmp");
                    } else {
                        llvm::CmpInst::Predicate predicate;
                        switch (op.kind) {
                        case ir::CompareKind::Equal: predicate = llvm::CmpInst::ICMP_EQ; break;
                        case ir::CompareKind::NotEqual: predicate = llvm::CmpInst::ICMP_NE; break;
                        case ir::CompareKind::Less: predicate = llvm::CmpInst::ICMP_SLT; break;
                        case ir::CompareKind::LessEqual: predicate = llvm::CmpInst::ICMP_SLE; break;
                        case ir::CompareKind::Greater: predicate = llvm::CmpInst::ICMP_SGT; break;
                        case ir::CompareKind::GreaterEqual: predicate = llvm::CmpInst::ICMP_SGE; break;
                        }
                        result = builder.CreateICmp(predicate, lhs, rhs, "cmp");
                    }
                    values.emplace(op.result, result);
                } else if constexpr (std::is_same_v<Op, ir::PhiOp>) {
                    llvm::Type* type = lowerType(context_, op.type);
                    if (type == nullptr) throw std::runtime_error{"unsupported phi type"};
                    if (op.incoming.size() >
                        std::numeric_limits<unsigned>::max()) {
                        throw std::runtime_error{"too many phi operands for LLVM"};
                    }
                    auto* phi = builder.CreatePHI(
                        type, static_cast<unsigned>(op.incoming.size()), "merge");
                    values.emplace(op.result, phi);
                    for (const auto& incoming : op.incoming) {
                        phi->addIncoming(values.at(incoming.value),
                                         blocks.at(incoming.predecessor));
                    }
                } else {
                    throw std::runtime_error{"tensor LLVM lowering is not implemented yet"};
                }
            }, operation);
        }

        std::visit([&](const auto& terminator) {
            using T = std::decay_t<decltype(terminator)>;
            if constexpr (std::is_same_v<T, ir::BranchOp>) {
                builder.CreateBr(blocks.at(terminator.target));
            } else if constexpr (std::is_same_v<T, ir::CondBranchOp>) {
                builder.CreateCondBr(values.at(terminator.condition),
                                     blocks.at(terminator.trueTarget),
                                     blocks.at(terminator.falseTarget));
            } else {
                for (const auto& [value, slot] : cleanupSlots) {
                    (void)value;
                    builder.CreateCall(*release,
                                       {builder.CreateLoad(pointerType, slot)});
                }
                builder.CreateRet(builder.CreateTrunc(
                    values.at(terminator.value),
                    llvm::Type::getInt32Ty(context_),
                    "exit.status"
                ));
            }
        }, *block.terminator);
    }

    std::string verificationError;
    llvm::raw_string_ostream errorStream{verificationError};
    if (llvm::verifyModule(*llvmModule, &errorStream)) {
        errorStream.flush();
        throw std::runtime_error{"generated invalid LLVM IR: " + verificationError};
    }
    return llvmModule;
}

std::string LLVMCodeGenerator::emitToString(const llvm::Module& module) const
{
    std::string output;
    llvm::raw_string_ostream stream{output};
    module.print(stream, nullptr);
    stream.flush();
    return output;
}

} // namespace ocelotl::codegen
