---
name: carts-analysis-triage
description: Use when behavior depends on pass order, stale facts, or metadata inconsistency across staged CARTS pipelines.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<pass-name | file>]
parameters:
  - name: pipeline_root
    type: str
    gather: "Path to the pipeline definition or pass file to analyze"
---

# CARTS Analysis Triage

Goal: determine whether the bug is caused by stale facts, a pass-ordering
assumption, or a transformation reading facts after it has invalidated them.

Use [[carts-vision]] when stale facts cross SDE/ARTS/ARTS-RT boundaries.
Do not recreate the retired ARTS cached graph stack; prefer stage diffs,
pass-local queries, and focused utilities.

Use bundled helpers when they fit:
- `scripts/find-analysis-usage.sh` — grep order-sensitive queries for a pass or token
- `scripts/find-invalidation-sites.sh` — grep invalidation APIs and direct invalidate calls
- `scripts/scan-analysis-hotspots.sh` — broad scan of order-sensitive analysis sites

Read these before editing invalidation behavior:
- `references/analysis-hotspots.md`
- `../debug/references/failure-signatures.md`
- `../debug/references/codepath-map.md`

## Triage Order

1. Confirm the order-sensitive symptom.
   - Does rerunning a pass change the result?
   - Does `--start-from` differ from a full pipeline run?
   - Does inserting a rebuild/invalidation make the bug disappear?
2. Identify what the pass reads and mutates.
   - Look for broad scans, cached maps, and queries after mutation.
   - Check whether the pass should keep a helper local or move it to a focused utility.
3. Check invalidation boundaries.
   - IR rewrites that erase or replace facts before all users consume them.
   - pipeline boundaries where a verifier should reject residual source-layer ops.
4. Prefer the narrowest correct fix.
   - add or correct `reads`
   - add or correct `invalidates`
   - move a query before mutation or after rebuild
   - replace blanket invalidation only when safety is preserved
5. Re-run the affected tests and any order-sensitive reproducer.

## Key Files

- `docs/compiler/phase-ordering-semantics.md`
- `tools/compile/Compile.cpp`
- `docs/compiler/pipeline.md`
- `docs/audits/2026-04-02-analysis-dependency-investigation.md`

## Guardrails

- Do not add a broad manager or graph cache to “fix” a stale-fact symptom.
- If a pass mutates DB/EDT structure, prove later queries are reading the new IR.
- Invalidation fixes must preserve committed-fact flow: SDE layout and movement
  facts, ARTS graph facts, ARTS owner routes/DB/EDT facts, and ARTS-RT
  mechanical lowering facts stay in their owning layers.

## Hand-off

- Wrong output caused by stale analysis -> `carts-miscompile-triage`
- Runtime crash after analysis fix -> `carts-runtime-triage`
- Need to verify op lifecycle across stages -> `/dialect-trace`
- Need pipeline stage comparison -> `/stage-diff`
- Need a regression test -> `/create-test`

## Validation

Close only after the minimal reproducer and the normal regression suite both agree.
