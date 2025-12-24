///==========================================================================///
/// File: AliasScopeGen.cpp
///
/// This pass generates LLVM alias scope metadata for ARTS data arrays to enable
/// LLVM optimizations like vectorization. Without this metadata, LLVM cannot
/// prove that distinct data arrays don't overlap.
///
/// Before (LLVM cannot prove arrays don't alias - no vectorization):
///   %ptr_A = llvm.load %deps[0].ptr : !llvm.ptr
///   %ptr_B = llvm.load %deps[1].ptr : !llvm.ptr
///   scf.for %i = ... {
///     %a = llvm.load %ptr_A[%i] : f64
///     %b = llvm.load %ptr_B[%i] : f64
///     llvm.store %result, %ptr_A[%i]  // May alias with %ptr_B - blocks opt!
///   }
///
/// After (distinct alias scopes enable vectorization):
///   llvm.experimental.noalias.scope.decl %scope_A
///   llvm.experimental.noalias.scope.decl %scope_B
///   %ptr_A = llvm.load %deps[0].ptr : !llvm.ptr
///   %ptr_B = llvm.load %deps[1].ptr : !llvm.ptr
///   scf.for %i = ... {
///     %a = llvm.load %ptr_A[%i] {alias_scopes=[@scope_A]}
///     %b = llvm.load %ptr_B[%i] {alias_scopes=[@scope_B]}
///     llvm.store %result, %ptr_A[%i] {alias_scopes=[@scope_A]}
///   }
///
/// The pass identifies data pointer loads from the deps struct and attaches
/// alias scope metadata to all memory accesses using those pointers. Each
/// data array gets a unique scope, and accesses are marked as not aliasing
/// with other arrays.
///
/// This pass must emit llvm.experimental.noalias.scope.decl intrinsics
/// at function entry for LLVM to recognize the alias scopes.
///==========================================================================///

#include "../ArtsPassDetails.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMInterfaces.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "polygeist/Ops.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"
#include "arts/Utils/ValueUtils.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_alias_scope_gen);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Maximum number of arrays for alias scope generation (following Polly's
/// limit) Alias scope metadata grows quadratically with number of arrays.
constexpr unsigned MaxArraysForAliasScopes = 10;

/// Information about a data pointer loaded from the deps struct
struct DataPointerInfo {
  LLVM::LoadOp loadOp;        // The load op that loads the data pointer
  Value ptr;                  // The loaded pointer value
  int64_t depIndex;           // Index into deps array (for naming)
  LLVM::AliasScopeAttr scope; // Assigned alias scope
};

static void
addAlignmentToDataPointerLoads(SmallVectorImpl<DataPointerInfo> &dataPointers) {
  for (auto &info : dataPointers) {
    if (!info.loadOp.getAlignment()) {
      info.loadOp.setAlignment(8);
      ARTS_DEBUG_TYPE("Set alignment=8 on data pointer load for array_"
                      << info.depIndex);
    }
  }
}

/// Collect data pointer loads from EDT function
static SmallVector<DataPointerInfo>
collectDataPointerLoads(LLVM::LLVMFuncOp funcOp) {
  SmallVector<DataPointerInfo> result;
  int64_t index = 0;

  funcOp.walk([&](LLVM::LoadOp loadOp) {
    if (!isa<LLVM::LLVMPointerType>(loadOp.getResult().getType()))
      return;

    Value addr = loadOp.getAddr();

    if (auto defOp = addr.getDefiningOp()) {
      StringRef opName = defOp->getName().getStringRef();
      if (opName == "arts.dep_gep" || opName == "arts.db_gep") {
        result.push_back({loadOp, loadOp.getResult(), index++, {}});
        return;
      }

      if (auto gepOp = dyn_cast<LLVM::GEPOp>(defOp)) {
        if (auto structType =
                dyn_cast<LLVM::LLVMStructType>(gepOp.getSourceElementType()))
          result.push_back({loadOp, loadOp.getResult(), index++, {}});
      }
    }
  });

  return result;
}

/// Create alias scope domain and scopes for data pointers
static void createAliasScopes(MLIRContext *ctx,
                              SmallVectorImpl<DataPointerInfo> &dataPointers) {
  if (dataPointers.empty())
    return;

  auto domain = LLVM::AliasScopeDomainAttr::get(
      ctx, DistinctAttr::create(UnitAttr::get(ctx)),
      StringAttr::get(ctx, "ARTS_data_arrays"));

  for (auto &info : dataPointers) {
    std::string scopeDesc = "array_" + std::to_string(info.depIndex);
    info.scope =
        LLVM::AliasScopeAttr::get(ctx, DistinctAttr::create(UnitAttr::get(ctx)),
                                  domain, StringAttr::get(ctx, scopeDesc));
  }
}

/// Emit llvm.experimental.noalias.scope.decl intrinsics at function entry.
static void
emitScopeDeclarations(LLVM::LLVMFuncOp funcOp,
                      SmallVectorImpl<DataPointerInfo> &dataPointers) {
  if (dataPointers.empty() || funcOp.getBody().empty())
    return;

  OpBuilder builder(funcOp.getBody().front().front().getContext());
  builder.setInsertionPointToStart(&funcOp.getBody().front());

  for (auto &info : dataPointers) {
    builder.create<LLVM::NoAliasScopeDeclOp>(funcOp.getLoc(), info.scope);
    ARTS_DEBUG_TYPE("Emitted noalias scope decl for array_" << info.depIndex);
  }
}

/// Find which data pointer (if any) a memory access address is derived from
static DataPointerInfo *
findSourceDataPointer(Value addr,
                      SmallVectorImpl<DataPointerInfo> &dataPointers) {
  for (auto &info : dataPointers) {
    if (ValueUtils::isDerivedFromPtr(addr, info.ptr))
      return &info;
  }
  return nullptr;
}

static void attachAliasScopes(Operation *op, DataPointerInfo &source,
                              SmallVectorImpl<DataPointerInfo> &allPointers) {
  MLIRContext *ctx = op->getContext();

  SmallVector<Attribute> aliasScopes = {source.scope};
  auto aliasScopesAttr = ArrayAttr::get(ctx, aliasScopes);

  SmallVector<Attribute> noaliasScopes;
  for (auto &info : allPointers) {
    if (&info != &source)
      noaliasScopes.push_back(info.scope);
  }
  ArrayAttr noaliasScopesAttr =
      noaliasScopes.empty() ? nullptr : ArrayAttr::get(ctx, noaliasScopes);

  if (auto loadOp = dyn_cast<LLVM::LoadOp>(op)) {
    loadOp.setAliasScopesAttr(aliasScopesAttr);
    if (noaliasScopesAttr)
      loadOp.setNoaliasScopesAttr(noaliasScopesAttr);
  } else if (auto storeOp = dyn_cast<LLVM::StoreOp>(op)) {
    storeOp.setAliasScopesAttr(aliasScopesAttr);
    if (noaliasScopesAttr)
      storeOp.setNoaliasScopesAttr(noaliasScopesAttr);
  } else {
    op->setAttr("alias_scopes", aliasScopesAttr);
    if (noaliasScopesAttr)
      op->setAttr("noalias_scopes", noaliasScopesAttr);
  }
}

/// Process memory accesses in the function.
static int
processMemoryAccesses(LLVM::LLVMFuncOp funcOp,
                      SmallVectorImpl<DataPointerInfo> &dataPointers) {
  int count = 0;

  /// Build set of data pointer values to skip when processing.
  DenseSet<Value> dataPointerValues;
  for (auto &info : dataPointers) {
    dataPointerValues.insert(info.ptr);
  }

  funcOp.walk([&](Operation *op) {
    Value addr;

    if (auto loadOp = dyn_cast<LLVM::LoadOp>(op)) {
      if (dataPointerValues.contains(loadOp.getResult()))
        return;
      addr = loadOp.getAddr();
    } else if (auto storeOp = dyn_cast<LLVM::StoreOp>(op)) {
      addr = storeOp.getAddr();
    } else if (auto dynLoad = dyn_cast<polygeist::DynLoadOp>(op)) {
      addr = dynLoad.getMemref();
    } else if (auto dynStore = dyn_cast<polygeist::DynStoreOp>(op)) {
      addr = dynStore.getMemref();
    } else {
      return;
    }

    if (!addr)
      return;

    if (auto *source = findSourceDataPointer(addr, dataPointers)) {
      attachAliasScopes(op, *source, dataPointers);
      count++;
    }
  });

  return count;
}

struct AliasScopeGenPass
    : public PassWrapper<AliasScopeGenPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AliasScopeGenPass)

  StringRef getArgument() const override { return "arts-alias-scope-gen"; }
  StringRef getDescription() const override {
    return "Generate alias scope metadata for ARTS data arrays";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    ARTS_INFO_HEADER(AliasScopeGenPass);

    int totalCount = 0;
    int totalDecls = 0;

    module.walk([&](LLVM::LLVMFuncOp funcOp) {
      if (!funcOp.getName().starts_with("__arts_edt_"))
        return;

      ARTS_DEBUG_TYPE("Processing EDT function: " << funcOp.getName());

      auto dataPointers = collectDataPointerLoads(funcOp);
      if (dataPointers.empty()) {
        ARTS_INFO("No data pointer loads found in " << funcOp.getName());
        return;
      }

      if (dataPointers.size() > MaxArraysForAliasScopes) {
        ARTS_INFO("Skipping alias scopes for "
                  << funcOp.getName() << ": " << dataPointers.size()
                  << " arrays exceeds limit (" << MaxArraysForAliasScopes
                  << ")");
        return;
      }

      ARTS_INFO("Found " << dataPointers.size() << " data pointer loads in "
                         << funcOp.getName());

      addAlignmentToDataPointerLoads(dataPointers);

      createAliasScopes(ctx, dataPointers);
      emitScopeDeclarations(funcOp, dataPointers);
      totalDecls += dataPointers.size();

      int count = processMemoryAccesses(funcOp, dataPointers);
      totalCount += count;

      ARTS_INFO("Attached alias scopes to " << count << " memory accesses");
    });

    ARTS_INFO("Total: emitted " << totalDecls << " scope declarations, "
                                << "attached alias scopes to " << totalCount
                                << " memory accesses");
    ARTS_INFO_FOOTER(AliasScopeGenPass);
  }
};

} // end anonymous namespace

///===----------------------------------------------------------------------===///
/// Pass creation and registration
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {

std::unique_ptr<Pass> createAliasScopeGenPass() {
  return std::make_unique<AliasScopeGenPass>();
}

} // namespace arts
} // namespace mlir
