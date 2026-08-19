#include "ocelotl/ir/IRVerifier.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace ocelotl::ir {
namespace {

bool sameType(const sema::TensorType& lhs, const sema::TensorType& rhs)
{
    return lhs.elementType == rhs.elementType && lhs.shape == rhs.shape;
}

struct Definition {
    BlockId block;
    std::size_t operationIndex;
    sema::TensorType type;
};

[[noreturn]] void fail(const std::string& message)
{
    throw IRVerificationError{"IR verification failed: " + message};
}

} // namespace

void IRVerifier::verify(const Module& module) const
{
    if (module.blocks.empty()) {
        fail("module has no basic blocks");
    }

    std::unordered_map<BlockId, const BasicBlock*> blocks;
    std::unordered_map<ValueId, Definition> definitions;

    for (const auto& block : module.blocks) {
        if (!blocks.emplace(block.id, &block).second) {
            fail("duplicate basic block id " + std::to_string(block.id));
        }
        if (!block.terminator) {
            fail("basic block '" + block.name + "' has no terminator");
        }

        bool sawNonPhi = false;
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            const Operation& operation = block.operations[index];
            const bool isPhi = std::holds_alternative<PhiOp>(operation);
            if (isPhi && sawNonPhi) {
                fail("phi nodes must precede other operations in block '" +
                     block.name + "'");
            }
            sawNonPhi = sawNonPhi || !isPhi;

            const ValueId result = resultOf(operation);
            if (!definitions.emplace(
                    result,
                    Definition{block.id, index, typeOf(operation)}
                ).second) {
                fail("SSA value %" + std::to_string(result) +
                     " has multiple definitions");
            }
        }
    }

    if (!blocks.contains(module.entry)) {
        fail("entry block does not exist");
    }

    std::unordered_map<BlockId, std::set<BlockId>> predecessors;
    auto requireBlock = [&blocks](const BlockId id) {
        if (!blocks.contains(id)) {
            fail("branch targets nonexistent block " + std::to_string(id));
        }
    };

    for (const auto& block : module.blocks) {
        std::visit([&](const auto& terminator) {
            using T = std::decay_t<decltype(terminator)>;
            if constexpr (std::is_same_v<T, BranchOp>) {
                requireBlock(terminator.target);
                predecessors[terminator.target].insert(block.id);
            } else if constexpr (std::is_same_v<T, CondBranchOp>) {
                requireBlock(terminator.trueTarget);
                requireBlock(terminator.falseTarget);
                predecessors[terminator.trueTarget].insert(block.id);
                predecessors[terminator.falseTarget].insert(block.id);
            }
        }, *block.terminator);
    }

    auto requireValue = [&definitions](ValueId value) -> const Definition& {
        const auto found = definitions.find(value);
        if (found == definitions.end()) {
            fail("use of nonexistent SSA value %" + std::to_string(value));
        }
        return found->second;
    };

    auto requireOperand = [&](ValueId value, const sema::TensorType& type,
                              BlockId useBlock, std::size_t useIndex) {
        const Definition& definition = requireValue(value);
        if (!sameType(definition.type, type)) {
            fail("type mismatch for SSA value %" + std::to_string(value));
        }
        if (definition.block == useBlock && definition.operationIndex >= useIndex) {
            fail("SSA value %" + std::to_string(value) +
                 " is used before its definition");
        }
    };

    std::optional<sema::TensorType> returnType;
    for (const auto& block : module.blocks) {
        for (std::size_t index = 0; index < block.operations.size(); ++index) {
            const Operation& operation = block.operations[index];
            std::visit([&](const auto& op) {
                using Op = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<Op, BinaryOp>) {
                    requireOperand(op.lhs, op.type, block.id, index);
                    requireOperand(op.rhs, op.type, block.id, index);
                } else if constexpr (std::is_same_v<Op, CompareOp>) {
                    requireOperand(op.lhs, op.operandType, block.id, index);
                    requireOperand(op.rhs, op.operandType, block.id, index);
                } else if constexpr (std::is_same_v<Op, PhiOp>) {
                    std::set<BlockId> incomingBlocks;
                    for (const auto& incoming : op.incoming) {
                        requireOperand(incoming.value, op.type, block.id, index);
                        if (!incomingBlocks.insert(incoming.predecessor).second) {
                            fail("phi has duplicate predecessor");
                        }
                    }
                    if (incomingBlocks != predecessors[block.id]) {
                        fail("phi predecessors do not match CFG predecessors in block '" +
                             block.name + "'");
                    }
                } else if constexpr (std::is_same_v<Op, MatMulOp>) {
                    requireValue(op.lhs);
                    requireValue(op.rhs);
                } else if constexpr (std::is_same_v<Op, ReluOp>) {
                    requireValue(op.input);
                }
            }, operation);
        }

        std::visit([&](const auto& terminator) {
            using T = std::decay_t<decltype(terminator)>;
            if constexpr (std::is_same_v<T, CondBranchOp>) {
                const Definition& condition = requireValue(terminator.condition);
                const sema::TensorType expected{.elementType = "i1", .shape = {}};
                if (!sameType(condition.type, expected)) {
                    fail("conditional branch condition must have type i1");
                }
            } else if constexpr (std::is_same_v<T, ReturnOp>) {
                const Definition& value = requireValue(terminator.value);
                if (returnType && !sameType(*returnType, value.type)) {
                    fail("return terminators have inconsistent types");
                }
                returnType = value.type;
            }
        }, *block.terminator);
    }

    if (!returnType) {
        fail("module has no return terminator");
    }
}

} // namespace ocelotl::ir
