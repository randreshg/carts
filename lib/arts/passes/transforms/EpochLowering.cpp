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
#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#define GEN_PASS_DEF_EPOCHLOWERING
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "arts/passes/Passes.h.inc"
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

#include "llvm/ADT/Statistic.h"
static llvm::Statistic numEpochsLowered{
    "epoch_lowering", "NumEpochsLowered",
    "Number of epoch operations lowered to CreateEpochOp + WaitOnEpochOp"};
static llvm::Statistic numContinuationEpochsLowered{
    "epoch_lowering", "NumContinuationEpochsLowered",
    "Number of continuation epochs lowered without WaitOnEpochOp"};
static llvm::Statistic numEmptyEpochsElided{
    "epoch_lowering", "NumEmptyEpochsElided", "Number of empty epochs elided"};
static llvm::Statistic numCpsAdvancesResolved{
    "epoch_lowering", "NumCpsAdvancesResolved",
    "Number of CPS advance placeholders resolved"};
static llvm::Statistic numEdtCreatesUpdatedWithEpoch{
    "epoch_lowering", "NumEdtCreatesUpdatedWithEpoch",
    "Number of EdtCreateOps updated with epoch GUID"};

using namespace mlir;
using namespace mlir::func;
using namespace mlir::arts;
using AttrNames::Operation::ContinuationForEpoch;
using AttrNames::Operation::CPSAdditiveParams;
using AttrNames::Operation::CPSAdvanceHasIvArg;
using AttrNames::Operation::CPSChainId;
using AttrNames::Operation::CPSDepRouting;
using AttrNames::Operation::CPSInitIter;
using AttrNames::Operation::CPSIterCounterParamIdx;
using AttrNames::Operation::CPSNumCarry;
using AttrNames::Operation::CPSOuterEpochParamIdx;
using AttrNames::Operation::CPSParamPerm;

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
      ++numEmptyEpochsElided;
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
        /// Control slot = depCount - 1 (EdtLowering added +1 for the control
        /// dep).
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

    numEdtCreatesUpdatedWithEpoch += edtCreatesToUpdate.size();
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
    if (auto initIterAttr = epochOp->getAttrOfType<IntegerAttr>(CPSInitIter))
      cpsInitIter = initIterAttr.getInt();

    if (hasContinuation)
      ++numContinuationEpochsLowered;
    else
      ++numEpochsLowered;

    /// Replace the epoch op with the epoch GUID.
    epochOp.replaceAllUsesWith(currentEpoch);
    epochOp.erase();

    /// For CPS outer epochs, inject the iteration counter AND outer epoch
    /// GUID into all continuation EdtCreateOps' param_packs so they
    /// propagate through the chain.
    if (isCPSOuterEpoch) {
      func::FuncOp parentFunc = createEpochOp->getParentOfType<func::FuncOp>();
      ARTS_INFO("CPS outer epoch: injecting iter counter + outer GUID "
                "into chain EDTs");
      if (parentFunc) {
        parentFunc.walk([&](EdtCreateOp edt) {
          if (!edt->hasAttr(CPSChainId)) {
            return;
          }
          ARTS_INFO("  Found chain EDT: " << edt);
          auto packOp = edt.getParamMemref().getDefiningOp<EdtParamPackOp>();
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
          edt->setAttr(CPSIterCounterParamIdx,
                       AC->getBuilder().getI64IntegerAttr(iterIdx));
          edt->setAttr(CPSOuterEpochParamIdx,
                       AC->getBuilder().getI64IntegerAttr(epochIdx));
          ARTS_INFO("  Injected iter counter at idx "
                    << iterIdx << ", outer epoch at idx " << epochIdx);
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
      StringAttr funcName;
      int64_t iterIdx;
      int64_t epochIdx;
    };
    SmallVector<ChainFuncInfo> worklist;
    DenseSet<StringAttr> queuedFuncs;

    auto enqueueChainFunc = [&](StringAttr funcName, int64_t iterIdx,
                                int64_t epochIdx) {
      if (!funcName || !queuedFuncs.insert(funcName).second)
        return;
      worklist.push_back({funcName, iterIdx, epochIdx});
    };

    module.walk([&](EdtCreateOp edt) {
      auto iterAttr = edt->getAttrOfType<IntegerAttr>(CPSIterCounterParamIdx);
      auto epochAttr = edt->getAttrOfType<IntegerAttr>(CPSOuterEpochParamIdx);
      auto funcAttr =
          edt->getAttrOfType<StringAttr>(AttrNames::Operation::OutlinedFunc);
      if (iterAttr && epochAttr && funcAttr)
        enqueueChainFunc(funcAttr, iterAttr.getInt(), epochAttr.getInt());
    });

    while (!worklist.empty()) {
      ChainFuncInfo info = worklist.pop_back_val();
      auto func = module.lookupSymbol<func::FuncOp>(info.funcName.getValue());
      if (!func)
        continue;

      SmallVector<EdtCreateOp> childChainEdts;
      func.walk([&](EdtCreateOp edt) {
        if (edt->hasAttr(CPSChainId) && !edt->hasAttr(CPSOuterEpochParamIdx))
          childChainEdts.push_back(edt);
      });
      if (childChainEdts.empty())
        continue;

      EdtParamUnpackOp unpackOp = nullptr;
      func.walk([&](EdtParamUnpackOp op) {
        if (!unpackOp)
          unpackOp = op;
      });

      Location loc = func.getLoc();
      Value paramArrayMemref;
      if (unpackOp) {
        AC->setInsertionPointAfter(unpackOp);
        paramArrayMemref = unpackOp->getOperand(0);
      } else if (func.getNumArguments() > 1) {
        Value candidate = func.getArgument(1);
        if (auto memrefType = dyn_cast<MemRefType>(candidate.getType());
            memrefType && memrefType.getElementType().isInteger(64)) {
          AC->setInsertionPointToStart(&func.front());
          paramArrayMemref = candidate;
        }
      }
      if (!paramArrayMemref)
        continue;

      /// Load iter counter from parent's param array.
      Value iterIdxVal = AC->createIndexConstant(info.iterIdx, loc);
      Value iterCounter = AC->create<memref::LoadOp>(loc, paramArrayMemref,
                                                     ValueRange{iterIdxVal});
      /// Load outer epoch GUID from parent's param array.
      Value epochIdxVal = AC->createIndexConstant(info.epochIdx, loc);
      Value outerEpochGuid = AC->create<memref::LoadOp>(
          loc, paramArrayMemref, ValueRange{epochIdxVal});

      ARTS_INFO("CPS propagation: " << info.funcName.getValue()
                                    << " loading iter[" << info.iterIdx
                                    << "], epoch[" << info.epochIdx << "]");

      /// Build a map from parent function's unpack results to their indices.
      DenseMap<Value, int64_t> unpackResultToIdx;
      if (unpackOp)
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
                if (auto idxOp = loadOp.getIndices()[0]
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
        edt->setAttr(CPSParamPerm, AC->getBuilder().getDenseI64ArrayAttr(perm));

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
        edt->setAttr(CPSIterCounterParamIdx,
                     AC->getBuilder().getI64IntegerAttr(newIterIdx));
        edt->setAttr(CPSOuterEpochParamIdx,
                     AC->getBuilder().getI64IntegerAttr(newEpochIdx));
        ARTS_INFO("  Injected iter[" << newIterIdx << "], epoch[" << newEpochIdx
                                     << "] for child chain EDT");

        if (auto childFuncAttr = edt->getAttrOfType<StringAttr>(
                AttrNames::Operation::OutlinedFunc))
          enqueueChainFunc(childFuncAttr, newIterIdx, newEpochIdx);
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
      /// Fall back to module-wide search (target may be in a different
      /// function).
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

    auto outlinedFunc = contEdtCreate->getAttrOfType<StringAttr>(
        AttrNames::Operation::OutlinedFunc);
    if (!outlinedFunc) {
      advanceOp.emitError("CPS advance: continuation has no outlined_func");
      return signalPassFailure();
    }

    ARTS_INFO("CPS advance: resolving chain '" << targetChainId << "' → "
                                               << outlinedFunc.getValue());

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
    /// IMPORTANT: Read CPS indices from the LOCAL function's own EdtCreateOp,
    /// not from the target chain's EdtCreateOp. Each chain function has its
    /// own CPS param indices based on its own param count.
    Value outerEpochGuid;
    Value curIter;
    IntegerAttr localOuterEpochIdxAttr;
    IntegerAttr localIterCounterIdxAttr;
    DenseI64ArrayAttr localParamPermAttr;
    if (parentFunc) {
      module.walk([&](EdtCreateOp edt) {
        auto funcAttr =
            edt->getAttrOfType<StringAttr>(AttrNames::Operation::OutlinedFunc);
        if (funcAttr && funcAttr.getValue() == parentFunc.getName()) {
          localIterCounterIdxAttr =
              edt->getAttrOfType<IntegerAttr>(CPSIterCounterParamIdx);
          localOuterEpochIdxAttr =
              edt->getAttrOfType<IntegerAttr>(CPSOuterEpochParamIdx);
          localParamPermAttr = edt->getAttrOfType<DenseI64ArrayAttr>(CPSParamPerm);
        }
      });
    }
    IntegerAttr targetIterCounterIdxAttr =
        contEdtCreate->getAttrOfType<IntegerAttr>(CPSIterCounterParamIdx);
    IntegerAttr targetOuterEpochIdxAttr =
        contEdtCreate->getAttrOfType<IntegerAttr>(CPSOuterEpochParamIdx);
    DenseI64ArrayAttr targetParamPermAttr =
        contEdtCreate->getAttrOfType<DenseI64ArrayAttr>(CPSParamPerm);
    if (!localIterCounterIdxAttr)
      localIterCounterIdxAttr = targetIterCounterIdxAttr;
    if (!localOuterEpochIdxAttr)
      localOuterEpochIdxAttr = targetOuterEpochIdxAttr;
    if (localUnpack) {
      Value paramArrayMemref = localUnpack->getOperand(0);
      if (localIterCounterIdxAttr) {
        unsigned idx = localIterCounterIdxAttr.getInt();
        Value idxVal = AC->createIndexConstant(idx, loc);
        curIter = AC->create<memref::LoadOp>(loc, paramArrayMemref,
                                             ValueRange{idxVal});
      }
      if (localOuterEpochIdxAttr) {
        unsigned idx = localOuterEpochIdxAttr.getInt();
        Value idxVal = AC->createIndexConstant(idx, loc);
        outerEpochGuid = AC->create<memref::LoadOp>(loc, paramArrayMemref,
                                                    ValueRange{idxVal});
      }
    }

    bool isAdditive = advanceOp->hasAttr(CPSAdditiveParams);

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
      Value cond =
          AC->create<arith::CmpIOp>(loc, arith::CmpIPredicate::slt, tNext, ub);
      auto ifOp = AC->create<scf::IfOp>(loc, cond, /*withElse=*/false);
      Block &thenBlock = ifOp.getThenRegion().front();
      AC->setInsertionPoint(thenBlock.getTerminator());
    }

    auto materializeLoopBackParam = [&](Value value) -> Value {
      if (!isa<MemRefType>(value.getType()))
        return value;
      /// Convert memref DB handles to raw i64 for paramv transport.
      Value rawPtr = AC->castToLLVMPtr(value, loc);
      return AC->create<LLVM::PtrToIntOp>(loc, AC->Int64, rawPtr);
    };

    bool hasCanonicalLoopBackContract = advanceOp->hasAttr(CPSNumCarry);

    /// Build the self-recreation param pack using the canonical carry
    /// params from CPSAdvanceOp. EpochOpt step CPS-8 ensures carry params
    /// are in the EXACT same order as EdtLowering::packParams, so we can
    /// use them directly. materializeLoopBackParam converts memref
    /// dbHandles to raw i64, matching packParams' conversion.
    SmallVector<Value> canonicalLoopBackParams;
    if (auto numCarryAttr =
            advanceOp->getAttrOfType<IntegerAttr>(CPSNumCarry)) {
      unsigned paramStart = (isAdditive && nextParams.size() >= 2) ? 2 : 0;
      unsigned available =
          nextParams.size() > paramStart ? nextParams.size() - paramStart : 0;
      unsigned carryCount =
          std::min<unsigned>(numCarryAttr.getInt(), available);
      canonicalLoopBackParams.reserve(carryCount);
      for (unsigned i = 0; i < carryCount; ++i)
        canonicalLoopBackParams.push_back(
            materializeLoopBackParam(nextParams[paramStart + i]));
    }

    auto rebuildCpsPackToTargetSchema = [&]() -> SmallVector<Value> {
      SmallVector<Value> newPackParams;
      if (!hasCanonicalLoopBackContract)
        return newPackParams;

      unsigned targetCarryCount = canonicalLoopBackParams.size();
      if (targetParamPermAttr)
        targetCarryCount = targetParamPermAttr.size();
      else if (targetIterCounterIdxAttr)
        targetCarryCount = targetIterCounterIdxAttr.getInt();
      else if (targetOuterEpochIdxAttr)
        targetCarryCount = targetOuterEpochIdxAttr.getInt();

      unsigned totalPackSlots = targetCarryCount;
      if (targetIterCounterIdxAttr)
        totalPackSlots =
            std::max<unsigned>(totalPackSlots,
                               targetIterCounterIdxAttr.getInt() + 1);
      if (targetOuterEpochIdxAttr)
        totalPackSlots =
            std::max<unsigned>(totalPackSlots,
                               targetOuterEpochIdxAttr.getInt() + 1);
      if (totalPackSlots == 0)
        return newPackParams;

      auto resolveCanonicalIndex = [](ArrayRef<int64_t> perm,
                                      unsigned slot) -> int64_t {
        if (slot < perm.size() && perm[slot] >= 0)
          return perm[slot];
        return slot;
      };

      int64_t canonicalWidth = canonicalLoopBackParams.size();
      auto growCanonicalWidth = [&](ArrayRef<int64_t> perm) {
        for (int64_t idx : perm) {
          if (idx >= 0)
            canonicalWidth = std::max<int64_t>(canonicalWidth, idx + 1);
        }
      };
      if (localParamPermAttr)
        growCanonicalWidth(localParamPermAttr.asArrayRef());
      if (targetParamPermAttr)
        growCanonicalWidth(targetParamPermAttr.asArrayRef());

      SmallVector<Value> canonicalValues(std::max<int64_t>(canonicalWidth, 0));
      ArrayRef<int64_t> localPerm =
          localParamPermAttr ? localParamPermAttr.asArrayRef()
                             : ArrayRef<int64_t>();
      ArrayRef<int64_t> targetPerm =
          targetParamPermAttr ? targetParamPermAttr.asArrayRef()
                              : ArrayRef<int64_t>();
      for (unsigned i = 0; i < canonicalLoopBackParams.size(); ++i) {
        int64_t canonicalIdx = resolveCanonicalIndex(localPerm, i);
        if (canonicalIdx < 0)
          continue;
        if (static_cast<size_t>(canonicalIdx) >= canonicalValues.size())
          canonicalValues.resize(canonicalIdx + 1);
        canonicalValues[canonicalIdx] = canonicalLoopBackParams[i];
      }

      Value zeroI64 = AC->createIntConstant(0, AC->Int64, loc);
      newPackParams.assign(totalPackSlots, zeroI64);

      unsigned missingCarrySlots = 0;
      for (unsigned slot = 0; slot < targetCarryCount; ++slot) {
        int64_t canonicalIdx = resolveCanonicalIndex(targetPerm, slot);
        if (canonicalIdx >= 0 &&
            static_cast<size_t>(canonicalIdx) < canonicalValues.size() &&
            canonicalValues[canonicalIdx]) {
          newPackParams[slot] = canonicalValues[canonicalIdx];
          continue;
        }
        if (!targetParamPermAttr && slot < canonicalLoopBackParams.size()) {
          newPackParams[slot] = canonicalLoopBackParams[slot];
          continue;
        }
        ++missingCarrySlots;
      }

      Value iterValue = tNext ? tNext : curIter;
      if (targetIterCounterIdxAttr && iterValue) {
        unsigned iterIdx = targetIterCounterIdxAttr.getInt();
        if (iterIdx < newPackParams.size())
          newPackParams[iterIdx] = iterValue;
      }
      if (targetOuterEpochIdxAttr && outerEpochGuid) {
        unsigned epochIdx = targetOuterEpochIdxAttr.getInt();
        if (epochIdx < newPackParams.size())
          newPackParams[epochIdx] = outerEpochGuid;
      }

      if (missingCarrySlots > 0) {
        ARTS_WARN("CPS advance: rebuilt continuation pack for "
                  << outlinedFunc.getValue() << " with " << missingCarrySlots
                  << " schema hole(s); preserving slot count with zero fill");
      }

      return newPackParams;
    };

    Value paramMemref;
    SmallVector<Value> schemaPackParams = rebuildCpsPackToTargetSchema();
    if (!schemaPackParams.empty()) {
      auto packType = MemRefType::get({ShapedType::kDynamic}, AC->Int64);
      paramMemref = AC->create<EdtParamPackOp>(loc, packType, schemaPackParams)
                         .getMemref();
    } else if (localUnpack) {
      auto unpackResults = localUnpack.getResults();
      SmallVector<Value> newPackParams;
      newPackParams.reserve(unpackResults.size() + 2);
      for (Value unpacked : unpackResults)
        newPackParams.push_back(unpacked);
      if (tNext)
        newPackParams.push_back(tNext);
      else if (curIter)
        newPackParams.push_back(curIter);
      if (outerEpochGuid)
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
    if (auto chainIdAttr = contEdtCreate->getAttrOfType<StringAttr>(CPSChainId))
      newContEdt->setAttr(CPSChainId, chainIdAttr);
    if (targetIterCounterIdxAttr)
      newContEdt->setAttr(CPSIterCounterParamIdx, targetIterCounterIdxAttr);
    if (targetOuterEpochIdxAttr)
      newContEdt->setAttr(CPSOuterEpochParamIdx, targetOuterEpochIdxAttr);
    if (targetParamPermAttr)
      newContEdt->setAttr(CPSParamPerm, targetParamPermAttr);

    Value newContGuid = newContEdt.getGuid();

    auto emitWholeDbDep = [&](Value depGuid, unsigned slotIdx, int32_t mode,
                              std::optional<int32_t> depFlags) {
      if (!depGuid)
        return;
      depGuid = AC->ensureI64(depGuid, loc);
      Value slot = AC->createIntConstant(slotIdx, AC->Int32, loc);
      Value modeVal = AC->createIntConstant(mode, AC->Int32, loc);
      ArtsCodegen::RuntimeCallBuilder RCB(*AC, loc);
      if (depFlags && *depFlags != 0) {
        Value flagsVal = AC->createIntConstant(*depFlags, AC->Int32, loc);
        RCB.callVoid(types::ARTSRTL_arts_add_dependence_ex,
                     {depGuid, newContGuid, slot, modeVal, flagsVal});
      } else {
        RCB.callVoid(types::ARTSRTL_arts_add_dependence,
                     {depGuid, newContGuid, slot, modeVal});
      }
    };

    auto findOriginalRecDep = [&](EdtCreateOp edt) -> RecordDepOp {
      for (Operation *user : edt.getGuid().getUsers())
        if (auto recordDep = dyn_cast<RecordDepOp>(user))
          if (recordDep.getEdtGuid() == edt.getGuid())
            return recordDep;
      return nullptr;
    };

    auto getLoopBackValueForPackIndex = [&](unsigned packIdx) -> Value {
      if (packIdx < canonicalLoopBackParams.size())
        return canonicalLoopBackParams[packIdx];
      if (localUnpack && packIdx < localUnpack.getNumResults())
        return localUnpack.getResult(packIdx);
      return Value();
    };

    auto findPackIndexForSafeWholeDbDep =
        [&](EdtParamPackOp packOp, Value depGuid) -> std::optional<unsigned> {
      if (!packOp || !depGuid)
        return std::nullopt;

      SmallVector<std::pair<Value, Value>> candidateSources;
      candidateSources.push_back({depGuid, Value()});

      if (auto depAcquire = depGuid.getDefiningOp<DbAcquireOp>()) {
        if (depAcquire.getSizes().size() == 1 &&
            depAcquire.getOffsets().size() == 1) {
          if (auto foldedSize =
                  ValueAnalysis::tryFoldConstantIndex(depAcquire.getSizes()[0]);
              foldedSize && *foldedSize == 1) {
            candidateSources.push_back(
                {depAcquire.getSourceGuid(), depAcquire.getOffsets()[0]});
          }
        }
      }

      auto sameIndex = [](Value lhs, Value rhs) {
        return ValueAnalysis::stripNumericCasts(lhs) ==
               ValueAnalysis::stripNumericCasts(rhs);
      };

      for (auto [idx, operand] : llvm::enumerate(packOp.getOperands())) {
        for (auto [sourceGuid, offset] : candidateSources) {
          if (operand == sourceGuid && !offset)
            return idx;
          auto loadOp = operand.getDefiningOp<memref::LoadOp>();
          if (!loadOp || loadOp.getIndices().size() != 1)
            continue;
          if (loadOp.getMemRef() != sourceGuid)
            continue;
          if (!offset) {
            if (ValueAnalysis::isZeroConstant(
                    ValueAnalysis::stripNumericCasts(loadOp.getIndices()[0])))
              return idx;
            continue;
          }
          if (sameIndex(loadOp.getIndices()[0], offset))
            return idx;
        }
      }

      return std::nullopt;
    };

    /// Replicate non-control deps from the initial creation.
    /// The dep GUIDs are at explicit indices in the carry params, recorded
    /// by EpochOpt step CPS-8 in the routing attribute.
    /// Format: [numTimingDbs, hasScratch, guidIdx0, guidIdx1, ...]
    /// Dep slots: timing DBs at slots 0..T-1, scratch at slot T.
    if (auto depRouting =
            advanceOp->getAttrOfType<DenseI64ArrayAttr>(CPSDepRouting)) {
      ArrayRef<int64_t> routing = depRouting.asArrayRef();
      int64_t numTimingDbs = routing.size() > 0 ? routing[0] : 0;
      int64_t hasScratch = routing.size() > 1 ? routing[1] : 0;
      unsigned paramStart = (isAdditive && nextParams.size() >= 2) ? 2 : 0;
      unsigned guidDataStart = 2; /// Index offsets start at routing[2].

      /// Scratch DB → dep slot after all timing DBs (mode=read=1).
      if (hasScratch && guidDataStart < routing.size()) {
        int64_t carryIdx = routing[guidDataStart];
        if (carryIdx >= 0 && paramStart + carryIdx < nextParams.size()) {
          Value scratchGuid = nextParams[paramStart + carryIdx];
          Value scratchSlot =
              AC->createIntConstant(numTimingDbs, AC->Int32, loc);
          Value modeRead = AC->createIntConstant(1, AC->Int32, loc);
          AC->createRuntimeCall(
              types::ARTSRTL_arts_add_dependence,
              {scratchGuid, newContGuid, scratchSlot, modeRead}, loc);
        }
        ++guidDataStart;
      }

      /// Timing DBs → dep slots 0..T-1 (mode=write=2, i.e. inout).
      for (int64_t t = 0; t < numTimingDbs; ++t) {
        unsigned routingIdx = guidDataStart + t;
        if (routingIdx >= routing.size())
          break;
        int64_t carryIdx = routing[routingIdx];
        if (carryIdx < 0 || paramStart + carryIdx >= nextParams.size())
          continue;
        Value timingGuid = nextParams[paramStart + carryIdx];
        Value slot = AC->createIntConstant(t, AC->Int32, loc);
        Value modeWrite = AC->createIntConstant(2, AC->Int32, loc);
        AC->createRuntimeCall(types::ARTSRTL_arts_add_dependence,
                              {timingGuid, newContGuid, slot, modeWrite}, loc);
      }
    } else if (auto originalRecDep = findOriginalRecDep(contEdtCreate)) {
      auto originalPack =
          contEdtCreate.getParamMemref().getDefiningOp<EdtParamPackOp>();
      ArrayRef<int32_t> acquireModes =
          originalRecDep.getAcquireModes()
              ? ArrayRef<int32_t>(*originalRecDep.getAcquireModes())
              : ArrayRef<int32_t>{};
      ArrayRef<int32_t> depFlags =
          originalRecDep.getDepFlags()
              ? ArrayRef<int32_t>(*originalRecDep.getDepFlags())
              : ArrayRef<int32_t>{};

      bool hasExplicitSlices = !originalRecDep.getByteOffsets().empty() ||
                               !originalRecDep.getByteSizes().empty();
      bool hasGuardedDeps =
          llvm::any_of(originalRecDep.getBoundsValids(), [](Value boundsValid) {
            return boundsValid &&
                   !ValueAnalysis::isOneConstant(
                       ValueAnalysis::stripNumericCasts(boundsValid));
          });

      if (!hasExplicitSlices && !hasGuardedDeps && originalPack) {
        for (auto [depIdx, depGuid] : llvm::enumerate(originalRecDep.getDatablocks())) {
          auto packIdx = findPackIndexForSafeWholeDbDep(originalPack, depGuid);
          if (!packIdx)
            continue;
          Value localDepGuid = getLoopBackValueForPackIndex(*packIdx);
          if (!localDepGuid)
            continue;
          int32_t mode = depIdx < acquireModes.size() ? acquireModes[depIdx] : 2;
          std::optional<int32_t> flags = std::nullopt;
          if (depIdx < depFlags.size())
            flags = depFlags[depIdx];
          emitWholeDbDep(localDepGuid, depIdx, mode, flags);
        }
      }
    }

    /// Finish slot = depCount - 1.
    Value one = AC->createIntConstant(1, AC->Int32, loc);
    Value finishSlot = AC->create<arith::SubIOp>(loc, localDepCount, one);

    /// Create epoch with finish target pointing to the new continuation.
    auto innerEpoch = AC->create<CreateEpochOp>(
        loc, IntegerType::get(AC->getContext(), 64), newContGuid, finishSlot);
    Value epochGuid = innerEpoch.getEpochGuid();

    /// If the advance region has a block argument serving as the IV
    /// placeholder (from wrappingIf support), replace it with the actual
    /// runtime iteration counter (tNext or curIter).
    Block &advBody = advanceOp.getEpochBody().front();
    if (advanceOp->hasAttr(CPSAdvanceHasIvArg) &&
        advBody.getNumArguments() > 0) {
      Value ivReplacement = tNext ? tNext : curIter;
      if (ivReplacement) {
        // Cast to index if needed (block arg is index type).
        if (ivReplacement.getType() != advBody.getArgument(0).getType()) {
          AC->setInsertionPointAfter(ivReplacement.getDefiningOp()
                                         ? ivReplacement.getDefiningOp()
                                         : &advBody.front());
          ivReplacement = AC->create<arith::IndexCastOp>(
              loc, advBody.getArgument(0).getType(), ivReplacement);
        }
        advBody.getArgument(0).replaceAllUsesWith(ivReplacement);
      }
      advBody.eraseArgument(0);
    }

    /// Move worker ops from CPSAdvanceOp's epochBody AFTER the epoch creation.
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
    ++numCpsAdvancesResolved;
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
