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

#include "ArtsPassDetails.h"
#include "mlir/Dialect/LLVMIR/LLVMAttrs.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMInterfaces.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_alias_scope_gen);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Maximum number of arrays for alias scope generation (following Polly's limit)
/// Alias scope metadata grows quadratically with number of arrays.
constexpr unsigned MaxArraysForAliasScopes = 10;

/// Information about a data pointer loaded from the deps struct
struct DataPointerInfo {
  LLVM::LoadOp loadOp;     // The load op that loads the data pointer
  Value ptr;               // The loaded pointer value
  int64_t depIndex;        // Index into deps array (for naming)
  LLVM::AliasScopeAttr scope;  // Assigned alias scope
};

/// Collect data pointer loads from EDT function
static SmallVector<DataPointerInfo>
collectDataPointerLoads(LLVM::LLVMFuncOp funcOp) {
  SmallVector<DataPointerInfo> result;
  int64_t index = 0;

  funcOp.walk([&](LLVM::LoadOp loadOp) {
    // Check if this loads a pointer type (data pointer from deps)
    if (!isa<LLVM::LLVMPointerType>(loadOp.getResult().getType()))
      return;

    Value addr = loadOp.getAddr();

    // Check if address comes from arts.dep_gep or arts.db_gep or struct GEP
    if (auto defOp = addr.getDefiningOp()) {
      StringRef opName = defOp->getName().getStringRef();
      if (opName == "arts.dep_gep" || opName == "arts.db_gep") {
        result.push_back({loadOp, loadOp.getResult(), index++, {}});
        return;
      }

      // Also check for GEP into struct (accessing ptr field)
      if (auto gepOp = dyn_cast<LLVM::GEPOp>(defOp)) {
        if (auto structType = dyn_cast<LLVM::LLVMStructType>(
                gepOp.getSourceElementType())) {
          result.push_back({loadOp, loadOp.getResult(), index++, {}});
        }
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

  // Create a unique domain for this EDT's data pointers
  auto domain = LLVM::AliasScopeDomainAttr::get(
      ctx, DistinctAttr::create(UnitAttr::get(ctx)),
      StringAttr::get(ctx, "ARTS_data_arrays"));

  // Create unique scope for each data pointer
  for (auto &info : dataPointers) {
    std::string scopeDesc = "array_" + std::to_string(info.depIndex);
    info.scope = LLVM::AliasScopeAttr::get(
        ctx, DistinctAttr::create(UnitAttr::get(ctx)), domain,
        StringAttr::get(ctx, scopeDesc));
  }
}

/// Emit llvm.experimental.noalias.scope.decl intrinsics at function entry.
static void emitScopeDeclarations(LLVM::LLVMFuncOp funcOp,
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

/// Check if a value is derived from a given pointer through GEPs,
/// pointer-to-memref conversions, and other address computations.
/// This handles the common patterns:
///   - LLVM GEP chains
///   - polygeist.pointer2memref (LLVM ptr -> memref)
///   - polygeist.memref2pointer (memref -> LLVM ptr)
///   - memref.reinterpret_cast / subview / cast
static bool isDerivedFrom(Value val, Value source) {
  if (val == source)
    return true;

  if (auto defOp = val.getDefiningOp()) {
    StringRef opName = defOp->getName().getStringRef();

    // LLVM GEP
    if (auto gepOp = dyn_cast<LLVM::GEPOp>(defOp)) {
      return isDerivedFrom(gepOp.getBase(), source);
    }
    // polygeist.pointer2memref - LLVM pointer to memref
    if (opName == "polygeist.pointer2memref") {
      return isDerivedFrom(defOp->getOperand(0), source);
    }
    // polygeist.memref2pointer - memref to LLVM pointer
    if (opName == "polygeist.memref2pointer") {
      return isDerivedFrom(defOp->getOperand(0), source);
    }
    // memref.reinterpret_cast
    if (opName == "memref.reinterpret_cast") {
      return isDerivedFrom(defOp->getOperand(0), source);
    }
    // memref.subview - takes a memref and produces a view
    if (opName == "memref.subview") {
      return isDerivedFrom(defOp->getOperand(0), source);
    }
    // memref.cast
    if (opName == "memref.cast") {
      return isDerivedFrom(defOp->getOperand(0), source);
    }
    // memref.view
    if (opName == "memref.view") {
      return isDerivedFrom(defOp->getOperand(0), source);
    }
    // polygeist.subindex
    if (opName == "polygeist.subindex") {
      return isDerivedFrom(defOp->getOperand(0), source);
    }
  }

  return false;
}

/// Find which data pointer (if any) a memory access address is derived from
static DataPointerInfo *findSourceDataPointer(
    Value addr, SmallVectorImpl<DataPointerInfo> &dataPointers) {
  for (auto &info : dataPointers) {
    if (isDerivedFrom(addr, info.ptr))
      return &info;
  }
  return nullptr;
}

/// Attach alias scope attributes to a load/store operation.
static void attachAliasScopes(Operation *op, DataPointerInfo &source,
                              SmallVectorImpl<DataPointerInfo> &allPointers) {
  MLIRContext *ctx = op->getContext();

  // Set alias_scopes (our scope) - this tells LLVM which scope this access belongs to
  SmallVector<Attribute> aliasScopes = {source.scope};
  auto aliasScopesAttr = ArrayAttr::get(ctx, aliasScopes);

  // Use the proper AliasAnalysisOpInterface methods if available
  if (auto loadOp = dyn_cast<LLVM::LoadOp>(op)) {
    loadOp.setAliasScopesAttr(aliasScopesAttr);
  } else if (auto storeOp = dyn_cast<LLVM::StoreOp>(op)) {
    storeOp.setAliasScopesAttr(aliasScopesAttr);
  } else {
    // Fallback for other ops (polygeist load/store)
    op->setAttr("alias_scopes", aliasScopesAttr);
  }
}

/// Process memory accesses in the function.
static int processMemoryAccesses(LLVM::LLVMFuncOp funcOp,
                                  SmallVectorImpl<DataPointerInfo> &dataPointers) {
  int count = 0;

  /// Build set of data pointer values to skip when processing.
  DenseSet<Value> dataPointerValues;
  for (auto &info : dataPointers) {
    dataPointerValues.insert(info.ptr);
  }

  funcOp.walk([&](Operation *op) {
    Value addr;

    // Get address from LLVM load/store
    if (auto loadOp = dyn_cast<LLVM::LoadOp>(op)) {
      // Skip the data pointer loads themselves using Value comparison
      // (works correctly after hoisting moves the loads)
      if (dataPointerValues.contains(loadOp.getResult()))
        return;
      addr = loadOp.getAddr();
    } else if (auto storeOp = dyn_cast<LLVM::StoreOp>(op)) {
      addr = storeOp.getAddr();
    }
    // Also check polygeist load/store
    else if (op->getName().getStringRef() == "polygeist.load") {
      if (op->getNumOperands() > 0)
        addr = op->getOperand(0);
    } else if (op->getName().getStringRef() == "polygeist.store") {
      if (op->getNumOperands() > 1)
        addr = op->getOperand(1);
    } else {
      return;
    }

    if (!addr)
      return;

    // Find which data pointer this access is derived from
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

    // Process each EDT function (LLVM::LLVMFuncOp after ConvertPolygeistToLLVM)
    module.walk([&](LLVM::LLVMFuncOp funcOp) {
      if (!funcOp.getName().starts_with("__arts_edt_"))
        return;

      ARTS_DEBUG_TYPE("Processing EDT function: " << funcOp.getName());

      // Collect data pointer loads
      auto dataPointers = collectDataPointerLoads(funcOp);
      if (dataPointers.empty()) {
        ARTS_INFO("No data pointer loads found in " << funcOp.getName());
        return;
      }

      /// Check array count limit (alias scope metadata grows quadratically).
      if (dataPointers.size() > MaxArraysForAliasScopes) {
        ARTS_INFO("Skipping alias scopes for " << funcOp.getName()
                  << ": " << dataPointers.size() << " arrays exceeds limit ("
                  << MaxArraysForAliasScopes << ")");
        return;
      }

      ARTS_INFO("Found " << dataPointers.size() << " data pointer loads in "
                         << funcOp.getName());

      createAliasScopes(ctx, dataPointers);
      emitScopeDeclarations(funcOp, dataPointers);
      totalDecls += dataPointers.size();

      int count = processMemoryAccesses(funcOp, dataPointers);
      totalCount += count;

      ARTS_INFO("Attached alias scopes to " << count << " memory accesses");
    });

    ARTS_INFO("Total: emitted " << totalDecls << " scope declarations, "
              << "attached alias scopes to " << totalCount << " memory accesses");
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
