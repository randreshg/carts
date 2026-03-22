///==========================================================================///
/// Epoch Lowering Pass
/// Transforms arts.epoch into CreateEpochOp + WaitOnEpochOp, propagating
/// epoch GUIDs to contained EdtCreateOps.
///
/// For continuation epochs (marked by EpochOpt), wires the epoch's finish
/// target to the continuation EDT and skips WaitOnEpochOp.
///
/// Example (standard path):
///   Before:
///     arts.epoch { ... arts.edt_create ... }
///
///   After:
///     %e = arts.create_epoch
///     ... arts.edt_create(..., %e) ...
///     arts.wait_on_epoch %e
///
/// Example (continuation path):
///   Before:
///     arts.epoch { ... arts.edt_create ... } {arts.continuation_for_epoch}
///     %cont = arts.edt_create(...) {arts.continuation_for_epoch, ...}
///
///   After:
///     %cont = arts.edt_create(...)
///     %e = arts.create_epoch finish(%cont_guid, %control_slot)
///     ... arts.edt_create(..., %e) ...
///     // NO wait_on_epoch
///==========================================================================///

#include "arts/Dialect.h"
#include "arts/codegen/Codegen.h"
#define GEN_PASS_DEF_EPOCHLOWERING
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "arts/utils/OperationAttributes.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/IRMapping.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Support/LLVM.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

#include "arts/utils/Debug.h"
#include "arts/utils/OperationAttributes.h"
ARTS_DEBUG_SETUP(epoch_lowering);

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;
using AttrNames::Operation::ContinuationForEpoch;
using AttrNames::Operation::CPSChainId;

namespace {
constexpr llvm::StringLiteral kCPSOuterEpochIdx =
    "arts.cps_outer_epoch_param_idx";
constexpr llvm::StringLiteral kCPSIterCounterIdx =
    "arts.cps_iter_counter_param_idx";
constexpr llvm::StringLiteral kCPSParamPerm = "arts.cps_param_perm";
} // namespace

///===----------------------------------------------------------------------===///
/// Epoch Lowering Pass Implementation
///===----------------------------------------------------------------------===///
struct EpochLoweringPass : public impl::EpochLoweringBase<EpochLoweringPass> {
  explicit EpochLoweringPass(bool debug = false) : debugMode(debug) {}

  void runOnOperation() override;

private:
  /// State
  ModuleOp module;
  ArtsCodegen *AC = nullptr;
  bool debugMode = false;
};

///===----------------------------------------------------------------------===///
/// Pass Implementation
///===----------------------------------------------------------------------===///

void EpochLoweringPass::runOnOperation() {
  module = getOperation();
  auto ownedAC = std::make_unique<ArtsCodegen>(module, debugMode);
  AC = ownedAC.get();

  ARTS_INFO_HEADER(EpochLoweringPass);
  ARTS_DEBUG_REGION(module.dump(););

  /// Collect all epoch operations bottom-to-top (post-order) so inner epochs
  /// are lowered before their parents.
  SmallVector<EpochOp> epochOps;
  module.walk<WalkOrder::PostOrder>(
      [&](EpochOp epochOp) { epochOps.push_back(epochOp); });

  ARTS_INFO("Found " << epochOps.size() << " epoch operations to lower");

  for (EpochOp epochOp : epochOps) {
    ARTS_INFO("Lowering Epoch Op " << epochOp);

    /// Elide empty epochs.
    auto &epochRegion = epochOp.getRegion();
    if (epochRegion.empty() ||
        epochRegion.front().without_terminator().empty()) {
      epochOp.erase();
      continue;
    }

    /// Check if this epoch has a continuation EDT (set by EpochOpt).
    bool hasContinuation = epochOp->hasAttr(ContinuationForEpoch);
    Value finishGuid;
    Value finishSlot;

    if (hasContinuation) {
      /// Find the continuation EdtCreateOp placed after this epoch by EpochOpt
      /// (already lowered from arts.edt by EdtLowering).
      EdtCreateOp contEdtCreate = nullptr;
      for (Operation *op = epochOp->getNextNode(); op; op = op->getNextNode()) {
        if (auto edt = dyn_cast<EdtCreateOp>(op)) {
          if (edt->hasAttr(ContinuationForEpoch)) {
            contEdtCreate = edt;
            break;
          }
        }
      }

      if (contEdtCreate) {
        /// Recursively move the continuation EdtCreateOp and ALL transitive
        /// operand-defining ops before the epoch. A shallow move misses ops
        /// like DB handle extractions (memref.load, llvm.ptrtoint) that feed
        /// into edt_param_pack but are not direct operands of EdtCreateOp.
        DenseSet<Operation *> moved;
        std::function<void(Operation *)> moveWithDeps = [&](Operation *op) {
          if (!op || moved.contains(op))
            return;
          if (op->getBlock() != epochOp->getBlock())
            return;
          if (!epochOp->isBeforeInBlock(op))
            return;
          moved.insert(op);
          for (Value operand : op->getOperands())
            if (auto *defOp = operand.getDefiningOp())
              moveWithDeps(defOp);
          op->moveBefore(epochOp);
        };
        moveWithDeps(contEdtCreate.getOperation());

        finishGuid = contEdtCreate.getGuid();
        /// Control slot = depCount - 1 (EdtLowering added +1 for the control dep).
        AC->setInsertionPointAfter(contEdtCreate);
        Value depCount = contEdtCreate.getDepCount();
        Value one = AC->createIntConstant(1, AC->Int32, epochOp.getLoc());
        finishSlot = AC->create<arith::SubIOp>(epochOp.getLoc(), depCount, one);
        ARTS_INFO("  Continuation path: finish GUID from "
                  << contEdtCreate << ", control slot = depCount - 1");
      } else {
        ARTS_WARN("  Epoch marked for continuation but no continuation "
                  "EdtCreateOp found; falling back to wait path");
        hasContinuation = false;
      }
    }

    /// Create the CreateEpochOp.
    AC->setInsertionPoint(epochOp);
    auto createEpochOp = AC->create<CreateEpochOp>(
        epochOp.getLoc(), IntegerType::get(AC->getContext(), 64),
        hasContinuation ? finishGuid : Value(),
        hasContinuation ? finishSlot : Value());
    auto currentEpoch = createEpochOp.getEpochGuid();

    /// Collect EdtCreateOps that need the epoch GUID.
    SmallVector<EdtCreateOp, 8> edtCreatesToUpdate;
    epochOp.walk([&](EdtCreateOp edtCreateOp) {
      if (!edtCreateOp.getEpochGuid())
        edtCreatesToUpdate.push_back(edtCreateOp);
    });

    ARTS_INFO("Updating " << edtCreatesToUpdate.size()
                          << " EdtCreateOps with epoch GUID");

    for (EdtCreateOp edtCreateOp : edtCreatesToUpdate) {
      AC->setInsertionPoint(edtCreateOp);
      auto newEdtCreateOp = AC->create<EdtCreateOp>(
          edtCreateOp.getLoc(), edtCreateOp.getParamMemref(),
          edtCreateOp.getDepCount(), edtCreateOp.getRoute(), currentEpoch);
      for (auto attr : edtCreateOp->getAttrs())
        newEdtCreateOp->setAttr(attr.getName(), attr.getValue());
      edtCreateOp->replaceAllUsesWith(newEdtCreateOp);
      edtCreateOp->erase();
    }

    /// Move operations out of the epoch region, tracking where to insert
    /// the wait afterward.
    auto &epochRegionForMove = epochOp.getRegion();
    Operation *insertionAfter = epochOp.getOperation();
    if (!epochRegionForMove.empty()) {
      auto &epochBlock = epochRegionForMove.front();
      SmallVector<Operation *> opsToMove;
      for (auto &innerOp : epochBlock.without_terminator()) {
        if (!isa<EpochOp>(innerOp))
          opsToMove.push_back(&innerOp);
      }
      for (auto *opToMove : opsToMove) {
        opToMove->moveBefore(epochOp);
        insertionAfter = opToMove;
      }
    }

    /// Determine whether to emit WaitOnEpochOp.
    ///
    /// Non-continuation epochs always get a blocking wait.
    ///
    /// Continuation epochs skip the wait entirely because the EDT's return
    /// naturally calls arts_increment_finished_epoch_list, processing all
    /// epochs in the EDT's epoch_list.
    ///
    /// Continuation epochs nested inside another EpochOp (the mainBody
    /// pattern from CPS chain) use arts_initialize_epoch (no caller-active
    /// count) instead of arts_initialize_and_start_epoch, so the epoch
    /// fires when all N workers finish (active=N, finished=N) without
    /// needing the caller to resolve its accounting.
    bool needsWait = !hasContinuation;
    if (hasContinuation) {
      bool nestedInEpoch = epochOp->getParentOfType<EpochOp>() != nullptr;
      if (nestedInEpoch) {
        createEpochOp->setAttr(AttrNames::Operation::NoStartEpoch,
                               AC->getBuilder().getUnitAttr());
        ARTS_INFO("  Nested continuation epoch — using no-start epoch "
                  "(caller not active)");
      }
      ARTS_INFO("  Skipping WaitOnEpochOp (continuation path)");
    }
    if (needsWait) {
      AC->setInsertionPointAfter(insertionAfter);
      AC->create<WaitOnEpochOp>(epochOp.getLoc(), currentEpoch);
    }

    /// Check CPS outer epoch attributes BEFORE erasing the EpochOp.
    bool isCPSOuterEpoch =
        epochOp->hasAttr(AttrNames::Operation::CPSOuterEpoch);
    int64_t cpsInitIter = 0;
    if (auto initIterAttr =
            epochOp->getAttrOfType<IntegerAttr>("arts.cps_init_iter"))
      cpsInitIter = initIterAttr.getInt();

    /// Replace the epoch op with the epoch GUID.
    epochOp.replaceAllUsesWith(currentEpoch);
    epochOp.erase();

    /// For CPS outer epochs, inject the iteration counter AND outer epoch
    /// GUID into all continuation EdtCreateOps' param_packs so they
    /// propagate through the chain.
    if (isCPSOuterEpoch) {
      func::FuncOp parentFunc =
          createEpochOp->getParentOfType<func::FuncOp>();
      ARTS_INFO("CPS outer epoch: injecting iter counter + outer GUID "
                "into chain EDTs");
      if (parentFunc) {
        parentFunc.walk([&](EdtCreateOp edt) {
          if (!edt->hasAttr(CPSChainId)) {
            return;
          }
          ARTS_INFO("  Found chain EDT: " << edt);
          auto packOp =
              edt.getParamMemref().getDefiningOp<EdtParamPackOp>();
          if (!packOp)
            return;
          /// Build a new param_pack with iter counter + outer epoch GUID
          /// appended. Iter counter first so its index is stable.
          SmallVector<Value> packParams(packOp->getOperands());
          AC->setInsertionPoint(packOp);
          Value initIterVal =
              AC->createIntConstant(cpsInitIter, AC->Int64, packOp->getLoc());
          packParams.push_back(initIterVal);
          unsigned iterIdx = packParams.size() - 1;
          packParams.push_back(currentEpoch);
          unsigned epochIdx = packParams.size() - 1;
          auto newPack = AC->create<EdtParamPackOp>(
              packOp->getLoc(), packOp->getResultTypes()[0], packParams);
          packOp->getResult(0).replaceAllUsesWith(newPack.getMemref());
          packOp->erase();
          edt->setAttr(kCPSIterCounterIdx,
                       AC->getBuilder().getI64IntegerAttr(iterIdx));
          edt->setAttr(kCPSOuterEpochIdx,
                       AC->getBuilder().getI64IntegerAttr(epochIdx));
          ARTS_INFO("  Injected iter counter at idx " << iterIdx
                    << ", outer epoch at idx " << epochIdx);
        });
      }
    }
  }

  /// Propagate iter counter + outer epoch GUID through the chain. The
  /// initial injection (above) only added them to chain EDTs in the main
  /// function. Outlined chain functions (e.g. __arts_edt_5 containing
  /// chain_1) also need to pass them to their child chain EDTs.
  /// This MUST run before CPSAdvance resolution because it computes param
  /// permutations that the CPSAdvance resolver needs.
  {
    struct ChainFuncInfo {
      int64_t iterIdx;
      int64_t epochIdx;
    };
    DenseMap<StringRef, ChainFuncInfo> funcToChainInfo;
    module.walk([&](EdtCreateOp edt) {
      auto iterAttr = edt->getAttrOfType<IntegerAttr>(kCPSIterCounterIdx);
      auto epochAttr = edt->getAttrOfType<IntegerAttr>(kCPSOuterEpochIdx);
      auto funcAttr =
          edt->getAttrOfType<StringAttr>(AttrNames::Operation::OutlinedFunc);
      if (iterAttr && epochAttr && funcAttr)
        funcToChainInfo[funcAttr.getValue()] = {iterAttr.getInt(),
                                                 epochAttr.getInt()};
    });

    for (auto &[funcName, info] : funcToChainInfo) {
      auto func = module.lookupSymbol<func::FuncOp>(funcName);
      if (!func)
        continue;

      SmallVector<EdtCreateOp> childChainEdts;
      func.walk([&](EdtCreateOp edt) {
        if (edt->hasAttr(CPSChainId) && !edt->hasAttr(kCPSOuterEpochIdx))
          childChainEdts.push_back(edt);
      });
      if (childChainEdts.empty())
        continue;

      EdtParamUnpackOp unpackOp = nullptr;
      func.walk([&](EdtParamUnpackOp op) {
        if (!unpackOp)
          unpackOp = op;
      });
      if (!unpackOp)
        continue;

      Location loc = func.getLoc();
      AC->setInsertionPointAfter(unpackOp);
      Value paramArrayMemref = unpackOp->getOperand(0);

      /// Load iter counter from parent's param array.
      Value iterIdxVal = AC->createIndexConstant(info.iterIdx, loc);
      Value iterCounter = AC->create<memref::LoadOp>(
          loc, paramArrayMemref, ValueRange{iterIdxVal});
      /// Load outer epoch GUID from parent's param array.
      Value epochIdxVal = AC->createIndexConstant(info.epochIdx, loc);
      Value outerEpochGuid = AC->create<memref::LoadOp>(
          loc, paramArrayMemref, ValueRange{epochIdxVal});

      ARTS_INFO("CPS propagation: " << funcName
                << " loading iter[" << info.iterIdx << "], epoch["
                << info.epochIdx << "]");

      /// Build a map from parent function's unpack results to their indices.
      DenseMap<Value, int64_t> unpackResultToIdx;
      for (unsigned i = 0; i < unpackOp->getNumResults(); ++i)
        unpackResultToIdx[unpackOp->getResult(i)] = i;

      for (EdtCreateOp edt : childChainEdts) {
        auto packOp = edt.getParamMemref().getDefiningOp<EdtParamPackOp>();
        if (!packOp)
          continue;

        /// Compute permutation: for each pack operand, find which parent
        /// param index it derives from. We check two paths:
        /// 1. SSA identity with EdtParamUnpackOp results (pre-CSE)
        /// 2. memref.load from the param array with a constant index (post-CSE)
        /// DB handles go through conversions (inttoptr, ptrtoint) so we trace
        /// the def chain up to 10 levels deep.
        auto traceToParamIdx = [&](Value val) -> int64_t {
          for (int depth = 0; depth < 10; ++depth) {
            /// Check SSA identity with unpack results.
            auto it = unpackResultToIdx.find(val);
            if (it != unpackResultToIdx.end())
              return it->second;
            auto *defOp = val.getDefiningOp();
            if (!defOp)
              return -1;
            /// Check if this is a memref.load from the param array.
            if (auto loadOp = dyn_cast<memref::LoadOp>(defOp)) {
              if (loadOp.getMemRef() == paramArrayMemref &&
                  loadOp.getIndices().size() == 1) {
                if (auto idxOp =
                        loadOp.getIndices()[0]
                            .getDefiningOp<arith::ConstantIndexOp>())
                  return idxOp.value();
              }
            }
            if (defOp->getNumOperands() == 0)
              return -1;
            val = defOp->getOperand(0);
          }
          return -1;
        };
        SmallVector<int64_t> perm;
        for (Value operand : packOp->getOperands())
          perm.push_back(traceToParamIdx(operand));
        edt->setAttr(kCPSParamPerm,
                     AC->getBuilder().getDenseI64ArrayAttr(perm));

        SmallVector<Value> packParams(packOp->getOperands());
        packParams.push_back(iterCounter);
        unsigned newIterIdx = packParams.size() - 1;
        packParams.push_back(outerEpochGuid);
        unsigned newEpochIdx = packParams.size() - 1;
        AC->setInsertionPoint(packOp);
        auto newPack = AC->create<EdtParamPackOp>(
            packOp->getLoc(), packOp->getResultTypes()[0], packParams);
        packOp->getResult(0).replaceAllUsesWith(newPack.getMemref());
        packOp->erase();
        edt->setAttr(kCPSIterCounterIdx,
                     AC->getBuilder().getI64IntegerAttr(newIterIdx));
        edt->setAttr(kCPSOuterEpochIdx,
                     AC->getBuilder().getI64IntegerAttr(newEpochIdx));
        ARTS_INFO("  Injected iter[" << newIterIdx << "], epoch["
                  << newEpochIdx << "] for child chain EDT");
      }
    }
  }

  /// Resolve CPSAdvanceOp placeholders left by the CPS chain transform.
  SmallVector<CPSAdvanceOp> cpsAdvanceOps;
  module.walk([&](CPSAdvanceOp op) { cpsAdvanceOps.push_back(op); });

  if (!cpsAdvanceOps.empty())
    ARTS_INFO("Resolving " << cpsAdvanceOps.size()
                           << " CPS advance placeholder(s)");

  for (CPSAdvanceOp advanceOp : cpsAdvanceOps) {
    StringRef targetChainId = advanceOp.getTargetChainId();

    /// Find the EdtCreateOp with matching cps_chain_id in the same function.
    func::FuncOp parentFunc = advanceOp->getParentOfType<func::FuncOp>();
    EdtCreateOp contEdtCreate = nullptr;
    if (parentFunc) {
      parentFunc.walk([&](EdtCreateOp edt) {
        auto chainAttr = edt->getAttrOfType<StringAttr>(CPSChainId);
        if (chainAttr && chainAttr.getValue() == targetChainId)
          contEdtCreate = edt;
      });
    }

    if (!contEdtCreate) {
      /// Fall back to module-wide search (target may be in a different function).
      module.walk([&](EdtCreateOp edt) {
        auto chainAttr = edt->getAttrOfType<StringAttr>(CPSChainId);
        if (chainAttr && chainAttr.getValue() == targetChainId)
          contEdtCreate = edt;
      });
    }

    if (!contEdtCreate) {
      advanceOp.emitError("CPS advance: no EdtCreateOp with chain_id '")
          << targetChainId << "' found";
      return signalPassFailure();
    }

    auto outlinedFunc =
        contEdtCreate->getAttrOfType<StringAttr>(AttrNames::Operation::OutlinedFunc);
    if (!outlinedFunc) {
      advanceOp.emitError("CPS advance: continuation has no outlined_func");
      return signalPassFailure();
    }

    ARTS_INFO("CPS advance: resolving chain '"
              << targetChainId << "' → " << outlinedFunc.getValue());

    Location loc = advanceOp.getLoc();
    AC->setInsertionPoint(advanceOp);

    /// Re-materialize depCount and route as local constants to avoid
    /// cross-function SSA references.
    Value origDepCount = contEdtCreate.getDepCount();
    int64_t depCountVal = 1; // default: 1 dep (control)
    if (auto cst = origDepCount.getDefiningOp<arith::ConstantIntOp>())
      depCountVal = cst.value();
    else if (auto cst = origDepCount.getDefiningOp<arith::ConstantOp>())
      if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue()))
        depCountVal = intAttr.getInt();
    Value localDepCount = AC->createIntConstant(depCountVal, AC->Int32, loc);

    Value origRoute = contEdtCreate.getRoute();
    int64_t routeVal = 0;
    if (auto cst = origRoute.getDefiningOp<arith::ConstantIntOp>())
      routeVal = cst.value();
    else if (auto cst = origRoute.getDefiningOp<arith::ConstantOp>())
      if (auto intAttr = dyn_cast<IntegerAttr>(cst.getValue()))
        routeVal = intAttr.getInt();
    Value localRoute = AC->createIntConstant(routeVal, AC->Int32, loc);

    SmallVector<Value> nextParams(advanceOp.getNextIterParams());

    /// Find the local edt_param_unpack to get this function's params.
    EdtParamUnpackOp localUnpack = nullptr;
    parentFunc.walk([&](EdtParamUnpackOp op) {
      if (!localUnpack)
        localUnpack = op;
    });

    /// Read extra parameters (iter counter + outer epoch GUID) from the raw
    /// parameter memref. These were injected by the CPS propagation above.
    Value outerEpochGuid;
    Value curIter;
    auto outerEpochIdxAttr =
        contEdtCreate->getAttrOfType<IntegerAttr>(kCPSOuterEpochIdx);
    auto iterCounterIdxAttr =
        contEdtCreate->getAttrOfType<IntegerAttr>(kCPSIterCounterIdx);
    if (localUnpack) {
      Value paramArrayMemref = localUnpack->getOperand(0);
      if (iterCounterIdxAttr) {
        unsigned idx = iterCounterIdxAttr.getInt();
        Value idxVal = AC->createIndexConstant(idx, loc);
        curIter = AC->create<memref::LoadOp>(
            loc, paramArrayMemref, ValueRange{idxVal});
      }
      if (outerEpochIdxAttr) {
        unsigned idx = outerEpochIdxAttr.getInt();
        Value idxVal = AC->createIndexConstant(idx, loc);
        outerEpochGuid = AC->create<memref::LoadOp>(
            loc, paramArrayMemref, ValueRange{idxVal});
      }
    }

    bool isAdditive = advanceOp->hasAttr("arts.cps_additive_params");

    if (!outerEpochGuid) {
      auto outerEpochGuidOp = AC->create<GetEdtEpochGuidOp>(
          loc, IntegerType::get(AC->getContext(), 64));
      outerEpochGuid = outerEpochGuidOp.getEpochGuid();
    }

    Value tNext;
    if (isAdditive && curIter && nextParams.size() >= 2) {
      Value step = nextParams[0];
      Value ub = nextParams[1];
      if (step.getType() != curIter.getType())
        step = AC->create<arith::IndexCastOp>(loc, curIter.getType(), step);
      if (ub.getType() != curIter.getType())
        ub = AC->create<arith::IndexCastOp>(loc, curIter.getType(), ub);
      tNext = AC->create<arith::AddIOp>(loc, curIter, step);
      Value cond = AC->create<arith::CmpIOp>(
          loc, arith::CmpIPredicate::slt, tNext, ub);
      auto ifOp = AC->create<scf::IfOp>(loc, cond, /*withElse=*/false);
      Block &thenBlock = ifOp.getThenRegion().front();
      AC->setInsertionPoint(thenBlock.getTerminator());
    }

    /// Build the param_pack using localUnpack results with a permutation
    /// to match the target chain function's expected param layout.
    ///
    /// Chain functions may have different DB handle orderings (e.g., chain_0
    /// has [guidB, ptrB, guidA, ptrA] while chain_1 has [guidA, ptrA, guidB,
    /// ptrB]). The permutation was computed during CPS propagation by tracing
    /// each child chain's pack operands back to the parent's unpack results.
    ///
    /// perm[i] = j means: child param[i] = parent param[j].
    /// Inverse: inv[j] = i means: parent param[j] = child param[i].
    Value paramMemref;
    if (localUnpack) {
      auto unpackResults = localUnpack.getResults();
      unsigned numUserParams = unpackResults.size();

      /// Find the EdtCreateOp that created the current function to get
      /// the permutation (how current function's params map to parent's).
      SmallVector<int64_t> invPerm;
      EdtCreateOp callerEdt = nullptr;
      module.walk([&](EdtCreateOp edt) {
        auto funcAttr = edt->getAttrOfType<StringAttr>(
            AttrNames::Operation::OutlinedFunc);
        if (funcAttr && funcAttr.getValue() == parentFunc.getName())
          callerEdt = edt;
      });
      if (callerEdt) {
        if (auto permAttr =
                callerEdt->getAttrOfType<DenseI64ArrayAttr>(kCPSParamPerm)) {
          ArrayRef<int64_t> perm = permAttr.asArrayRef();
          /// Compute inverse permutation: inv[perm[i]] = i.
          invPerm.resize(numUserParams, -1);
          for (unsigned i = 0; i < perm.size() && i < numUserParams; ++i) {
            int64_t j = perm[i];
            if (j >= 0 && static_cast<unsigned>(j) < numUserParams)
              invPerm[j] = i;
          }
        }
      }

      SmallVector<Value> newPackParams;
      if (!invPerm.empty()) {
        /// Apply inverse permutation: target param[j] = local param[inv[j]].
        for (unsigned j = 0; j < numUserParams; ++j) {
          int64_t srcIdx = invPerm[j];
          if (srcIdx >= 0 && static_cast<unsigned>(srcIdx) < numUserParams)
            newPackParams.push_back(unpackResults[srcIdx]);
          else
            newPackParams.push_back(unpackResults[j]);
        }
      } else {
        for (unsigned i = 0; i < numUserParams; ++i)
          newPackParams.push_back(unpackResults[i]);
      }

      /// Append iter counter and outer epoch GUID.
      newPackParams.push_back(tNext ? tNext : curIter);
      newPackParams.push_back(outerEpochGuid);
      auto packType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      paramMemref =
          AC->create<EdtParamPackOp>(loc, packType, newPackParams).getMemref();
    } else {
      SmallVector<Value> packVals;
      if (tNext)
        packVals.push_back(tNext);
      else if (curIter)
        packVals.push_back(curIter);
      if (outerEpochGuid)
        packVals.push_back(outerEpochGuid);
      auto packType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      paramMemref =
          AC->create<EdtParamPackOp>(loc, packType, packVals).getMemref();
    }

    /// Create the self-referential EdtCreateOp, enrolled in the outer epoch.
    auto newContEdt = AC->create<EdtCreateOp>(loc, paramMemref, localDepCount,
                                               localRoute, outerEpochGuid);
    setOutlinedFunc(newContEdt, outlinedFunc.getValue());
    newContEdt->setAttr(ContinuationForEpoch, AC->getBuilder().getUnitAttr());

    Value newContGuid = newContEdt.getGuid();

    /// Finish slot = depCount - 1.
    Value one = AC->createIntConstant(1, AC->Int32, loc);
    Value finishSlot = AC->create<arith::SubIOp>(loc, localDepCount, one);

    /// Create epoch with finish target pointing to the new continuation.
    auto innerEpoch = AC->create<CreateEpochOp>(
        loc, IntegerType::get(AC->getContext(), 64), newContGuid, finishSlot);
    Value epochGuid = innerEpoch.getEpochGuid();

    /// Move worker ops from CPSAdvanceOp's epochBody AFTER the epoch creation.
    Block &advBody = advanceOp.getEpochBody().front();
    SmallVector<Operation *> workerOps;
    for (Operation &op : advBody.without_terminator())
      workerOps.push_back(&op);

    Operation *insertAfter = innerEpoch.getOperation();
    for (Operation *workerOp : workerOps) {
      workerOp->moveAfter(insertAfter);
      insertAfter = workerOp;
    }

    /// Update EdtCreateOps in moved worker ops with the epoch GUID.
    for (Operation *workerOp : workerOps) {
      auto updateEdtCreates = [&](EdtCreateOp edtCreate) {
        if (!edtCreate.getEpochGuid()) {
          AC->setInsertionPoint(edtCreate);
          auto updated = AC->create<EdtCreateOp>(
              edtCreate.getLoc(), edtCreate.getParamMemref(),
              edtCreate.getDepCount(), edtCreate.getRoute(), epochGuid);
          for (auto attr : edtCreate->getAttrs())
            updated->setAttr(attr.getName(), attr.getValue());
          edtCreate->replaceAllUsesWith(updated);
          edtCreate->erase();
        }
      };

      if (auto edtCreate = dyn_cast<EdtCreateOp>(workerOp)) {
        updateEdtCreates(edtCreate);
      } else {
        workerOp->walk(updateEdtCreates);
      }
    }

    advanceOp.erase();
    ARTS_INFO("CPS advance: resolved with epoch finish → "
              << outlinedFunc.getValue());
  }

  ARTS_INFO_FOOTER(EpochLoweringPass);
  AC = nullptr;
  ARTS_DEBUG_REGION(module.dump(););
}

///===----------------------------------------------------------------------===///
/// Pass Registration
///===----------------------------------------------------------------------===///

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createEpochLoweringPass() {
  return std::make_unique<EpochLoweringPass>();
}

} // namespace arts
} // namespace mlir
