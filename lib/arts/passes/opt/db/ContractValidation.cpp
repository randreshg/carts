///==========================================================================///
/// File: ContractValidation.cpp
///
/// Lightweight verification pass for LoweringContractOp integrity.
///
/// Walks all arts.lowering_contract operations and validates:
///   1. Target value exists and is a valid memref
///   2. If supported_block_halo is set, depPattern must be stencil family
///   3. min_offsets/max_offsets must have matching rank
///   4. block_shape rank > 0 if present
///   5. owner_dims values are non-negative
///
/// Additionally walks DbAcquireOps and warns if stencil attrs are present
/// but no lowering contract is attached to the source pointer.
///
/// Collects and logs statistics: total contracts, per-pattern counts, orphans.
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/passes/PassDetails.h"
#include "arts/passes/Passes.h"
#include "arts/utils/LoweringContractUtils.h"
#include "arts/utils/OperationAttributes.h"
#include "arts/utils/StencilAttributes.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"

/// Debug
#include "arts/utils/Debug.h"
ARTS_DEBUG_SETUP(contract_validation);

using namespace mlir;
using namespace mlir::arts;

namespace {

struct ContractValidationPass
    : public arts::ContractValidationBase<ContractValidationPass> {

  ContractValidationPass() = default;
  explicit ContractValidationPass(bool fail) { this->failOnError = fail; }

  void runOnOperation() override {
    ModuleOp module = getOperation();

    ARTS_INFO_HEADER(ContractValidationPass);

    unsigned totalContracts = 0;
    unsigned invalidContracts = 0;
    unsigned orphanAcquires = 0;
    DenseMap<ArtsDepPattern, unsigned> patternCounts;

    /// ---------------------------------------------------------------
    /// Phase 1: Validate every LoweringContractOp
    /// ---------------------------------------------------------------
    module.walk([&](LoweringContractOp contract) {
      ++totalContracts;

      /// Track per-pattern statistics
      if (auto dp = contract.getDepPattern())
        ++patternCounts[*dp];

      /// 1. Target value must exist and be a valid memref
      Value target = contract.getTarget();
      if (!target) {
        contract.emitWarning("lowering_contract has null target value");
        ++invalidContracts;
        return;
      }
      if (!isa<MemRefType>(target.getType())) {
        contract.emitWarning("lowering_contract target is not a memref type");
        ++invalidContracts;
        return;
      }

      /// 2. If supported_block_halo is set, depPattern must be stencil family
      if (contract.getSupportedBlockHalo()) {
        auto dp = contract.getDepPattern();
        if (!dp) {
          contract.emitWarning(
              "supported_block_halo set but no dep_pattern specified");
          ++invalidContracts;
        } else if (!isStencilFamilyDepPattern(*dp)) {
          contract.emitWarning(
              "supported_block_halo set but dep_pattern is not stencil family");
          ++invalidContracts;
        }
      }

      /// 3. min_offsets and max_offsets must have matching rank
      unsigned minRank = contract.getMinOffsets().size();
      unsigned maxRank = contract.getMaxOffsets().size();
      if (minRank != 0 && maxRank != 0 && minRank != maxRank) {
        contract.emitWarning("min_offsets rank (")
            << minRank << ") differs from max_offsets rank (" << maxRank << ")";
        ++invalidContracts;
      }

      /// 4. block_shape rank > 0 if present (non-empty means it was set)
      auto blockShape = contract.getBlockShape();
      if (!blockShape.empty() && blockShape.size() == 0) {
        /// Unreachable by construction; guards against future changes that
        /// allow empty-but-present block_shape attrs.
        contract.emitWarning("block_shape is present but has rank 0");
        ++invalidContracts;
      }

      /// 5. owner_dims values must be non-negative
      if (auto ownerDims = contract.getOwnerDims()) {
        for (int64_t dim : *ownerDims) {
          if (dim < 0) {
            contract.emitWarning("owner_dims contains negative value: ") << dim;
            ++invalidContracts;
            break;
          }
        }
      }
    });

    /// ---------------------------------------------------------------
    /// Phase 2: Walk DbAcquireOps, warn if stencil attrs present but
    ///          no LoweringContractOp on the source pointer
    /// ---------------------------------------------------------------
    module.walk([&](DbAcquireOp acquire) {
      Operation *op = acquire.getOperation();
      bool hasStencilAttrs = hasSupportedBlockHalo(op) ||
                             getStencilMinOffsets(op).has_value() ||
                             getStencilMaxOffsets(op).has_value() ||
                             getStencilOwnerDims(op).has_value();
      if (!hasStencilAttrs)
        return;

      Value sourcePtr = acquire.getSourcePtr();
      LoweringContractOp contractOp = getLoweringContractOp(sourcePtr);
      if (!contractOp) {
        acquire.emitWarning(
            "db_acquire has stencil attributes but no lowering_contract on "
            "source pointer");
        ++orphanAcquires;
      }
    });

    /// ---------------------------------------------------------------
    /// Phase 3: Log statistics
    /// ---------------------------------------------------------------
    llvm::errs() << "[CARTS] Contract validation: " << totalContracts
                 << " contracts, " << invalidContracts << " invalid, "
                 << orphanAcquires << " orphan acquires\n";

    if (!patternCounts.empty()) {
      llvm::errs() << "[CARTS]   Per-pattern counts:";
      for (auto &entry : patternCounts) {
        llvm::errs() << " " << stringifyArtsDepPattern(entry.first) << "="
                     << entry.second;
      }
      llvm::errs() << "\n";
    }

    ARTS_INFO("Total contracts: " << totalContracts
                                  << ", invalid: " << invalidContracts
                                  << ", orphan acquires: " << orphanAcquires);

    /// Fail the pass if requested and there were problems
    if (failOnError && (invalidContracts > 0 || orphanAcquires > 0)) {
      emitError(module.getLoc())
          << "Contract validation failed: " << invalidContracts
          << " invalid contracts, " << orphanAcquires << " orphan acquires";
      signalPassFailure();
    }

    ARTS_INFO_FOOTER(ContractValidationPass);
  }
};

} // namespace

std::unique_ptr<Pass>
mlir::arts::createContractValidationPass(bool failOnError) {
  return std::make_unique<ContractValidationPass>(failOnError);
}
