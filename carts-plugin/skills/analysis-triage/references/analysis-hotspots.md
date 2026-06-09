# Analysis Hotspots

Start here when the symptom depends on pass order, stale facts, or metadata
consistency across stage boundaries.

## ARTS APIs

- `include/carts/dialect/arts/Utils/DbUtils.h`
- `include/carts/dialect/arts/Utils/EdtUtils.h`
- `include/carts/dialect/arts/Utils/LoweringFactUtils.h`
- `lib/carts/dialect/arts/Transforms/`

## High-Risk Docs

- `docs/compiler/phase-ordering-semantics.md`
- `tools/compile/Compile.cpp`
- `docs/compiler/pipeline.md`
- `docs/audits/2026-04-02-analysis-dependency-investigation.md`
- `docs/audits/2026-04-02-am-thread-safety-audit.md`

## High-Risk Patterns

- queries after mutation without rereading the rewritten IR
- stale stage names or pass manifests
- source-layer facts surviving past a dialect boundary
- `--start-from` behavior that differs from a full pipeline run
