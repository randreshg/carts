# Debug Codepath Map

High-value files by failure class.

## Shared Compiler Entry Points

- `tools/compile/Compile.cpp`
- `docs/compiler/pipeline.md`
- `docs/heuristics/partitioning.md`
- `docs/heuristics/distribution.md`

## Miscompile / Semantic Drift

- `lib/arts/passes/transforms/PatternDiscovery.cpp`
- `lib/arts/passes/transforms/ForLowering.cpp`
- `lib/arts/passes/opt/db/DbPartitioning.cpp`
- `lib/arts/utils/LoweringContractUtils.cpp`
- `lib/arts/utils/PatternSemantics.cpp`

## Runtime / Epoch / Lowering

- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`
- `lib/arts/passes/transforms/EdtLowering.cpp`
- `lib/arts/passes/opt/edt/`
- `external/arts/`

## Distributed Ownership / Multi-Node

- `lib/arts/passes/opt/db/DbDistributedOwnership.cpp`
- `lib/arts/passes/opt/loop/DistributedHostLoopOutlining.cpp`
- `lib/arts/Dialect/Codegen.cpp`
- `lib/arts/passes/transforms/ConvertArtsToLLVM.cpp`

## Analysis / Invalidation / Ordering

- `include/arts/analysis/AnalysisDependencies.h`
- `include/arts/analysis/AnalysisManager.h`
- `lib/arts/analysis/AnalysisManager.cpp`
- `docs/compiler/phase-ordering-semantics.md`
- `docs/plans/phase-ordering-design.md`

## Benchmark Harness / Artifacts

- `tools/scripts/triage.py`
- `external/carts-benchmarks/common/carts.mk`
- `docs/benchmarks/`
