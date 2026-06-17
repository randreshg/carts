# MLIR Pass Anatomy

Healthy CARTS passes should look like MLIR passes: a narrow orchestration point
that owns one transform or verifier and delegates reusable algorithms to named
helpers.

## Evidence

- `SymbolDCE` declares one pass struct and one `runOnOperation` entry point:
  `external/Polygeist/llvm-project/mlir/lib/Transforms/SymbolDCE.cpp:31`,
  `external/Polygeist/llvm-project/mlir/lib/Transforms/SymbolDCE.cpp:33`,
  `external/Polygeist/llvm-project/mlir/lib/Transforms/SymbolDCE.cpp:45`.
- Its non-trivial liveness algorithm is a named helper rather than mixed into a
  second unrelated transform:
  `external/Polygeist/llvm-project/mlir/lib/Transforms/SymbolDCE.cpp:38`,
  `external/Polygeist/llvm-project/mlir/lib/Transforms/SymbolDCE.cpp:63`,
  `external/Polygeist/llvm-project/mlir/lib/Transforms/SymbolDCE.cpp:66`.
- `Canonicalizer` is a compact wrapper around pattern application and state:
  `external/Polygeist/llvm-project/mlir/lib/Transforms/Canonicalizer.cpp:61`,
  `external/Polygeist/llvm-project/mlir/lib/Transforms/Canonicalizer.cpp:63`,
  `external/Polygeist/llvm-project/mlir/lib/Transforms/Canonicalizer.cpp:68`.
- The MLIR pass runner invokes the virtual pass entry point; a pass has one
  operational throat to audit:
  `external/Polygeist/llvm-project/mlir/lib/Pass/Pass.cpp:608`,
  `external/Polygeist/llvm-project/mlir/lib/Pass/Pass.cpp:612`,
  `external/Polygeist/llvm-project/mlir/lib/Pass/Pass.cpp:613`.

## CARTS Split-Axis Test

When a file crosses roughly 1000 lines, ask:

1. Is it one transform, one verifier, one analysis, or one mechanical lowering?
2. Could a reusable analysis/helper move to `Utils/` without changing ownership?
3. Is pass-order knowledge encoded in helper calls or raw attrs?
4. Can a focused lit test name the one behavior this file owns?

Keep a large file only when the answer shows one cohesive algorithm. Otherwise
split by responsibility, not by arbitrary line ranges.
