///==========================================================================///
/// File: GuidRangCallOpt.cpp
///
/// Rewrites per-iteration GUID reservation calls into GUID-range calls after
/// ARTS-to-LLVM lowering, when safe:
///
///   scf.for %i = 0 to N step 1 {
///     %g = call @arts_guid_reserve(type, route0)
///     ...
///   }
///
/// becomes:
///
///   %range = call @arts_guid_reserve_range(type, N, route0)
///   scf.for %i = 0 to N step 1 {
///     %g = call @arts_guid_from_index(%range, %i32)
///     ...
///   }
///
/// This pass is intentionally conservative and only applies when:
/// - lower bound is 0 and step is 1,
/// - exactly one @arts_guid_reserve call appears in the loop body,
/// - route operand is compile-time constant 0.
///
/// For dynamic trip counts, it emits a guarded rewrite:
/// - if (upperBound <= UINT32_MAX): use reserve_range/from_index
/// - else: keep per-iteration reserve semantics
///==========================================================================///

#include "arts/analysis/value/ValueAnalysis.h"
#include "arts/codegen/Codegen.h"
#include "arts/codegen/Types.h"
#define GEN_PASS_DEF_GUIDRANGCALLOPT
#include "arts/Dialect.h"
#include "arts/passes/Passes.h"
#include "mlir/Pass/Pass.h"
#include "arts/passes/Passes.h.inc"
#include "arts/passes/Passes.h"
#include "arts/utils/Debug.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/BuiltinOps.h"
#include <cstdint>
#include <limits>

ARTS_DEBUG_SETUP(guid_rang_call_opt);

using namespace mlir;
using namespace mlir::arts;

namespace {

static bool isCanonicalZeroBasedUnitStepLoop(scf::ForOp loop) {
  auto lb = ValueAnalysis::tryFoldConstantIndex(loop.getLowerBound());
  auto step = ValueAnalysis::tryFoldConstantIndex(loop.getStep());
  return lb && step && *lb == 0 && *step == 1;
}

/// Returns static trip count when it fits into uint32_t, otherwise nullopt.
static std::optional<int64_t> getStaticTripCountU32(scf::ForOp loop) {
  auto ub = ValueAnalysis::tryFoldConstantIndex(loop.getUpperBound());
  if (!ub || *ub < 0)
    return std::nullopt;
  if (*ub > static_cast<int64_t>(std::numeric_limits<uint32_t>::max()))
    return std::nullopt;
  return *ub;
}

static bool isGuidReserveCall(func::CallOp call) {
  if (call.getNumOperands() != 2 || call.getNumResults() != 1)
    return false;
  std::optional<types::RuntimeFunction> fn =
      types::runtimeFunctionFromName(call.getCallee());
  return fn && *fn == types::ARTSRTL_arts_guid_reserve;
}

struct GuidRangCallOptPass
    : public impl::GuidRangCallOptBase<GuidRangCallOptPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    ArtsCodegen codegen(module, /*debug=*/false);
    MLIRContext *ctx = module.getContext();

    Type i32 = IntegerType::get(ctx, 32);
    Type i64 = IntegerType::get(ctx, 64);
    func::FuncOp reserveFn =
        codegen.getOrCreateRuntimeFunction(types::ARTSRTL_arts_guid_reserve);
    func::FuncOp reserveRangeFn = codegen.getOrCreateRuntimeFunction(
        types::ARTSRTL_arts_guid_reserve_range);
    func::FuncOp fromIndexFn =
        codegen.getOrCreateRuntimeFunction(types::ARTSRTL_arts_guid_from_index);

    SmallVector<scf::ForOp> loops;
    module.walk([&](scf::ForOp loop) { loops.push_back(loop); });

    int rewrittenStatic = 0;
    int rewrittenGuarded = 0;
    for (scf::ForOp loop : loops) {
      if (!isCanonicalZeroBasedUnitStepLoop(loop))
        continue;

      SmallVector<func::CallOp> reserveCalls;
      for (Operation &op : loop.getBody()->without_terminator()) {
        if (auto call = dyn_cast<func::CallOp>(op))
          if (isGuidReserveCall(call))
            reserveCalls.push_back(call);
      }

      if (reserveCalls.empty())
        continue;

      for (func::CallOp reserveCall : reserveCalls) {
        auto routeConst =
            ValueAnalysis::tryFoldConstantIndex(reserveCall.getOperand(1));
        if (!routeConst || *routeConst != 0)
          continue;

        auto staticTripCount = getStaticTripCountU32(loop);
        if (staticTripCount && *staticTripCount <= 1)
          continue;

        /// Fast path: fully static trip count.
        if (staticTripCount && *staticTripCount > 1) {
          OpBuilder b(loop);
          Value tripCountI32 = b.create<arith::ConstantIntOp>(
              loop.getLoc(), *staticTripCount, 32);
          auto reserveRange = b.create<func::CallOp>(
              loop.getLoc(), reserveRangeFn,
              ValueRange{reserveCall.getOperand(0), tripCountI32,
                         reserveCall.getOperand(1)});
          Value rangeGuid = reserveRange.getResult(0);

          OpBuilder callBuilder(reserveCall);
          Value ivI32 = callBuilder.create<arith::IndexCastOp>(
              reserveCall.getLoc(), i32, loop.getInductionVar());
          auto fromIndex = callBuilder.create<func::CallOp>(
              reserveCall.getLoc(), fromIndexFn,
              ValueRange{rangeGuid, ivI32});
          reserveCall.getResult(0).replaceAllUsesWith(fromIndex.getResult(0));
          reserveCall.erase();
          ++rewrittenStatic;
          continue;
        }

        /// Guarded path for dynamic trip counts:
        /// preserve exact semantics when upper bound exceeds uint32 range.
        OpBuilder b(loop);
        Location loc = loop.getLoc();
        Value ubI64 =
            b.create<arith::IndexCastUIOp>(loc, i64, loop.getUpperBound());
        Value oneI64 = b.create<arith::ConstantIntOp>(loc, 1, 64);
        Value u32Max = b.create<arith::ConstantIntOp>(
            loc, std::numeric_limits<uint32_t>::max(), 64);
        Value fitsU32 = b.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::ule, ubI64, u32Max);
        Value moreThanOne = b.create<arith::CmpIOp>(
            loc, arith::CmpIPredicate::ugt, ubI64, oneI64);
        Value canUseRange =
            b.create<arith::AndIOp>(loc, fitsU32, moreThanOne);

        auto chooseRangeGuid = b.create<scf::IfOp>(loc, TypeRange{i64},
                                                   canUseRange,
                                                   /*withElseRegion=*/true);
        {
          OpBuilder thenBuilder =
              OpBuilder::atBlockBegin(&chooseRangeGuid.getThenRegion().front());
          Value tripCountI32 = thenBuilder.create<arith::IndexCastOp>(
              loc, i32, loop.getUpperBound());
          auto reserveRange = thenBuilder.create<func::CallOp>(
              loc, reserveRangeFn,
              ValueRange{reserveCall.getOperand(0), tripCountI32,
                         reserveCall.getOperand(1)});
          thenBuilder.create<scf::YieldOp>(loc, reserveRange.getResult(0));
        }
        {
          OpBuilder elseBuilder =
              OpBuilder::atBlockBegin(&chooseRangeGuid.getElseRegion().front());
          Value nullGuid = elseBuilder.create<arith::ConstantIntOp>(loc, 0, 64);
          elseBuilder.create<scf::YieldOp>(loc, nullGuid);
        }
        Value rangeGuid = chooseRangeGuid.getResult(0);

        OpBuilder callBuilder(reserveCall);
        auto chooseGuid = callBuilder.create<scf::IfOp>(
            reserveCall.getLoc(), TypeRange{i64}, canUseRange,
            /*withElseRegion=*/true);
        {
          OpBuilder thenBuilder =
              OpBuilder::atBlockBegin(&chooseGuid.getThenRegion().front());
          Value ivI32 = thenBuilder.create<arith::IndexCastOp>(
              reserveCall.getLoc(), i32, loop.getInductionVar());
          auto fromIndex = thenBuilder.create<func::CallOp>(
              reserveCall.getLoc(), fromIndexFn,
              ValueRange{rangeGuid, ivI32});
          thenBuilder.create<scf::YieldOp>(reserveCall.getLoc(),
                                           fromIndex.getResult(0));
        }
        {
          OpBuilder elseBuilder =
              OpBuilder::atBlockBegin(&chooseGuid.getElseRegion().front());
          auto reserve = elseBuilder.create<func::CallOp>(
              reserveCall.getLoc(), reserveFn,
              reserveCall.getOperands());
          elseBuilder.create<scf::YieldOp>(reserveCall.getLoc(),
                                           reserve.getResult(0));
        }

        reserveCall.getResult(0).replaceAllUsesWith(chooseGuid.getResult(0));
        reserveCall.erase();
        ++rewrittenGuarded;
      }
    }

    ARTS_INFO("GuidRangCallOpt: rewritten " << rewrittenStatic
                                            << " static loops and "
                                            << rewrittenGuarded
                                            << " guarded dynamic loops");
  }
};

} // namespace

namespace mlir {
namespace arts {

std::unique_ptr<Pass> createGuidRangCallOptPass() {
  return std::make_unique<GuidRangCallOptPass>();
}

std::unique_ptr<Pass> createGuidRangeCallOptimizationPass() {
  return createGuidRangCallOptPass();
}

} // namespace arts
} // namespace mlir
