# Debug Codepath Map

High-value files by failure class.

## Shared Compiler Entry Points

- `tools/compile/Compile.cpp`
- `docs/compiler/pipeline.md`
- `docs/heuristics/partitioning.md`
- `docs/heuristics/distribution.md`

## Miscompile / Semantic Drift

- `lib/carts/dialect/sde/Transforms/CollectMetadata.cpp`
- `lib/carts/dialect/arts/Transforms/SdeToArtsBoundary.cpp`
- `lib/carts/dialect/arts/Transforms/db/DbModeTightening.cpp`
- `lib/carts/dialect/arts/Transforms/db/CreateDbs.cpp`
- `lib/carts/dialect/sde/Transforms/effect/distribution/DistributionPlanning.cpp`
- `lib/carts/dialect/arts/Utils/LoweringFactUtils.cpp`
- `include/carts/dialect/arts/IR/Ops.td`
- `include/carts/dialect/arts/IR/Attributes.td`
- `include/carts/utils/StencilAttributes.h`

## Runtime / Epoch / Lowering

- `lib/carts/dialect/arts-rt/Conversion/ArtsRtToLLVM/ConvertArtsRtToLLVM.cpp`
- `lib/carts/dialect/arts-rt/Conversion/ArtsToRt/EdtLowering.cpp` (pre-lowering)
- `lib/carts/dialect/arts-rt/Conversion/ArtsToRt/EpochLowering.cpp` (pre-lowering)
- `external/arts/`

## Distributed Ownership / Multi-Node

- `lib/carts/dialect/arts/Transforms/db/DbOwnerMapRealization.cpp`
- `lib/carts/codegen/Codegen.cpp`
- `lib/carts/dialect/arts-rt/Conversion/ArtsRtToLLVM/ConvertArtsRtToLLVM.cpp`

## Pass Ordering / Fact Flow

- `docs/compiler/phase-ordering-semantics.md`
- `tools/compile/Compile.cpp`
- `docs/compiler/pipeline.md`

## Benchmark Harness / Artifacts

- `tools/scripts/triage.py`
- `external/carts-benchmarks/common/carts.mk`
- `docs/benchmarks/`
