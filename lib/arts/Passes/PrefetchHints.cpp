///==========================================================================///
/// File: PrefetchHints.cpp
///
/// Inserts software prefetch intrinsics for strided memory accesses in EDT
/// functions. Based on LLVM's LoopDataPrefetch algorithm with adaptations
/// for the CARTS execution model and stencil patterns.
///
/// Key Features:
/// 1. Detects strided memory access patterns via GEP analysis
/// 2. Calculates prefetch distance based on stride magnitude
/// 3. Deduplicates prefetches within same cache line
/// 4. Targets large-stride accesses (row accesses in 2D arrays)
///
/// The pass analyzes loops in EDT functions to find memory accesses with
/// large strides (>= 128 bytes). Hardware prefetchers handle small strides
/// well, but fail on large strides common in row-major 2D array access
/// (e.g., A[i+1][j] in stencil computations).
///
/// Prefetch distance is calculated based on stride magnitude:
/// - Very large stride (>=4KB): 2 iterations ahead
/// - Large stride (1-4KB): 4 iterations ahead
/// - Medium stride (128B-1KB): latency/cycles ~ 6-10 iterations
///
/// Research basis:
/// - LLVM LoopDataPrefetch: Distance formula, SCEV stride detection
/// - GCC -fprefetch-loop-arrays: Trip count heuristics
/// - Intel ICC: Two-level prefetching (L2+L1)
/// - Mowry's algorithm (1992): Cache miss isolation
///==========================================================================///

#include "ArtsPassDetails.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Dominance.h"

#include "arts/ArtsDialect.h"
#include "arts/Passes/ArtsPasses.h"

#include "arts/Utils/ArtsDebug.h"
ARTS_DEBUG_SETUP(arts_prefetch_hints);

using namespace mlir;
using namespace mlir::arts;

namespace {

/// Information about a stencil row access pattern
struct StencilRowPattern {
  Value basePtr;           // Base array pointer
  Value rowIndexBase;      // The outer loop index (i)
  int64_t rowStride;       // Row stride in elements (e.g., 1024)
  int64_t maxRowOffset;    // Maximum row offset used (e.g., +1 for A[i+1][j])
  unsigned elemBytes;      // Element size in bytes
  LLVM::LoadOp sampleLoad; // A sample load to use for insertion point
};

/// Configuration constants based on research from LLVM, GCC, and Intel ICC
struct PrefetchConfig {
  /// Memory latency in cycles (L3 miss to DRAM on typical x86-64)
  static constexpr unsigned MemoryLatencyCycles = 200;

  /// Estimated cycles per loop iteration for typical stencil
  static constexpr unsigned CyclesPerIteration = 30;

  /// Cache line size in bytes (x86-64)
  static constexpr unsigned CacheLineBytes = 64;

  /// Minimum stride (bytes) to trigger prefetch.
  /// Below this, hardware prefetcher is effective.
  /// For stencils, we want to prefetch cache lines (64B), so use 64B threshold.
  /// This enables prefetching for row accesses and larger strides.
  static constexpr unsigned MinPrefetchStride = 64; // 1 cache line

  /// Maximum iterations ahead to prefetch (avoid cache pollution)
  static constexpr unsigned MaxItersAhead = 16;

  /// Maximum prefetches per loop (avoid bandwidth saturation)
  static constexpr unsigned MaxPrefetchesPerLoop = 4;

  /// Minimum loop trip count to enable prefetching
  static constexpr unsigned MinTripCount = 32;
};

/// Information about a strided memory access
struct StridedAccessInfo {
  LLVM::LoadOp loadOp;       // The load operation
  Value basePtr;             // Base pointer of the access
  Value inductionVar;        // Loop induction variable
  int64_t strideBytes;       // Stride in bytes per iteration
  unsigned elementBytes;     // Size of element type
  bool isLargeStride;        // Stride >= MinPrefetchStride
  int dynamicIdxPos;         // Position of the dynamic index containing IV

  /// Get the GEP operation from the load address
  LLVM::GEPOp getGepOp() {
    return loadOp.getAddr().getDefiningOp<LLVM::GEPOp>();
  }
};

/// Calculate element size in bytes for a type
static unsigned getElementBytes(Type type) {
  if (type.isF64())
    return 8;
  if (type.isF32())
    return 4;
  if (type.isInteger(64))
    return 8;
  if (type.isInteger(32))
    return 4;
  if (type.isInteger(16))
    return 2;
  if (type.isInteger(8))
    return 1;
  return 8; // default to f64
}

/// Check if a value depends on the induction variable and extract stride info.
/// Returns the stride multiplier if detectable, 0 otherwise.
static int64_t analyzeValueForIV(Value val, Value inductionVar) {
  if (val == inductionVar) {
    // Direct use: idx = iv
    return 1;
  }

  if (auto mulOp = val.getDefiningOp<LLVM::MulOp>()) {
    Value lhs = mulOp.getLhs();
    Value rhs = mulOp.getRhs();

    bool hasIV = (lhs == inductionVar || rhs == inductionVar);
    if (hasIV) {
      // Try to extract constant multiplier
      Value other = (lhs == inductionVar) ? rhs : lhs;
      if (auto constOp = other.getDefiningOp<LLVM::ConstantOp>()) {
        if (auto intAttr = dyn_cast<IntegerAttr>(constOp.getValue())) {
          return intAttr.getInt();
        }
      }
      // Non-constant stride (runtime value) - assume stride of 1
      return 1;
    }
  }

  if (auto addOp = val.getDefiningOp<LLVM::AddOp>()) {
    Value lhs = addOp.getLhs();
    Value rhs = addOp.getRhs();

    // Check if IV is directly used
    if (lhs == inductionVar || rhs == inductionVar) {
      // Adding offset doesn't change stride
      return 1;
    }

    // Check if either operand is derived from IV via multiplication
    // Pattern: (iv * row_width) + col_offset
    for (Value operand : {lhs, rhs}) {
      int64_t stride = analyzeValueForIV(operand, inductionVar);
      if (stride > 0)
        return stride;
    }
  }

  return 0; // No dependency on IV found
}

/// Analyze GEP to detect stride pattern relative to induction variable.
/// Returns stride in bytes if detectable, 0 otherwise.
/// Also sets outDynamicIdxPos to the position of the dynamic index with IV.
static int64_t analyzeGEPStride(LLVM::GEPOp gepOp, Value inductionVar,
                                unsigned &outElemBytes, int &outDynamicIdxPos) {
  Type elemType = gepOp.getSourceElementType();
  outElemBytes = getElementBytes(elemType);
  outDynamicIdxPos = -1;

  // Get dynamic indices (actual Value operands)
  auto dynamicIndices = gepOp.getDynamicIndices();

  int pos = 0;
  for (Value idx : dynamicIndices) {
    int64_t strideMultiplier = analyzeValueForIV(idx, inductionVar);
    if (strideMultiplier > 0) {
      outDynamicIdxPos = pos;
      return strideMultiplier * outElemBytes;
    }
    pos++;
  }

  return 0; // Could not determine stride
}

/// Try to extract a constant value from a Value, tracing through operations.
/// Returns the constant if found, std::nullopt otherwise.
static std::optional<int64_t> tryExtractConstant(Value val, int depth = 0) {
  if (depth > 3)
    return std::nullopt; // Limit recursion depth

  // Direct constant
  if (auto constOp = val.getDefiningOp<LLVM::ConstantOp>()) {
    if (auto intAttr = dyn_cast<IntegerAttr>(constOp.getValue()))
      return intAttr.getInt();
  }

  // arith.constant
  if (auto constOp = val.getDefiningOp<arith::ConstantOp>()) {
    if (auto intAttr = dyn_cast<IntegerAttr>(constOp.getValue()))
      return intAttr.getInt();
  }

  // Trace through add of constant (x + c or c + x)
  if (auto addOp = val.getDefiningOp<LLVM::AddOp>()) {
    auto lhsConst = tryExtractConstant(addOp.getLhs(), depth + 1);
    auto rhsConst = tryExtractConstant(addOp.getRhs(), depth + 1);
    if (lhsConst && rhsConst)
      return *lhsConst + *rhsConst;
    if (lhsConst)
      return lhsConst;
    if (rhsConst)
      return rhsConst;
  }

  // Trace through mul of constant
  if (auto mulOp = val.getDefiningOp<LLVM::MulOp>()) {
    auto lhsConst = tryExtractConstant(mulOp.getLhs(), depth + 1);
    auto rhsConst = tryExtractConstant(mulOp.getRhs(), depth + 1);
    if (lhsConst && rhsConst)
      return *lhsConst * *rhsConst;
    // For mul, return the constant operand if only one is constant
    if (lhsConst)
      return lhsConst;
    if (rhsConst)
      return rhsConst;
  }

  return std::nullopt;
}

/// Extract the row stride constant from a GEP index expression.
/// Looks for patterns like: (outer_idx * row_stride) + column_idx
/// Returns the row stride if found, 0 otherwise.
static int64_t extractRowStrideFromIndex(Value idx, Value &outRowIndex) {
  // Pattern: add (mul outer_idx, row_stride), column_part
  if (auto addOp = idx.getDefiningOp<LLVM::AddOp>()) {
    for (Value operand : {addOp.getLhs(), addOp.getRhs()}) {
      if (auto mulOp = operand.getDefiningOp<LLVM::MulOp>()) {
        Value lhs = mulOp.getLhs();
        Value rhs = mulOp.getRhs();

        // Try to extract constants by tracing through operation chains
        for (auto [potential_idx, potential_stride] :
             {std::make_pair(lhs, rhs), std::make_pair(rhs, lhs)}) {

          auto maybeStride = tryExtractConstant(potential_stride);
          if (maybeStride) {
            int64_t stride = *maybeStride;
            // Row stride should be reasonably large (>= 64 elements)
            if (stride >= 64) {
              outRowIndex = potential_idx;
              return stride;
            }
          }
        }
      }
    }
  }

  // Direct pattern: mul outer_idx, row_stride
  if (auto mulOp = idx.getDefiningOp<LLVM::MulOp>()) {
    Value lhs = mulOp.getLhs();
    Value rhs = mulOp.getRhs();

    for (auto [potential_idx, potential_stride] :
         {std::make_pair(lhs, rhs), std::make_pair(rhs, lhs)}) {
      auto maybeStride = tryExtractConstant(potential_stride);
      if (maybeStride && *maybeStride >= 64) {
        outRowIndex = potential_idx;
        return *maybeStride;
      }
    }
  }

  return 0;
}

/// Extract the row offset from a row index value.
/// Looks for patterns like: i, i+1, i-1
/// Returns the constant offset or 0 if it's just the base index.
static int64_t extractRowOffset(Value rowIndex, Value &outBaseIndex) {
  // Pattern: add base_idx, offset
  if (auto addOp = rowIndex.getDefiningOp<LLVM::AddOp>()) {
    Value lhs = addOp.getLhs();
    Value rhs = addOp.getRhs();

    for (auto [potential_base, potential_offset] :
         {std::make_pair(lhs, rhs), std::make_pair(rhs, lhs)}) {
      auto maybeOffset = tryExtractConstant(potential_offset);
      if (maybeOffset) {
        outBaseIndex = potential_base;
        return *maybeOffset;
      }
    }
  }

  // No offset detected, this is the base index itself
  outBaseIndex = rowIndex;
  return 0;
}

/// Detect stencil row patterns in a loop.
/// Returns patterns grouped by base array pointer.
static SmallVector<StencilRowPattern>
detectStencilRowPatterns(Block *headerBlock, DominanceInfo &domInfo,
                         LLVM::LLVMFuncOp funcOp) {
  // Map from base pointer to {rowStride, maxRowOffset, elemBytes, rowBaseIndex}
  DenseMap<Value, std::tuple<int64_t, int64_t, unsigned, Value, LLVM::LoadOp>>
      baseToInfo;

  // Walk all blocks dominated by the header (loop body)
  for (Block &block : funcOp.getBody()) {
    if (!domInfo.dominates(headerBlock, &block))
      continue;

    for (auto &op : block) {
      auto loadOp = dyn_cast<LLVM::LoadOp>(&op);
      if (!loadOp)
        continue;

      auto gepOp = loadOp.getAddr().getDefiningOp<LLVM::GEPOp>();
      if (!gepOp)
        continue;

      Value basePtr = gepOp.getBase();
      Type elemType = gepOp.getSourceElementType();
      unsigned elemBytes = getElementBytes(elemType);

      // Analyze each dynamic index for row stride pattern
      for (Value idx : gepOp.getDynamicIndices()) {
        Value rowIndex;
        int64_t rowStride = extractRowStrideFromIndex(idx, rowIndex);

        if (rowStride > 0) {
          // Found a row stride pattern, now extract the row offset
          Value baseIndex;
          int64_t rowOffset = extractRowOffset(rowIndex, baseIndex);

          // Update or create entry for this base pointer
          auto it = baseToInfo.find(basePtr);
          if (it == baseToInfo.end()) {
            baseToInfo[basePtr] = {rowStride, rowOffset, elemBytes, baseIndex,
                                   loadOp};
          } else {
            // Update max row offset if this one is larger
            auto &[existingStride, maxOffset, existingBytes, existingBase,
                   existingLoad] = it->second;
            if (rowOffset > maxOffset) {
              maxOffset = rowOffset;
            }
            // Keep the first load as sample
          }
          break; // Found pattern for this GEP, move to next load
        }
      }
    }
  }

  // Convert map to vector of StencilRowPattern
  SmallVector<StencilRowPattern> patterns;
  for (auto &[basePtr, info] : baseToInfo) {
    auto &[rowStride, maxOffset, elemBytes, baseIndex, sampleLoad] = info;

    // Only consider patterns with positive max offset (accessing future rows)
    // and significant row stride (at least 64 elements = useful 2D array)
    if (maxOffset >= 0 && rowStride >= 64) {
      StencilRowPattern pattern;
      pattern.basePtr = basePtr;
      pattern.rowIndexBase = baseIndex;
      pattern.rowStride = rowStride;
      pattern.maxRowOffset = maxOffset;
      pattern.elemBytes = elemBytes;
      pattern.sampleLoad = sampleLoad;
      patterns.push_back(pattern);

      ARTS_DEBUG_TYPE("  Detected stencil: rowStride="
                      << rowStride << " elements, maxRowOffset=" << maxOffset
                      << ", elemBytes=" << elemBytes);
    }
  }

  return patterns;
}

/// Insert prefetch for a stencil row pattern.
/// Prefetches the row at (maxRowOffset + prefetchDistance) ahead.
static void insertStencilPrefetch(OpBuilder &builder, Location loc,
                                  const StencilRowPattern &pattern,
                                  unsigned prefetchRowsAhead) {
  auto ctx = builder.getContext();
  auto i64Ty = builder.getI64Type();
  auto ptrTy = LLVM::LLVMPointerType::get(ctx);

  // Calculate the row to prefetch: base_row + maxRowOffset + prefetchRowsAhead
  int64_t totalRowOffset = pattern.maxRowOffset + prefetchRowsAhead;

  // Create constant for row offset
  auto rowOffsetConst = builder.create<LLVM::ConstantOp>(
      loc, i64Ty, builder.getI64IntegerAttr(totalRowOffset));

  // Calculate: prefetch_row_index = rowIndexBase + totalRowOffset
  Value prefetchRowIndex =
      builder.create<LLVM::AddOp>(loc, pattern.rowIndexBase, rowOffsetConst);

  // Calculate linear index: prefetch_row_index * rowStride
  auto rowStrideConst = builder.create<LLVM::ConstantOp>(
      loc, i64Ty, builder.getI64IntegerAttr(pattern.rowStride));
  Value linearIndex =
      builder.create<LLVM::MulOp>(loc, prefetchRowIndex, rowStrideConst);

  // Create GEP for prefetch address
  auto prefetchGep = builder.create<LLVM::GEPOp>(
      loc, ptrTy, builder.getF32Type(), // Assuming float for now
      pattern.basePtr, linearIndex);

  // Insert prefetch intrinsic
  // rw=0 (read), hint=3 (high locality), cache=1 (data cache)
  builder.create<LLVM::Prefetch>(loc, prefetchGep,
                                 /*rw=*/0,
                                 /*hint=*/3,
                                 /*cache=*/1);
}

/// Find loop header block and induction variable for a backedge.
/// Returns {headerBlock, inductionVar} or {nullptr, nullptr} if not a loop.
static std::pair<Block *, Value> findLoopInfo(LLVM::BrOp brOp,
                                              DominanceInfo &domInfo) {
  Block *currentBlock = brOp->getBlock();
  Block *headerBlock = brOp.getDest();

  // Verify this is a backedge (branch to a dominating block)
  if (!domInfo.dominates(headerBlock, currentBlock))
    return {nullptr, nullptr};

  // Header block's first argument is typically the induction variable
  if (headerBlock->getNumArguments() == 0)
    return {nullptr, nullptr};

  return {headerBlock, headerBlock->getArgument(0)};
}

/// Collect strided accesses in a loop body.
static SmallVector<StridedAccessInfo>
collectStridedAccesses(Block *headerBlock, Value inductionVar,
                       DominanceInfo &domInfo, LLVM::LLVMFuncOp funcOp) {
  SmallVector<StridedAccessInfo> result;

  // Walk all blocks dominated by the header (loop body)
  for (Block &block : funcOp.getBody()) {
    if (!domInfo.dominates(headerBlock, &block))
      continue;

    for (auto &op : block) {
      auto loadOp = dyn_cast<LLVM::LoadOp>(&op);
      if (!loadOp)
        continue;

      // Check if address comes from GEP
      auto gepOp = loadOp.getAddr().getDefiningOp<LLVM::GEPOp>();
      if (!gepOp)
        continue;

      // Analyze stride pattern
      unsigned elemBytes = 0;
      int dynamicIdxPos = -1;
      int64_t strideBytes =
          analyzeGEPStride(gepOp, inductionVar, elemBytes, dynamicIdxPos);

      if (strideBytes == 0)
        continue; // No detectable stride

      StridedAccessInfo info;
      info.loadOp = loadOp;
      info.basePtr = gepOp.getBase();
      info.inductionVar = inductionVar;
      info.strideBytes = strideBytes;
      info.elementBytes = elemBytes;
      info.isLargeStride = (strideBytes >= PrefetchConfig::MinPrefetchStride);
      info.dynamicIdxPos = dynamicIdxPos;

      ARTS_DEBUG_TYPE("  Access stride=" << strideBytes << " bytes"
                      << ", elemBytes=" << elemBytes
                      << ", dynamicIdxPos=" << dynamicIdxPos
                      << ", isLarge=" << info.isLargeStride);

      result.push_back(info);
    }
  }

  return result;
}

/// Calculate optimal prefetch iterations ahead based on stride magnitude.
/// Uses research-based heuristics from LLVM, GCC, and Intel ICC.
static unsigned calculateItersAhead(int64_t strideBytes) {
  // Large stride (row access): prefetch few iterations ahead
  // Small stride (column access): hardware prefetcher handles it

  if (strideBytes >= 4096) {
    // Very large stride (>= 4KB): prefetch 1-2 iterations ahead
    // This is typical for row access in large 2D arrays
    return 2;
  } else if (strideBytes >= 1024) {
    // Large stride (1-4KB): prefetch 2-4 iterations ahead
    return 4;
  } else if (strideBytes >= PrefetchConfig::MinPrefetchStride) {
    // Medium stride (128B-1KB): use latency-based calculation
    // ItersAhead = MemoryLatency / CyclesPerIteration
    unsigned itersAhead = PrefetchConfig::MemoryLatencyCycles /
                          PrefetchConfig::CyclesPerIteration;
    return std::min(itersAhead, PrefetchConfig::MaxItersAhead);
  }

  // Small stride: hardware prefetcher effective, no software prefetch
  return 0;
}

/// Deduplicate prefetches within same cache line.
/// Based on LLVM's LoopDataPrefetch cache line deduplication strategy.
static void
deduplicatePrefetches(SmallVectorImpl<StridedAccessInfo> &accesses) {
  // Sort by base pointer and stride to group similar accesses
  llvm::stable_sort(accesses, [](const StridedAccessInfo &a,
                                 const StridedAccessInfo &b) {
    // Group by base pointer (by Operation* as proxy)
    if (a.basePtr.getDefiningOp() != b.basePtr.getDefiningOp())
      return a.basePtr.getDefiningOp() < b.basePtr.getDefiningOp();
    return a.strideBytes < b.strideBytes;
  });

  // Remove accesses within same cache line of another
  SmallVector<StridedAccessInfo> deduped;
  for (auto &access : accesses) {
    bool isDuplicate = false;
    for (auto &existing : deduped) {
      // Same base and similar stride -> likely same cache line
      if (existing.basePtr == access.basePtr) {
        int64_t diff = std::abs(existing.strideBytes - access.strideBytes);
        if (diff < PrefetchConfig::CacheLineBytes) {
          isDuplicate = true;
          break;
        }
      }
    }
    if (!isDuplicate)
      deduped.push_back(access);
  }

  accesses = std::move(deduped);
}

/// Compute future address for prefetching by modifying the dynamic index
/// that depends on the induction variable.
static Value computeFutureAddress(OpBuilder &builder, Location loc,
                                  StridedAccessInfo &access,
                                  unsigned itersAhead) {
  auto ptrTy = LLVM::LLVMPointerType::get(builder.getContext());

  // Get the GEP operation from the load
  auto gepOp = access.getGepOp();
  if (!gepOp)
    return nullptr;

  // Get original dynamic indices
  SmallVector<Value> dynamicIndices(gepOp.getDynamicIndices().begin(),
                                    gepOp.getDynamicIndices().end());

  if (access.dynamicIdxPos < 0 ||
      access.dynamicIdxPos >= (int)dynamicIndices.size()) {
    return nullptr;
  }

  // Compute future value for the index: futureIdx = originalIdx + itersAhead
  Value originalIdx = dynamicIndices[access.dynamicIdxPos];
  auto distConst = builder.create<LLVM::ConstantOp>(
      loc, originalIdx.getType(), builder.getI64IntegerAttr(itersAhead));
  Value futureIdx = builder.create<LLVM::AddOp>(loc, originalIdx, distConst);

  // Replace the index with the future value
  dynamicIndices[access.dynamicIdxPos] = futureIdx;

  // Reconstruct GEPArg indices from the raw constant indices and updated
  // dynamic values. The raw constant indices use LLVM::GEPOp::kDynamicIndex
  // to mark positions that use dynamic values.
  auto rawConstantIndices = gepOp.getRawConstantIndices();
  SmallVector<LLVM::GEPArg> gepArgs;
  unsigned dynamicIdx = 0;

  for (int32_t rawIdx : rawConstantIndices) {
    if (rawIdx == LLVM::GEPOp::kDynamicIndex) {
      // This position uses a dynamic value
      if (dynamicIdx < dynamicIndices.size()) {
        gepArgs.push_back(dynamicIndices[dynamicIdx]);
        dynamicIdx++;
      }
    } else {
      // This position uses a static constant
      gepArgs.push_back(rawIdx);
    }
  }

  return builder.create<LLVM::GEPOp>(loc, ptrTy, gepOp.getSourceElementType(),
                                     access.basePtr, gepArgs);
}

/// Insert prefetch intrinsic for a strided access.
static void insertPrefetch(OpBuilder &builder, Location loc,
                           StridedAccessInfo &access,
                           unsigned itersAhead) {
  Value futureAddr = computeFutureAddress(builder, loc, access, itersAhead);
  if (!futureAddr)
    return;

  // Insert prefetch intrinsic
  // Parameters: rw=0 (read), hint=3 (high locality), cache=1 (data cache)
  builder.create<LLVM::Prefetch>(loc, futureAddr,
                                 /*rw=*/0,
                                 /*hint=*/3,
                                 /*cache=*/1);
}

struct PrefetchHintsPass
    : public PassWrapper<PrefetchHintsPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(PrefetchHintsPass)

  StringRef getArgument() const override { return "arts-prefetch-hints"; }
  StringRef getDescription() const override {
    return "Insert software prefetch hints for strided memory accesses";
  }

  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<LLVM::LLVMDialect>();
  }

  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();
    ARTS_INFO_HEADER(PrefetchHintsPass);

    int totalPrefetches = 0;

    module.walk([&](LLVM::LLVMFuncOp funcOp) {
      // Only process EDT functions
      if (!funcOp.getName().starts_with("__arts_edt_"))
        return;

      ARTS_DEBUG_TYPE("Processing EDT function: " << funcOp.getName());

      DominanceInfo domInfo(funcOp);
      int prefetchesInFunc = 0;

      // Find loop backedges and process each loop
      funcOp.walk([&](LLVM::BrOp brOp) {
        auto [headerBlock, inductionVar] = findLoopInfo(brOp, domInfo);
        if (!headerBlock || !inductionVar)
          return;

        ARTS_DEBUG_TYPE("Found loop at " << brOp.getLoc());

        // Collect strided accesses in this loop
        auto accesses =
            collectStridedAccesses(headerBlock, inductionVar, domInfo, funcOp);

        if (accesses.empty())
          return;

        ARTS_DEBUG_TYPE("Found " << accesses.size() << " strided accesses");

        // Filter to large-stride accesses only (hardware handles small strides)
        accesses.erase(llvm::remove_if(accesses,
                                       [](const StridedAccessInfo &a) {
                                         return !a.isLargeStride;
                                       }),
                       accesses.end());

        if (accesses.empty()) {
          ARTS_DEBUG_TYPE("No large-stride accesses, checking for stencil patterns");

          // Try stencil row pattern detection instead
          auto stencilPatterns =
              detectStencilRowPatterns(headerBlock, domInfo, funcOp);

          if (!stencilPatterns.empty()) {
            ARTS_DEBUG_TYPE("Found " << stencilPatterns.size()
                                     << " stencil row patterns");

            OpBuilder builder(ctx);

            for (auto &pattern : stencilPatterns) {
              // Insert prefetch before the sample load
              builder.setInsertionPoint(pattern.sampleLoad);

              // Prefetch 2 rows ahead of the maximum row accessed
              // This hides the memory latency for the next outer-loop iteration
              insertStencilPrefetch(builder, pattern.sampleLoad.getLoc(),
                                    pattern, /*prefetchRowsAhead=*/2);
              prefetchesInFunc++;

              int64_t bytesAhead = (pattern.maxRowOffset + 2) *
                                   pattern.rowStride * pattern.elemBytes;
              ARTS_DEBUG_TYPE("Inserted stencil prefetch: rowStride="
                              << pattern.rowStride
                              << " maxOffset=" << pattern.maxRowOffset
                              << " bytesAhead=" << bytesAhead);
            }
          } else {
            ARTS_DEBUG_TYPE("No stencil patterns found");
          }
          return;
        }

        // Deduplicate prefetches within same cache line
        deduplicatePrefetches(accesses);

        // Limit prefetches per loop to avoid bandwidth saturation
        if (accesses.size() > PrefetchConfig::MaxPrefetchesPerLoop)
          accesses.resize(PrefetchConfig::MaxPrefetchesPerLoop);

        // Insert prefetches right before each load operation to ensure
        // all needed values are available (GEP indices are in scope)
        OpBuilder builder(ctx);

        for (auto &access : accesses) {
          unsigned itersAhead = calculateItersAhead(access.strideBytes);
          if (itersAhead == 0)
            continue;

          // Insert just before the load that we're prefetching for
          builder.setInsertionPoint(access.loadOp);
          insertPrefetch(builder, access.loadOp.getLoc(), access, itersAhead);
          prefetchesInFunc++;

          ARTS_DEBUG_TYPE("Inserted prefetch for stride="
                          << access.strideBytes
                          << " itersAhead=" << itersAhead);
        }
      });

      if (prefetchesInFunc > 0) {
        ARTS_INFO("Inserted " << prefetchesInFunc << " prefetches in "
                              << funcOp.getName());
        totalPrefetches += prefetchesInFunc;
      }
    });

    ARTS_INFO("Total: inserted " << totalPrefetches
                                 << " prefetch instructions");
    ARTS_INFO_FOOTER(PrefetchHintsPass);
  }
};

} // end anonymous namespace

///===----------------------------------------------------------------------===///
/// Pass creation and registration
///===----------------------------------------------------------------------===///
namespace mlir {
namespace arts {

std::unique_ptr<Pass> createPrefetchHintsPass() {
  return std::make_unique<PrefetchHintsPass>();
}

} // namespace arts
} // namespace mlir
