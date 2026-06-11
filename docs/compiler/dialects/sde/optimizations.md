# SDE Optimizations

SDE optimizations are semantic and target-neutral. They may change MU/CU/SU
shape only when SDE analyses prove legality.

Owned optimizations:

- loop interchange backed by structured access facts;
- tiling that rewrites scheduling shape and MU token windows together;
- elementwise fusion when root windows and effects are compatible;
- schedule refinement and chunk selection using logical resource facts;
- distribution intent, never ARTS placement;
- iteration-space decomposition;
- barrier elimination and timestep stage-boundary planning;
- reduction strategy selection;
- memory-unit materialization.

Rules:

- Do not materialize ARTS worker counts, routes, nodes, or runtime calls.
- Do not tile only CU/SU loops without matching MU token and access rewrites.
- Do not use benchmark-specific constants.
- Do not keep frontend carrier paths once memref MU/token coverage exists.

Exit facts for ARTS:

- token windows;
- codelet boundary requirements;
- scalar capture candidates;
- local values that should be reconstructed inside codelets;
- unsupported diagnostics when legality is not proven.
