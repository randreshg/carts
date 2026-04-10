# Analysis Hotspots

Start here when the symptom depends on pass order, graph freshness, or stale metadata.

## Core APIs

- `include/arts/dialect/core/Analysis/AnalysisDependencies.h`
- `include/arts/dialect/core/Analysis/AnalysisManager.h`
- `lib/arts/dialect/core/Analysis/AnalysisManager.cpp`

## High-Risk Docs

- `docs/compiler/phase-ordering-semantics.md`
- `docs/plans/phase-ordering-design.md`
- `docs/audits/2026-04-02-analysis-dependency-investigation.md`
- `docs/audits/2026-04-02-am-thread-safety-audit.md`

## High-Risk Patterns

- `AM->invalidate()`
- `invalidateAndRebuildGraphs`
- direct `dbAnalysis.invalidate()` or `edtAnalysis.invalidate()`
- missing or stale `k<Pass>_reads`
- queries after mutation without rebuild
- `--start-from` behavior that differs from a full pipeline run
