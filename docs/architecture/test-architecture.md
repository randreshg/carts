# Test Architecture: Decoupling from Benchmarks

## Current State Assessment

### Test Inventory (168 tests)

| Category | Count | % | Description |
|---|---|---|---|
| Self-contained inline MLIR | 108 | 64% | `%carts-compile %s` — test IS the input |
| Benchmark-coupled (C source) | 36 | 21% | Compiles C from `external/carts-benchmarks/` |
| Shared MLIR inputs | 23 | 14% | Uses files from `tests/contracts/inputs/` |
| Cross-test snapshot dependency | 1 | 1% | Reads another test's Output/ dir |

### Per-Dialect Distribution

```
sde/    — 27 tests (5 benchmark-coupled)
core/   — 74 tests (14 benchmark-coupled, 1 snapshot-dependent)
  partitioning/ — 11 tests (4 benchmark-coupled)
rt/     — 45 tests (17 benchmark-coupled)
verify/ —  7 tests (0 benchmark-coupled)
cli/    — 10 tests (0 benchmark-coupled)
```

### Benchmark C Source Coupling

9 distinct C files from `external/carts-benchmarks/` serve as inputs:

| C Source | Tests | Dialect Coverage |
|---|---|---|
| `seidel-2d.c` | 9 | rt(6), core(2), sde(1) |
| `rhs4sg_base.c` | 8 | rt(5), sde(1), core(1), partitioning(1) |
| `velocity_update.c` | 5 | rt(4), sde(1) |
| `activations.c` | 4 | sde(2), core(2) |
| `vel4sg_base.c` | 2 | rt(1), partitioning(1) |
| `convolution-2d.c` | 2 | core(2) |
| `atax.c` | 2 | core(2) |
| `layernorm.c` | 1 | core(1) |
| `batchnorm.c` | 1 | rt(1) |

**None of these tests test the C frontend.** They all use C purely as an
MLIR generator — the CHECK lines verify compiler pass behavior on the
generated MLIR, not C compilation correctness.

### Output/ Snapshot Problem

| Location | Files | Lines | Status |
|---|---|---|---|
| `rt/Output/` | 100 | 70,332 | Unused by any test |
| `core/Output/` | 58 | 29,739 | 1 test reads from here |
| `sde/Output/` | 26 | 18,137 | Unused by any test |
| `partitioning/safety/Output/` | 30 | 22,832 | Unused by any test |
| **Total** | **214** | **141,040** | **29:1 ratio vs test logic** |

The Output/ directories are artifacts of lit's in-source test execution
(`test_exec_root = test_source_root` in `lit.cfg.py`). When a test's RUN
line uses `CARTS_COMPILE_WORKDIR=%t.compile`, lit creates `Output/<test>.tmp.compile/`
alongside the test file. These snapshots were accidentally committed and
provide no regression value — they're machine-specific, contain absolute
paths, and are never checked.

**One exception**: `matmul_reduction_inputs_preserve_full_range.mlir` reads
MLIR from `Output/matmul_update_tiling_2mm.mlir.tmp.compile/2mm.mlir` — a
cross-test snapshot dependency that is fragile and non-portable.

---

## Industry Best Practices

### IREE Test Conventions

- **All tests are self-contained inline MLIR** — no external file dependencies
- **Tests co-located with code** — `test/` directory adjacent to the pass
- **Phase-boundary verification passes** — `iree-hal-verify-*` catches
  invariant violations automatically
- **Minimal hand-written IR** — each test contains only the ops relevant to
  the pass under test, with minimal surrounding boilerplate
- **`-split-input-file`** — multiple test cases in a single `.mlir` file,
  separated by `// -----`

### MLIR Upstream Conventions

- **Hand-written minimal IR** — never auto-generated from a frontend
- **`generate-test-checks.py`** — auto-generates CHECK lines from pass output
  (used for initial creation, then maintained by hand)
- **`mlir-reduce`** — reduces failing IR to minimal reproduction
- **`-split-input-file`** — standard for grouping related cases
- **No Output/ directories** — tests are pure input→output, no snapshots

### Flang/HLFIR Conventions

- **Source tests and IR tests separated** — C/Fortran tests test the frontend;
  MLIR tests test the passes
- **Each pass tested in isolation** — one `.mlir` file per pass behavior
- **No cross-test dependencies** — every test is independently runnable

### CIRCT Conventions

- **Layered by dialect** — `test/Dialect/HW/`, `test/Dialect/Comb/`, etc.
- **Conversion tests separate** — `test/Conversion/HWToLLVM/`
- **Negative tests explicit** — `test/Dialect/HW/errors.mlir`
- **No benchmark coupling** — hardware benchmarks are entirely separate

---

## What's Wrong with Benchmark-Coupled Tests

### 1. External dependency fragility

Tests fail if `external/carts-benchmarks/` submodule is not initialized,
checked out at wrong version, or source files change. A benchmark developer
changing `seidel-2d.c` can break 9 compiler regression tests.

### 2. Non-deterministic MLIR generation

Polygeist output varies by LLVM version, optimization level, and platform.
The same C file can produce different MLIR across environments, making CHECK
lines unreliable.

### 3. Slow test execution

Each benchmark-coupled test invokes Polygeist (C→MLIR) + the full CARTS
pipeline. Self-contained tests only invoke `carts-compile` on ready MLIR,
skipping the frontend entirely.

### 4. Conflated concerns

These tests don't test C compilation — they test specific pass behaviors
(partitioning modes, dep patterns, hoisting). The C source is merely an
inconvenient MLIR generator that obscures what's actually being tested.

### 5. Output/ snapshot pollution

141K lines of accidentally-committed snapshots that provide no regression
value, bloat the repo, and create confusion about what's tested.

---

## Migration Strategy

### Phase 1: Snapshot Decoupling (Effort: 4h)

**Goal**: Remove all Output/ directories and fix the one snapshot-dependent test.

1. **Fix `matmul_reduction_inputs_preserve_full_range.mlir`**: Replace the
   Output/ reference with an inline MLIR fixture or a shared input in
   `tests/contracts/inputs/`.

2. **Delete all Output/ directories**:
   ```bash
   git rm -r tests/contracts/*/Output tests/contracts/*/*/Output
   ```

3. **Add `.gitignore` in `tests/contracts/`**:
   ```
   # Lit test execution artifacts (generated in-source)
   Output/
   ```

4. **Verify**: `dekk carts test` — should show same 159/168 baseline.

**Impact**: -141,040 lines, -214 files, removes repo bloat.

### Phase 2: Staged Inline MLIR Conversion (Effort: 20h)

**Goal**: Convert 36 benchmark-coupled tests to self-contained inline MLIR.

**Method**: For each benchmark-coupled test:

1. Run the test's compile step manually to capture the MLIR at the pipeline
   stage just before the pass under test:
   ```bash
   dekk carts compile external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c \
     --pipeline <stage-before> --arts-config tests/contracts/inputs/arts_64t.cfg \
     -- [flags] > /tmp/full.mlir
   ```

2. Manually reduce the MLIR to the minimum needed to trigger the CHECK lines.
   Remove all functions, ops, and metadata that don't contribute to the
   tested behavior.

3. Replace the benchmark-coupled RUN line with a self-contained one:
   ```mlir
   // RUN: %carts-compile %s --pipeline <stage> --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s
   ```

4. Inline the reduced MLIR as the test body.

**Priority order** (most-coupled first):
1. `seidel-2d.c` tests (9 tests — highest coupling)
2. `rhs4sg_base.c` tests (8 tests)
3. `velocity_update.c` tests (5 tests)
4. `activations.c` tests (4 tests)
5. Remaining (10 tests)

**Quality bar**: Each converted test should be:
- < 100 lines of MLIR (ideally 30-60)
- Self-contained (no external dependencies)
- Testing ONE specific behavior
- Using descriptive CHECK labels if multiple cases exist

### Phase 3: Test Generation Tooling (Effort: 8h, optional)

**Goal**: Create tooling to prevent future benchmark coupling.

1. **`carts test-snapshot`** command: Captures pipeline-stage MLIR from any
   input and generates a skeleton `.mlir` test file with auto-generated
   CHECK lines (similar to MLIR's `generate-test-checks.py`).

2. **lit.local.cfg rule**: Add a CI check that rejects new tests with
   `external/carts-benchmarks` in RUN lines.

3. **`carts test-reduce`** command: Wraps `mlir-reduce` with CARTS-specific
   interestingness tests to automatically minimize failing IR.

---

## Verification Checklist

After each phase:

```bash
# All tests pass (159/168 baseline)
dekk carts test

# No benchmark references remain (after Phase 2)
grep -r 'external/carts-benchmarks' tests/contracts/ --include="*.mlir" | grep -v Output

# No Output/ dirs committed (after Phase 1)
git ls-files tests/contracts/*/Output tests/contracts/*/*/Output

# No cross-test dependencies
grep -r '%S.*Output/' tests/contracts/ --include="*.mlir" | grep -v '/Output/'
```

---

## Summary

| Metric | Before | After Phase 1 | After Phase 2 |
|---|---|---|---|
| Self-contained tests | 108 (64%) | 108 (64%) | 144 (86%) |
| Benchmark-coupled | 36 (21%) | 36 (21%) | 0 (0%) |
| Shared input tests | 23 (14%) | 24 (14%) | 24 (14%) |
| Output/ files | 214 | 0 | 0 |
| Output/ lines | 141,040 | 0 | 0 |
| External deps | carts-benchmarks | carts-benchmarks | none |
| Avg test execution | ~3s (bench) / <1s (inline) | same | <1s (all) |
