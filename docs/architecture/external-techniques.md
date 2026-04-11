# External Techniques and Their Real Status

This document tracks which outside compiler and runtime ideas are actually
visible in the current CARTS SDE work, which ones only influence the design,
and which ones remain future work.

The status labels below are intentionally strict:

- `Implemented`: concrete behavior or structure exists in the current branch
- `Partial`: some ideas are reflected in the implementation, but only in a
  narrower form
- `Planned`: discussed in architecture, not implemented in code
- `Reference only`: useful comparison, but not currently adopted

## Current Status Matrix

| Technique | Source | Status | Real status in CARTS |
|---|---|---|---|
| Progressive dialect staging | IREE, CIRCT | Implemented | CARTS now has a real SDE optimization boundary ahead of ARTS lowering, with explicit verification boundaries after lowering stages. |
| Algorithm/schedule separation | Halide | Partial | The branch separates compute-region and scheduling concepts in SDE, but there is not yet a first-class schedule language or transform-IR schedule program. |
| Schedule as IR | MLIR Transform dialect | Planned | No transform-dialect schedule emission or application exists in the current SDE pipeline. |
| Parameterized task-family style | PaRSEC PTG | Partial | `arts_sde.su_iterate` captures parameterized loop families and supports symbolic loop reasoning, but CARTS does not yet materialize a first-class symbolic task-graph language. |
| Deferred structural raising before lowering | Flang HLFIR | Partial | The branch uses transient linalg and tensor carriers to defer some structural decisions, but there is no full HLFIR-style abstract array layer or standalone bufferization phase in the SDE pipeline. |
| Stencil-oriented structured analysis | Open Earth Compiler | Partial | Stencil structure is recognized and carried as classification, and some tensor/linalg staging exists, but there is no dedicated high-level stencil dialect or full stencil-specific execution model in SDE. |
| Declarative dependency intent | Charm++ SDAG, StarPU data handles | Partial | Task dependencies are expressed explicitly through `arts_sde.mu_dep` and `arts_sde.cu_task`, but CARTS does not yet have a general declarative dependency language or a StarPU-like coherence handle system. |
| Tensor/linalg as optimization substrate | MLIR linalg/tensor ecosystem | Implemented | `RaiseToLinalg`, `RaiseToTensor`, and `SdeTensorOptimization` now perform real SDE-stage transformations on supported subsets rather than only classification. |
| Runtime-calibrated scheduling decisions | Cost-model-driven systems broadly | Partial | SDE schedule refinement, chunk selection, scope selection, and tensor tiling use the SDE cost-model interface, but broader core heuristics are still not consistently driven by the same model. |
| Logical regions and privileges | Legion | Planned | CARTS does not currently implement Legion-style logical regions, privilege sets, or hierarchical partition objects in SDE. |
| Extensible distribution interface | Chapel DSI | Planned | There is no implemented `sde.su_distribute` op or domain-map-like interface in the current dialect. |
| Functional array combinators | Lift/RISE | Reference only | Useful conceptual comparison for structured tensor computation, but not an implemented substrate in CARTS. |
| Tensor micro-kernel named ops | TPP-MLIR | Reference only | CARTS does not currently define TPP-style named micro-kernel ops or a fused micro-kernel layer. |
| Temporal blocking metadata | Devito | Planned | No temporal-reuse metadata or temporal blocking transform exists in current SDE passes. |

## What Is Implemented Now

These external ideas are concretely reflected in the current branch:

- staged dialect progression as an architectural pattern
- tensor/linalg as a temporary but real optimization substrate inside SDE
- partial algorithm/schedule separation through SDE compute and scheduling ops
- partial parameterized-task-family reasoning through `arts_sde.su_iterate`
- partial cost-model-guided decisions inside SDE passes

## What Is Still Only Planned

These techniques should not be described as adopted in current docs:

- transform-dialect schedule programs
- Chapel-style extensible distribution interfaces
- Legion-style logical regions
- Devito-style temporal blocking metadata
- standalone backend-neutral distribution semantics
- TPP-style named micro-kernel layers

## Notes on Honest Positioning

Two corrections matter for architecture accuracy:

- CARTS should not claim that Legion regions or Chapel DSI are already adopted.
  They are design references, not implemented features.
- CARTS can honestly claim that tensor/linalg techniques are now used for real
  SDE-stage transformations on supported subsets, but not that the entire SDE
  layer is fully tensor-native.
