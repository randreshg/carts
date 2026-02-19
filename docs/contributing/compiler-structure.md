# Compiler Structure

This document defines layering boundaries for compiler code in `lib/arts/`.

## Layering Rules

- `Utils/`
  - Scope: generic helpers with no pass policy.
  - Allowed: SSA/value normalization, conservative identity helpers, metadata accessors.
  - Not allowed: workload-specific pattern decisions.
- `Analysis/`
  - Scope: classification and query APIs used by passes/transforms.
  - Allowed: pattern matching (`*Pattern*`), graph/alias analysis, machine-aware heuristics.
  - Not allowed: IR mutation.
- `Transforms/`
  - Scope: localized IR rewrites and lowering building blocks.
  - Allowed: rewrite implementations and transformation utilities.
  - Not allowed: top-level pipeline policy.
- `Passes/`
  - Scope: orchestration and policy.
  - Allowed: sequencing analyses/transforms and setting pass-level gates.
  - Not allowed: duplicate pattern matching logic that belongs in `Analysis/*Pattern*`.

## Practical Rules

- If two passes need the same matcher, move it to `Analysis/Db/DbPatternMatchers.*`.
- If helper behavior is not pass-specific, move it to `Utils/`.
- Keep machine topology resolution through `ArtsAbstractMachine` and `DistributionHeuristics`.
- Keep pass files small: evaluate policy, call shared interfaces, apply transforms.

## Review Checklist

- Does this code duplicate an existing matcher/helper?
- Is IR mutation happening inside `Analysis/`? If yes, move it.
- Is pass-local policy leaking into shared utility code?
- Are documented flags and pass behavior still consistent?
