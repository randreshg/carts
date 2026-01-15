# Plan: DB Partitioning Refactoring for Clarity + LULESH Case Study

## Problem Summary

The current partitioning system has naming confusion:
- `RewriterMode::ElementWise` represents **both** coarse (`outerRank=0`) and fine-grained (`outerRank>0`)
- `PromotionMode::none` doesn't clearly convey "coarse"
- Decision logic and rewriter implementation are conflated

**The user correctly identified**: ElementWise can be either coarse-grained or fine-grained based on `outerRank`. This is architecturally correct but confusing.

## Current Pass Flow (CORRECTED from carts-run.cpp)

The actual pipeline order is critical to understand:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ACTUAL PIPELINE ORDER                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Stage 7: CreateDbs                                                         │
│  ├── CreateDbsPass         ← Creates initial DBs (NO hints yet!)            │
│  └── Canonicalize/CSE                                                       │
│                                                                             │
│  Stage 8: DbOpt                                                             │
│  └── DbPass                ← First DB optimization pass                     │
│                                                                             │
│  Stage 9: EdtOpt                                                            │
│  └── EdtPass + LoopFusion                                                   │
│                                                                             │
│  Stage 10: Concurrency                                                      │
│  ├── ConcurrencyPass                                                        │
│  └── ForLoweringPass       ← GENERATES offset_hints, size_hints, indices!   │
│                                                                             │
│  Stage 11: ConcurrencyOpt                                                   │
│  ├── EdtPass                                                                │
│  ├── DbPartitioningPass    ← USES hints to partition (coarse → fine!)       │
│  └── DbPass                ← Re-run to adjust modes after partitioning      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Insight: Hints Are Generated AFTER CreateDbs!

**CreateDbs Pass** (stage 7):
- Creates DB allocations based on memref usage in EDTs
- At this point, **NO partition hints exist** - no offset_hints, size_hints, indices
- DBs are created with basic structure only

**ForLoweringPass** (stage 10, in Concurrency):
- Analyzes loop iteration variables and depend clauses
- **GENERATES** `offset_hints`, `size_hints`, `indices` on DbAcquireOps
- These hints indicate how the loop accesses the DB

**DbPartitioningPass** (stage 11, in ConcurrencyOpt):
- **USES** the hints from ForLowering to make partitioning decisions
- Can convert coarse → fine-grained based on hints and heuristics
- Key code at `DbPartitioning.cpp:761-778`:

```cpp
if (auto existingMode = getPartitionMode(allocOp)) {
  if (*existingMode != PromotionMode::none) {
    return allocOp;  // Skip - already partitioned!
  }
  // mode == none means need to re-analyze with hints from ForLowering
}
```

**Conversion happens when:**
1. ForLowering has added hints to the acquires
2. `canBePartitioned(useFineGrainedFallback)` returns true
3. Heuristics choose fine-grained mode based on hints
4. All acquires have valid partition info from hints

This confirms: **Coarse → Fine conversion happens in DbPartitioning using hints from ForLowering**.

---

## Design Principles

### No Hardcoded Strings
All mode names and decisions must use **enums and attributes**, never string comparisons:

```cpp
// BAD - hardcoded string comparisons
if (modeName == "coarse") { ... }
if (getPartitionModeLabel(mode) == "element_wise") { ... }

// GOOD - enum comparisons
if (decision.granularity == PartitionGranularity::Coarse) { ... }
if (decision.isCoarse()) { ... }
if (mode == PromotionMode::element_wise) { ... }
```

This prevents typos, enables compiler type checking, and makes refactoring safe.

---

## Solution: Layered Abstraction

Separate **decision vocabulary** (user-facing) from **rewriter implementation** (internal):

```
User/CLI Level          Decision Level              Implementation Level
------------------      ------------------          --------------------
--partition-fallback    PartitionGranularity       RewriterMode + outerRank
  =coarse               =Coarse                    =ElementWise, outerRank=0
  =fine                 =FineGrained               =ElementWise, outerRank>0
                        =Chunked                   =Chunked
                        =Stencil                   =Stencil
```

---

## Phase 1: Add `PartitionGranularity` Enum

**File**: `include/arts/Analysis/Heuristics/PartitioningHeuristics.h`

Add a new semantic enum for decision-level clarity:

```cpp
/// User-facing partition granularity - the "what" (semantic meaning).
/// Maps to RewriterMode + outerRank (the "how") internally.
enum class PartitionGranularity {
  Coarse,      // Single DB for entire array (ElementWise + outerRank=0)
  FineGrained, // One DB per element/row (ElementWise + outerRank>0)
  Chunked,     // Fixed-size chunks
  Stencil      // Chunked with halo regions (ESD)
};
```

---

## Phase 2: Update `PartitioningDecision`

**File**: `include/arts/Analysis/Heuristics/PartitioningHeuristics.h`

Update the decision struct to use the new enum:

```cpp
struct PartitioningDecision {
  PartitionGranularity granularity = PartitionGranularity::Coarse;
  unsigned outerRank = 0, innerRank = 0;
  std::string rationale;

  // Factory methods with clear names
  static PartitioningDecision coarse(const PartitioningContext &ctx, StringRef reason);
  static PartitioningDecision fineGrained(const PartitioningContext &ctx, unsigned outerRank, StringRef reason);
  static PartitioningDecision chunked(const PartitioningContext &ctx, StringRef reason);
  static PartitioningDecision stencil(const PartitioningContext &ctx, StringRef reason);

  // Queries derived from granularity
  bool isCoarse() const { return granularity == PartitionGranularity::Coarse; }
  bool isFineGrained() const { return granularity == PartitionGranularity::FineGrained; }
  bool isChunked() const {
    return granularity == PartitionGranularity::Chunked ||
           granularity == PartitionGranularity::Stencil;
  }

  // Convert to implementation-level RewriterMode
  RewriterMode toRewriterMode() const;
};
```

---

## Phase 3: Add Mapping Function

**File**: `include/arts/Transforms/Datablock/DbRewriter.h`

```cpp
/// Map decision-level granularity to implementation-level RewriterMode.
inline RewriterMode toRewriterMode(PartitionGranularity granularity) {
  switch (granularity) {
  case PartitionGranularity::Coarse:
  case PartitionGranularity::FineGrained:
    return RewriterMode::ElementWise;  // Both use same rewriter, differ by outerRank
  case PartitionGranularity::Chunked:
    return RewriterMode::Chunked;
  case PartitionGranularity::Stencil:
    return RewriterMode::Stencil;
  }
}
```

---

## Phase 4: Update Heuristics to Use New Enum

**File**: `lib/arts/Analysis/Heuristics/PartitioningHeuristics.cpp`

Update each heuristic to use the semantic factory methods:

```cpp
// H1.1: ReadOnlySingleNodeHeuristic
std::optional<PartitioningDecision>
ReadOnlySingleNodeHeuristic::evaluate(const PartitioningContext &ctx) const {
  if (/* conditions */) {
    return PartitioningDecision::coarse(ctx, "H1.1: Read-only single-node");
  }
  return std::nullopt;
}

// H1.5: StencilPatternHeuristic
std::optional<PartitioningDecision>
StencilPatternHeuristic::evaluate(const PartitioningContext &ctx) const {
  if (patterns.hasIndexed) {
    return PartitioningDecision::fineGrained(ctx, 1, "H1.5: Indexed access");
  }
  // ... etc
}
```

---

## Phase 5: Update DbPartitioning Pass

**File**: `lib/arts/Passes/Optimizations/DbPartitioning.cpp`

- Use `decision.granularity` instead of `decision.mode` where appropriate
- Call `decision.toRewriterMode()` when constructing `DbRewritePlan`
- Update logging to show `PartitionGranularity` names

---

## Phase 6: Consider Renaming IR Attribute (Optional)

**File**: `include/arts/ArtsAttributes.td`

Optionally rename for clarity (breaking change):
- `none` → `coarse`
- `element_wise` → `fine_grained`

**Alternative**: Keep existing names for IR compatibility, add comments documenting the mapping.

---

## Phase 7: Update Documentation

**File**: `docs/heuristics/partitioning/partitioning.md`

### 7.1: Add Terminology Mapping Section (after line 8)

```markdown
## Terminology Mapping

| User/CLI Term | Decision Enum | RewriterMode | outerRank | IR Attribute |
|---------------|---------------|--------------|-----------|--------------|
| coarse        | Coarse        | ElementWise  | 0         | none         |
| fine          | FineGrained   | ElementWise  | >0        | element_wise |
| chunked       | Chunked       | Chunked      | 1         | chunked      |
| stencil       | Stencil       | Stencil      | 1         | chunked      |

**Key insight**: `RewriterMode::ElementWise` handles both coarse and fine-grained.
The difference is `outerRank`: coarse has `outerRank=0` (single DB), fine-grained
has `outerRank>0` (multiple DBs, one per element/row).
```

### 7.2: Add Complete Partitioning Flowchart (after Modes at a Glance)

```markdown
## Complete Partitioning Decision Flowchart

This flowchart shows the end-to-end decision process across the pipeline stages.

**CRITICAL**: The hints (offset_hints, size_hints, indices) are generated by ForLoweringPass
AFTER CreateDbs runs. DbPartitioning uses these hints to make partitioning decisions.

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        STAGE 7: CreateDbs Pass                                       │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│   For each memref used in EDTs:                                                      │
│                                                                                      │
│   ┌──────────────────────────────────────────────────────────────────────┐          │
│   │              Create Initial DbAllocOp + DbAcquireOps                 │          │
│   │                                                                      │          │
│   │   • No partition hints exist yet (ForLowering hasn't run!)           │          │
│   │   • Creates basic DB structure from memref usage                     │          │
│   │   • DbAcquireOps have offsets/sizes but NO offset_hints/size_hints   │          │
│   └──────────────────────────────────────────────────────────────────────┘          │
│                                                                                      │
│   Output: DbAllocOps with DbAcquireOps (no partition hints yet)                      │
│                                                                                      │
└─────────────────────────────────────────┬───────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        STAGE 8-9: DbOpt + EdtOpt                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│   DbPass: Initial DB optimization                                                    │
│   EdtPass + LoopFusion: EDT optimization                                            │
└─────────────────────────────────────────┬───────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        STAGE 10: Concurrency                                         │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│   ConcurrencyPass: Analyze concurrency patterns                                      │
│                                                                                      │
│   ┌──────────────────────────────────────────────────────────────────────┐          │
│   │              ForLoweringPass - GENERATES HINTS!                      │          │
│   │                                                                      │          │
│   │   Analyzes loop iteration variables and depend clauses:              │          │
│   │                                                                      │          │
│   │   • indices[] ← from indexed access patterns (A[i])                  │          │
│   │   • offset_hints[] ← from chunk start (workerIdx * chunkSize)        │          │
│   │   • size_hints[] ← from chunk size (min(chunkSize, remaining))       │          │
│   │                                                                      │          │
│   │   These hints are ADDED to existing DbAcquireOps                     │          │
│   └──────────────────────────────────────────────────────────────────────┘          │
│                                                                                      │
│   Output: DbAcquireOps NOW have offset_hints, size_hints, indices                    │
│                                                                                      │
└─────────────────────────────────────────┬───────────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        STAGE 11: ConcurrencyOpt                                      │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│   EdtPass: Final EDT pass                                                            │
│                                                                                      │
│   ┌──────────────────────────────────────────────────────────────────────┐          │
│   │              DbPartitioningPass - USES HINTS!                        │          │
│   │                                                                      │          │
│   │   For each DbAllocOp:                                                │          │
│   │                                                                      │          │
│   │   ┌────────────────────────────────────────────────────────────┐    │          │
│   │   │  canBePartitioned(useFineGrainedFallback)?                 │    │          │
│   │   │                                                            │    │          │
│   │   │  Checks:                                                   │    │          │
│   │   │  • hasNonAffineAccesses && !useFineGrainedFallback → false│    │          │
│   │   │  • Safety checks on hints and structure                    │    │          │
│   │   │                                                            │    │          │
│   │   │  false? → SKIP (keep original)                             │    │          │
│   │   └──────────────────────────┬─────────────────────────────────┘    │          │
│   │                              │ true                                  │          │
│   │                              ▼                                       │          │
│   │   ┌────────────────────────────────────────────────────────────┐    │          │
│   │   │  Per-Acquire Analysis (using hints from ForLowering)       │    │          │
│   │   │                                                            │    │          │
│   │   │  For each DbAcquireOp:                                     │    │          │
│   │   │    • Read offset_hints → partitionOffset                   │    │          │
│   │   │    • Read size_hints → partitionSize                       │    │          │
│   │   │    • Read indices → canElementWise = true                  │    │          │
│   │   │    • Determine mode: Coarse / ElementWise / Chunked        │    │          │
│   │   └──────────────────────────┬─────────────────────────────────┘    │          │
│   │                              │                                       │          │
│   │                              ▼                                       │          │
│   │   ┌────────────────────────────────────────────────────────────┐    │          │
│   │   │  Heuristics Evaluation                                     │    │          │
│   │   │                                                            │    │          │
│   │   │  Priority Order:                                           │    │          │
│   │   │  100: H1.1 ReadOnlySingleNode → Coarse                     │    │          │
│   │   │   95: H1.5 StencilPattern → Stencil/FineGrained            │    │          │
│   │   │   90: H1.4 MultiNode → FineGrained preferred               │    │          │
│   │   │   80: H1.3 AccessUniformity → Coarse fallback              │    │          │
│   │   │   50: H1.2 CostModel → Best of options                     │    │          │
│   │   │                                                            │    │          │
│   │   │  First heuristic returning decision WINS                   │    │          │
│   │   └──────────────────────────┬─────────────────────────────────┘    │          │
│   │                              │                                       │          │
│   │                              ▼                                       │          │
│   │   ┌────────────────────────────────────────────────────────────┐    │          │
│   │   │  Apply Decision                                            │    │          │
│   │   │                                                            │    │          │
│   │   │  if decision.isCoarse():                                   │    │          │
│   │   │      → Keep original (no change)                           │    │          │
│   │   │  else:                                                     │    │          │
│   │   │      → Apply DbRewriter (transforms allocation)            │    │          │
│   │   │      → Set partition attribute                             │    │          │
│   │   │                                                            │    │          │
│   │   │  ┌──────────────────────────────────────────────────┐     │    │          │
│   │   │  │  CONVERSION EXAMPLE:                             │     │    │          │
│   │   │  │  sizes=[1] → sizes=[N]                           │     │    │          │
│   │   │  │  partition=none → partition=element_wise         │     │    │          │
│   │   │  └──────────────────────────────────────────────────┘     │    │          │
│   │   └────────────────────────────────────────────────────────────┘    │          │
│   └──────────────────────────────────────────────────────────────────────┘          │
│                                                                                      │
│   DbPass: Re-run to adjust modes after partitioning                                  │
│                                                                                      │
└─────────────────────────────────────────────────────────────────────────────────────┘
```
```

### 7.3: Add Pass Pipeline Section (after flowchart)

```markdown
## Pass Pipeline: CreateDbs → ForLowering → DbPartitioning

The partitioning decision spans THREE key passes in the pipeline:

### Stage 7: CreateDbs Pass (Initial Allocation)
- Creates DbAllocOps and DbAcquireOps based on memref usage in EDTs
- **NO partition hints exist yet** - ForLowering hasn't run!
- Creates basic structure: offsets/sizes from memref access patterns
- Does NOT make partitioning decisions at this stage

### Stage 10: ForLowering Pass (Hint Generation) - IN CONCURRENCY STAGE
- Analyzes loop iteration variables and depend clauses
- **GENERATES hints** on existing DbAcquireOps:
  - `indices[]` ← from indexed access patterns (e.g., `A[i]`)
  - `offset_hints[]` ← from chunk start (e.g., `workerIdx * chunkSize`)
  - `size_hints[]` ← from chunk size (e.g., `min(chunkSize, remaining)`)
- These hints enable partitioning decisions in the next stage

### Stage 11: DbPartitioning Pass (Partitioning Decisions) - IN CONCURRENCYOPT STAGE
- **USES hints** from ForLowering to make partitioning decisions
- Can convert allocations from coarse → fine-grained
- Applies heuristics (H1.1 through H1.5) based on:
  - Per-acquire capabilities (from hints)
  - Access patterns (uniform/stencil/indexed)
  - Machine configuration (single-node vs multi-node)
- `--partition-fallback=fine` enables non-affine access conversion

### Conversion Flow
```
Stage 7 (CreateDbs):
    DbAllocOp created with basic structure
    DbAcquireOps have offsets/sizes (NO hints yet)
                    ↓
Stage 10 (ForLowering):
    ForLowering analyzes loop structure
    ADDS offset_hints, size_hints, indices to DbAcquireOps
                    ↓
Stage 11 (DbPartitioning):
    Reads hints from DbAcquireOps
    Builds PartitioningContext
    Evaluates heuristics → PartitioningDecision
    If fine-grained chosen:
        → Apply DbRewriter
        → sizes=[1] → sizes=[N]
        → partition=none → partition=element_wise
```

This three-stage design allows:
- CreateDbs to run early without loop analysis overhead
- ForLowering to add rich hints based on loop structure
- DbPartitioning to make informed decisions using those hints
```

### 7.3: Add Stencil Mode Section (after Mixed Mode, ~line 567)

```markdown
## Stencil Mode (ESD - Ephemeral Slice Dependencies)

Stencil mode is a specialized form of chunked partitioning for stencil access patterns.

### When Stencil Mode is Selected
- H1.5 detects `hasStencil` pattern in access analysis
- Access bounds show non-zero offsets (e.g., `A[i-1]`, `A[i+1]`)

### How It Differs from Chunked
- **Inner size extended**: `innerSize = baseChunkSize + haloLeft + haloRight`
- **Halo regions**: Left and right neighbors included in each chunk
- **ESD delivery**: Runtime delivers halo data to adjacent chunks

### IR Representation
Both Chunked and Stencil use `promotion_mode<chunked>` in IR.
The difference is internal (StencilInfo with halo sizes).

For detailed stencil documentation, see:
`/docs/heuristics/partitioning/stencil.md`
```

---

## Phase 8: Expand LULESH Case Study

**File**: `docs/heuristics/partitioning/partitioning.md` (replace lines 205-248)

### Complete LULESH Case Study Content

```markdown
## LULESH Case Study: Coarse vs Fine vs DB Copy

This case study compares three partitioning strategies for LULESH, demonstrating
the performance implications of each approach.

### The LULESH Challenge
LULESH uses hexahedral mesh elements where each element accesses 8 nodes through
an indirection array (`nodelist[elem][0..7]`). This indirect/gather pattern is
classified as non-affine, causing `canBePartitioned()` to block partitioning by default.

### Mode 1: Coarse-Grained (Default)

**What happens:**
- Non-affine accesses → `canBePartitioned()` returns false
- Allocation stays coarse: `sizes=[1], elementSizes=[numNodes]`
- All EDTs serialize on coarse acquires

**Commands:**
```bash
carts run lulesh.mlir --concurrency-opt > lulesh_coarse.mlir
carts benchmarks run lulesh --size small
```

**Expected metrics:**
| Metric | Value |
|--------|-------|
| E2E Time | ~11.28s |
| Slowdown vs OMP | ~250x |
| DBs Created | ~30 (one per array) |
| Acquires/Element | ~15 |
| Parallelism | None (serialized) |

### Mode 2: Fine-Grained (Element-wise Fallback)

**What happens:**
- `--partition-fallback=fine` enables non-affine conversion
- `canBePartitioned(useFineGrainedFallback=true)` returns true
- H1.5 selects element-wise for indexed access patterns
- Allocation becomes: `sizes=[numNodes], elementSizes=[1]`

**Commands:**
```bash
carts run lulesh.mlir --concurrency-opt --partition-fallback=fine > lulesh_fine.mlir
carts benchmarks run lulesh --size small -- --partition-fallback=fine
```

**Expected metrics:**
| Metric | Value |
|--------|-------|
| E2E Time | TBD (measure) |
| Speedup vs Coarse | TBD |
| DBs Created | numNodes (e.g., 729 for s=8) |
| Acquires/Element | 8 (hex corners) × 3 (x,y,z) = 24 |
| Parallelism | Full (independent) |

### Mode 3: Versioned DB Copy (Future)

**Proposed approach:**
- Write-optimized DB: Chunked partitioning for direct writes
- Read-optimized DB: Element-wise copy for indirect reads
- Explicit sync between write and read phases

**Placeholder commands (not yet implemented):**
```bash
carts run lulesh.mlir --concurrency-opt --db-versioning > lulesh_versioned.mlir
```

### Verification

All modes must produce identical results:
```bash
# Verify correctness for each mode
carts benchmarks run lulesh --size small --verify
carts benchmarks run lulesh --size small --verify -- --partition-fallback=fine

# Compare IR partition modes
grep "promotion_mode" lulesh_coarse.mlir | sort | uniq -c
grep "promotion_mode" lulesh_fine.mlir | sort | uniq -c
```

### Performance Comparison Table

| Metric | Coarse | Fine | Versioned | OMP |
|--------|--------|------|-----------|-----|
| E2E Time (s) | 11.28 | TBD | TBD | 0.045 |
| Speedup vs OMP | 0.004x | TBD | TBD | 1.0x |
| DBs Created | ~30 | numNodes | 2×numNodes | N/A |
| Acquires/Iter | ~15 | ~24K | ~512+8K | N/A |
| Memory | 1x | 1x+meta | 2x | 1x |
| Parallelism | None | Full | Moderate | Full |
| Correctness | ✓ | ✓ | ✓ | ✓ |

### When to Use Each Mode

| Scenario | Mode | Rationale |
|----------|------|-----------|
| Development/debugging | Coarse | Simple, guaranteed correct |
| Small problem sizes | Fine | Acquire overhead < parallelism benefit |
| Large problem sizes | Versioned | Sync cost amortized |
| Memory-constrained | Fine | No 2x memory for copies |
```

---

## Files to Modify

| File | Change |
|------|--------|
| `include/arts/Analysis/Heuristics/PartitioningHeuristics.h` | Add `PartitionGranularity` enum, update `PartitioningDecision` |
| `lib/arts/Analysis/Heuristics/PartitioningHeuristics.cpp` | Update heuristics to use factory methods |
| `include/arts/Transforms/Datablock/DbRewriter.h` | Add `toRewriterMode()` mapping |
| `lib/arts/Passes/Optimizations/DbPartitioning.cpp` | Use new decision structure |
| `docs/heuristics/partitioning/partitioning.md` | Add terminology mapping, expand LULESH case study |

---

## Verification Plan

### Step 1: Build
```bash
cd /Users/randreshg/Documents/carts && carts build
```

### Step 2: Test Default Behavior (no regressions)
```bash
carts benchmarks run jacobi --size small
carts benchmarks run lulesh --size small
```

### Step 3: Test Fine-Grained Fallback
```bash
carts benchmarks run lulesh --size small -- --partition-fallback=fine
# Verify:
# - Correctness: checksum matches OMP
# - Performance: expect significant speedup vs coarse
# - IR: promotion_mode<element_wise> for indirect arrays
```

### Step 4: Verify the Three-Stage Pipeline
```bash
# Stage 7: After CreateDbs - DBs exist but NO hints yet
carts run lulesh.mlir --create-dbs > lulesh_after_createdb.mlir
grep "offset_hints" lulesh_after_createdb.mlir | wc -l  # Should be 0!
grep "arts.db_alloc" lulesh_after_createdb.mlir | head -3

# Stage 10: After ForLowering (in Concurrency) - Hints NOW exist
carts run lulesh.mlir --concurrency > lulesh_after_forlowering.mlir
grep "offset_hints" lulesh_after_forlowering.mlir | wc -l  # Should be > 0!
grep "size_hints" lulesh_after_forlowering.mlir | wc -l    # Should be > 0!

# Stage 11: After DbPartitioning (in ConcurrencyOpt) - Partitioning applied
carts run lulesh.mlir --concurrency-opt > lulesh_coarse.mlir
grep "promotion_mode" lulesh_coarse.mlir | sort | uniq -c
```

### Step 5: Verify Coarse → Fine Conversion Flow
```bash
# Default (coarse) - non-affine arrays stay coarse
carts run lulesh.mlir --concurrency-opt > lulesh_coarse.mlir
grep "promotion_mode<none>" lulesh_coarse.mlir | wc -l

# Fine fallback - non-affine arrays converted to element-wise
carts run lulesh.mlir --concurrency-opt --partition-fallback=fine > lulesh_fine.mlir
grep "promotion_mode<element_wise>" lulesh_fine.mlir | wc -l

# Compare DB allocation structure
diff <(grep "arts.db_alloc" lulesh_coarse.mlir | head -5) \
     <(grep "arts.db_alloc" lulesh_fine.mlir | head -5)
# Should show: sizes=[1] → sizes=[numNodes]
```

### Step 6: Verify Heuristic Decisions
```bash
# Check which heuristics fire
carts run lulesh.mlir --concurrency-opt 2>&1 | grep -E "H1\.[0-9]"
carts run lulesh.mlir --concurrency-opt --partition-fallback=fine 2>&1 | grep -E "H1\.[0-9]"

# Verify H1.5 triggers for indexed patterns
carts run lulesh.mlir --concurrency-opt --partition-fallback=fine 2>&1 | grep "H1.5"
```

### Step 7: Documentation Review
- Verify terminology mapping is accurate
- Check LULESH case study commands work
- Confirm performance metrics can be collected
- Verify flowchart renders correctly in markdown viewer

### Step 8: Code Review Checklist (No Hardcoded Strings)
After implementation, verify NO code contains:
```bash
# Search for hardcoded string comparisons (should find NONE)
grep -rn '"coarse"' lib/arts/Analysis/Heuristics/
grep -rn '"fine"' lib/arts/Analysis/Heuristics/
grep -rn '"element_wise"' lib/arts/Passes/Optimizations/DbPartitioning.cpp
grep -rn 'modeName ==' lib/arts/

# All mode checks should use enums:
grep -rn 'PartitionGranularity::' lib/arts/  # Should find many
grep -rn 'decision.isCoarse()' lib/arts/      # Should find many
grep -rn 'decision.isFineGrained()' lib/arts/ # Should find many
```

---

## Success Criteria

1. **Code clarity**: `PartitionGranularity::Coarse` is self-documenting
2. **No regressions**: Existing tests pass
3. **LULESH improvement**: Fine-grained mode shows speedup vs coarse
4. **Documentation**: Clear mapping between all terminology layers
5. **User preference**: `--partition-fallback=coarse|fine` works as expected
