# Fix Block Rewriter for Single-Block Allocations (sizes=[1])

## Problem

LULESH fails during `concurrency-opt` with:

```text
'arts.db_ref' op Coarse-grained datablock expects db_ref indices to be constant zero
```

at lulesh.c:1085 (`CalcForceForNodes` / `CalcHourglassControlForElems`).

## Root Cause

`DbRefOp::verify()` correctly requires constant-zero db_ref indices when all allocation sizes are constant 1. The verifier depends only on sizes and element type — mode does not interfere. The bug is in the block rewriter/indexer which produces div/mod (dynamic) db_ref indices even for single-block allocations.

### Error sequence

1. `DbPartitioning` decides `PartitionMode::block` for a datablock
2. `computeSizesFromDecision()` computes `ceil(elemSize/blockSize)`. When `elemSize <= blockSize`, MLIR constant-folds to **[1]**
3. `DbBlockRewriter::transformAcquire()` computes `isSingleBlock=true` and passes it to `rebaseEdtUsers()`
4. **BUG:** `rebaseEdtUsers()` ignores `isSingleBlock` — creates `DbBlockIndexer` which unconditionally produces `(globalIdx / blockSize) - startBlock` (dynamic)
5. Verifier sees `sizes=[1]` → requires constant-zero → **FAILS** on dynamic div/mod

### Layout for single-block case

```text
Original:  db_alloc memref<8 x f64>     (N=8 elements)
Block decision: blockSize=16  (blockSize >= N)

  newOuterSizes = [ceil(8/16)] = [1]    ← triggers verifier coarse check
  newInnerSizes = [16]

New: db_alloc memref<1 x memref<16 x f64>>

  ┌──────────────────────────────────────────────┐
  │ Block 0:  [e0 e1 e2 e3 e4 e5 e6 e7 - - - - - - - -] │
  └──────────────────────────────────────────────┘
  Only 1 block. 8 of 16 slots used.

Access A[j], j in [0,8):
  db_ref index  = 0             (must be constant zero — only 1 block)
  memref index  = j % 16 = j   (rem still correct, just identity here)
```

## Fix Strategy

**Only force db_ref indices to constant zero. Leave ALL memref index computation unchanged** (including div/rem, de-linearization, inner-rank handling).

**Two-tier condition:**

- **Allocation-single-block** (`newOuterSizes` all constant 1): matches verifier condition exactly. This is the must-fix case. Compute from `newOuterSizes` member so it's available everywhere.
- **Acquire-single-block** (per-acquire `elemSz <= blockSize`): optional future optimization. NOT used in this fix because pass-through memref would be unsafe when `startBlock != 0`.

### Why only forcing db_ref to zero is safe

- db_ref selects which block to access. With 1 block, it's always 0.
- memref rem (`j % blockSize`) is still correct — it gives the local index within the block. For single-block it happens to equal `j`, but computing it via rem is harmless and avoids the startBlock pitfall.
- No change to index arity — linearized paths still de-linearize correctly.

## Changes

### Change 1: Add `allocSingleBlock` to `DbBlockIndexer`

**File: `include/arts/Transforms/Datablock/Block/DbBlockIndexer.h`**

```cpp
class DbBlockIndexer : public DbIndexerBase {
  SmallVector<Value> blockSizes;
  SmallVector<Value> startBlocks;
  bool allocSingleBlock = false;  // NEW: all outer sizes are constant 1

public:
  DbBlockIndexer(const PartitionInfo &info, ArrayRef<Value> startBlocks,
                 unsigned outerRank, unsigned innerRank,
                 bool allocSingleBlock = false);  // ADD PARAMETER
```

**File: `lib/arts/Transforms/Datablock/Block/DbBlockIndexer.cpp`** — constructor:

```cpp
DbBlockIndexer::DbBlockIndexer(const PartitionInfo &info,
                               ArrayRef<Value> startBlocks, unsigned outerRank,
                               unsigned innerRank, bool allocSingleBlock)
    : DbIndexerBase(info, outerRank, innerRank),
      blockSizes(info.sizes.begin(), info.sizes.end()),
      startBlocks(startBlocks.begin(), startBlocks.end()),
      allocSingleBlock(allocSingleBlock) {}
```

### Change 2: Force db_ref=0 in `localize()`, keep memref unchanged

**File: `lib/arts/Transforms/Datablock/Block/DbBlockIndexer.cpp`** — `localize()` lines 96-121

In the N-D loop, wrap only the db_ref computation:

```cpp
  for (unsigned d = 0; d < nPartDims && d < globalIndices.size(); ++d) {
    Value globalIdx = globalIndices[d];
    Value bs = (d < blockSizes.size() && blockSizes[d]) ? blockSizes[d] : one();
    bs = builder.create<arith::MaxUIOp>(loc, bs, one());

    /// db_ref index: force zero when allocation has only 1 block
    if (allocSingleBlock) {
      result.dbRefIndices.push_back(zero());
    } else {
      Value sb =
          (d < startBlocks.size() && startBlocks[d]) ? startBlocks[d] : zero();
      Value physBlock = builder.create<arith::DivUIOp>(loc, globalIdx, bs);
      Value relBlock = builder.create<arith::SubIOp>(loc, physBlock, sb);
      result.dbRefIndices.push_back(relBlock);
    }

    /// memref index: UNCHANGED — always compute rem
    if (result.memrefIndices.size() + nonPartDims < innerRank) {
      Value localIdx = builder.create<arith::RemUIOp>(loc, globalIdx, bs);
      result.memrefIndices.push_back(localIdx);
      ARTS_DEBUG("    Dim " << d << ": dbRef="
                            << (allocSingleBlock ? "0(forced)" : "relBlock")
                            << ", memref=" << localIdx);
    } else {
      ARTS_DEBUG("    Dim " << d << ": dbRef only (skipping rem)");
    }
  }
```

### Change 3: Force db_ref=0 in `localizeLinearized()`, keep de-linearization unchanged

**File: `lib/arts/Transforms/Datablock/Block/DbBlockIndexer.cpp`** — `localizeLinearized()` lines 170-175

Replace only the db_ref computation block:

```cpp
  /// db_ref index
  if (allocSingleBlock) {
    result.dbRefIndices.push_back(zero());
  } else {
    Value sb = (!startBlocks.empty() && startBlocks[0]) ? startBlocks[0] : zero();
    Value physBlock = builder.create<arith::DivUIOp>(loc, globalRow, bsRow);
    Value relBlock = builder.create<arith::SubIOp>(loc, physBlock, sb);
    result.dbRefIndices.push_back(relBlock);
  }

  /// memref computation: UNCHANGED — de-linearize, rem, re-linearize as before
  Value localRow = builder.create<arith::RemUIOp>(loc, globalRow, bsRow);
  Value localCol = builder.create<arith::RemUIOp>(loc, globalCol, bsCol);
  // ... rest unchanged (innerRank >= 2 check, re-linearization, etc.)
```

### Change 4: Force db_ref=0 in `transformOps()` SubIndexOp path

**File: `lib/arts/Transforms/Datablock/Block/DbBlockIndexer.cpp`** — `transformOps()` lines 239-259

```cpp
      SmallVector<Value> dbRefIndices;
      if (outerRank > 0) {
        if (allocSingleBlock) {
          /// Single-block: all db_ref indices are zero
          for (unsigned i = 0; i < outerRank; ++i)
            dbRefIndices.push_back(zero);
        } else {
          Value globalIdx = subindex.getIndex();
          // ... existing div/sub code unchanged ...
        }
      } else {
        dbRefIndices.push_back(zero);
      }
```

### Change 5: Compute and pass `allocSingleBlock` in `rebaseEdtUsers()`

**File: `lib/arts/Transforms/Datablock/Block/DbBlockRewriter.cpp`** — `rebaseEdtUsers()` line ~418

Compute from `newOuterSizes` (member), so it works for ALL call paths including mixed-mode full-range:

```cpp
  /// Detect allocation-level single-block: all outer sizes are constant 1.
  /// This matches the verifier's coarse check exactly.
  bool allocSingleBlock = !newOuterSizes.empty() &&
      llvm::all_of(newOuterSizes, [](Value v) {
        int64_t val;
        return ValueUtils::getConstantIndex(v, val) && val == 1;
      });

  auto indexer = std::make_unique<DbBlockIndexer>(
      info, effectiveStartBlocks, newOuterSizes.size(), newInnerSizes.size(),
      allocSingleBlock);  // PASS FLAG
```

### Change 6: Handle single-block in `transformDbRef()` (outside-EDT)

**File: `lib/arts/Transforms/Datablock/Block/DbBlockRewriter.cpp`** — `transformDbRef()`

This does inline div/mod, not through the indexer. Add the same allocation-level check.

After line 229 (after computing `newSource`):

```cpp
  /// Detect allocation-level single-block (same condition as verifier)
  bool allocSingleBlock = !newOuterSizes.empty() &&
      llvm::all_of(newOuterSizes, [](Value v) {
        int64_t val;
        return ValueUtils::getConstantIndex(v, val) && val == 1;
      });
```

In the load/store loop (lines 261-277), wrap only the outer index computation:

```cpp
      if (d < nPartDims) {
        if (allocSingleBlock) {
          newOuter.push_back(
              builder.create<arith::ConstantIndexOp>(userLoc, 0));
        } else {
          Value bs = plan.getBlockSize(d);
          if (!bs)
            bs = one;
          bs = builder.create<arith::MaxUIOp>(userLoc, bs, one);
          newOuter.push_back(
              builder.create<arith::DivUIOp>(userLoc, globalIdx, bs));
        }
        /// memref (inner) index: ALWAYS compute rem
        Value bs = plan.getBlockSize(d);
        if (!bs)
          bs = one;
        bs = builder.create<arith::MaxUIOp>(userLoc, bs, one);
        newInner.push_back(
            builder.create<arith::RemUIOp>(userLoc, globalIdx, bs));
      }
```

In the non-load/store db_ref loop (lines 315-327):

```cpp
    for (unsigned d = 0; d < nPartDims; ++d) {
      if (allocSingleBlock) {
        blockIndices.push_back(builder.create<arith::ConstantIndexOp>(loc, 0));
      } else {
        // ... existing div code unchanged ...
      }
    }
```

### Change 7: Remove dead code `localizeCoordinates()`

**File: `include/arts/Transforms/Datablock/Block/DbBlockIndexer.h`** — remove declaration (lines 85-90)

**File: `lib/arts/Transforms/Datablock/Block/DbBlockIndexer.cpp`** — remove implementation (lines 386-440)

This method is never called anywhere in the pipeline.

### Change 8 (secondary): Use `plan.mode` in `apply()`

**File: `lib/arts/Transforms/Datablock/DbRewriter.cpp`** — line 84-85

```cpp
  PartitionMode partitionMode = plan.mode;
```

Not needed for the verifier fix, but avoids temporary mode mismatch during rewriting.

## Code Paths Covered

| Path | Where db_ref indices created | Fix applied |
| --- | --- | --- |
| EDT, normal access | `localize()` via `rebaseEdtUsers` | Change 2 |
| EDT, linearized access | `localizeLinearized()` via `rebaseEdtUsers` | Change 3 |
| EDT, subindex access | `transformOps()` SubIndexOp | Change 4 |
| Outside EDT, load/store | `transformDbRef()` inline div | Change 6 |
| Outside EDT, bare db_ref | `transformDbRef()` inline div | Change 6 |
| Mixed-mode full-range | `rebaseEdtUsers` (allocSingleBlock computed from member) | Change 5 |
| `localizeCoordinates()` | Dead code — removed (Change 7) | N/A |

## Edge Cases

| Case | outerSizes | allocSingleBlock | Behavior |
| --- | --- | --- | --- |
| Normal block (N=100, bs=25) | [4] | false | div/mod as before |
| Single block (N=8, bs=16) | [1] | true | zero dbRef, rem memref |
| Multi-dim single (N=8x8, bs=16x16) | [1,1] | true | zeros for all outer dims |
| Mixed dims (N=100x8, bs=25x16) | [4,1] | false | div/mod for all |
| Dynamic sizes | [?] | false | div/mod (can't prove single) |
| Coarse (no partitioning) | [1] | n/a | never reaches block rewriter |

## Files to Modify

| File | Change |
| --- | --- |
| `include/arts/Transforms/Datablock/Block/DbBlockIndexer.h` | Add `allocSingleBlock` member + constructor param |
| `lib/arts/Transforms/Datablock/Block/DbBlockIndexer.cpp` | Constructor, `localize()`, `localizeLinearized()`, `transformOps()` SubIndexOp |
| `lib/arts/Transforms/Datablock/Block/DbBlockRewriter.cpp` | `rebaseEdtUsers()` compute+pass flag; `transformDbRef()` inline check |
| `lib/arts/Transforms/Datablock/DbRewriter.cpp` | Use `plan.mode` (secondary) |

## Verification

Following the [CARTS pipeline guide](../agents.md):

1. **Build carts:**

   ```bash
   carts build
   ```

2. **Concurrency-opt checkpoint:**

   ```bash
   cd /Users/randreshg/Documents/carts/external/carts-benchmarks/lulesh
   carts compile lulesh.mlir --concurrency-opt &> lulesh_concurrency_opt.mlir
   ```

   Expected: no `Coarse-grained datablock expects db_ref indices to be constant zero` error.

3. **Full compile and execute:**

   ```bash
   cd /Users/randreshg/Documents/carts/external/carts-benchmarks/lulesh
   carts compile lulesh.c -O3 -DMINI_DATASET -I. -I../common -I../utilities
   ```

4. **Runtime correctness:**

   ```bash
   cd /Users/randreshg/Documents/carts/external/carts-benchmarks/lulesh
   artsConfig=arts.cfg ./lulesh_arts -s 3 -i 5
   ```

5. **Regression check:**

   ```bash
   carts build check-arts 2>&1 | tail -20
   ```
