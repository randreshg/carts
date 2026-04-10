#!/usr/bin/env python3
"""WF11 — Regression Hunter.

Reproduces a regression, classifies it by subsystem, then routes to the
appropriate specialist via handoff_when.  Results are merged, verified,
and a post-mortem reflection is stored to memory.

Topology:
  reproduce  -->  classify  -->  handoff_when(dialect | runtime | codegen)
  -->  merge  -->  verify  -->  reflect  -->  update_memory
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude, codex

from .experts import (
    arts_runtime_expert,
    codegen_specialist,
    debugger_detective,
    mlir_dialect_engineer,
)


@compile()
def regression_hunter(g: GraphRecorder, failing_test: str, regression_commit: str):
    """Hunt down a regression by reproducing, classifying, and routing to the right specialist."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- spawn all agents ---
    detective = g.spawn("detective", profile=claude, cwd=cwd)
    dialect_agent = g.spawn("dialect_specialist", profile=claude, cwd=cwd)
    runtime_agent = g.spawn("runtime_specialist", profile=claude, cwd=cwd)
    codegen_agent = g.spawn("codegen_specialist", profile=codex, cwd=cwd)

    # --- reproduce the regression ---
    reproduction = g.ask(
        "Reproduce and characterize this regression.\n\n"
        "Failing test: {failing_test}\n"
        "Regression commit: {regression_commit}\n\n"
        "Steps:\n"
        "1. Run the failing test and capture exact output\n"
        "2. Check out the parent commit and confirm the test passes\n"
        "3. Compare the diff of the regression commit\n"
        "4. Capture the IR at each pipeline stage for both passing and failing versions\n"
        "5. Identify the first stage where output diverges\n"
        "6. Produce a minimal reproducer .mlir file",
        agent=debugger_detective,
    )

    # --- classify by subsystem ---
    classification = g.ask(
        "Classify this regression into one of three subsystems based on the reproduction "
        "analysis below.\n\n"
        "Reproduction report:\n{reproduction}\n\n"
        "Classify as exactly ONE of:\n"
        "- 'dialect': The bug is in ArtsOps.td/ArtsTypes.td definitions, canonicalization "
        "patterns, DRR rewrite rules, or ODS-generated code\n"
        "- 'runtime': The bug is in EDT lifecycle, DB access modes, epoch handling, CPS chains, "
        "or ARTS runtime call generation in lowering passes\n"
        "- 'codegen': The bug is in LLVM lowering, type conversion, operation legalization, "
        "or C++ pass implementation logic\n\n"
        "Output ONLY the subsystem label on the first line, followed by your reasoning.",
        agent=debugger_detective,
    )

    # --- route to the right specialist ---
    g.handoff_when(
        from_agent=classification,
        routes={
            "dialect": dialect_agent,
            "runtime": runtime_agent,
            "codegen": codegen_agent,
        },
    )

    # --- specialist investigations (each activated by handoff) ---
    dialect_fix = g.ask(
        "Fix this dialect-level regression.\n\n"
        "Reproduction:\n{reproduction}\n"
        "Classification:\n{classification}\n\n"
        "Focus on:\n"
        "- Check ArtsOps.td and ArtsTypes.td for changed operation semantics\n"
        "- Review canonicalization patterns for incorrect rewrites\n"
        "- Verify DRR pattern preconditions haven't been weakened\n"
        "- Check PatternBenefit ordering for priority inversions\n"
        "- Produce a fix with updated ODS/TableGen and C++ implementation",
        agent=mlir_dialect_engineer,
    )

    runtime_fix = g.ask(
        "Fix this runtime-level regression.\n\n"
        "Reproduction:\n{reproduction}\n"
        "Classification:\n{classification}\n\n"
        "Focus on:\n"
        "- Trace EDT lifecycle state transitions for correctness\n"
        "- Verify DB mode constants (ARTS_DB_PIN=2, ARTS_DB_ONCE=3) usage\n"
        "- Check epoch_create/signal/wait balance\n"
        "- Validate CPS chain continuations\n"
        "- Verify no AnalysisManager thread-safety violations\n"
        "- Produce a fix in lib/arts/passes/transforms/",
        agent=arts_runtime_expert,
    )

    codegen_fix = g.ask(
        "Fix this codegen-level regression.\n\n"
        "Reproduction:\n{reproduction}\n"
        "Classification:\n{classification}\n\n"
        "Focus on:\n"
        "- Check dialect conversion patterns for type mismatches\n"
        "- Review operation legalization for missing cases\n"
        "- Verify LLVM IR generation for correctness\n"
        "- Check AttrNames::Operation constant usage (no hardcoded strings)\n"
        "- Ensure thread-safety: no global/static mutable state\n"
        "- Produce complete, compilable C++ fix with headers and namespace",
        agent=codegen_specialist,
    )

    # --- merge all specialist outputs ---
    merged = g.merge("specialist_results", [dialect_fix, runtime_fix, codegen_fix])

    # --- verify the fix ---
    verified = g.verify(
        claim="The regression fix resolves the failing test without introducing new failures",
        evidence=(
            "Merged specialist analysis:\n{merged}\n\n"
            "Verify:\n"
            "1. The fix resolves the original failing test: {failing_test}\n"
            "2. The fix resolves the minimal reproducer from the reproduction step\n"
            "3. No new regressions: existing lit tests in tests/contracts/ still pass\n"
            "4. The fix is minimal — no unnecessary refactoring\n"
            "5. LLVM coding style is preserved"
        ),
    )

    # --- post-mortem reflection ---
    postmortem = g.reflect(trace_id="regression_hunter")

    # --- persist findings ---
    g.update_memory(
        data=verified,
        key="regression_hunt",
        space="episodic",
    )

    g.print(
        "=== Regression Hunt Complete ===\n\n"
        "Failing test: {failing_test}\n"
        "Regression commit: {regression_commit}\n\n"
        "Fix:\n{merged}\n\n"
        "Verification:\n{verified}\n\n"
        "Post-mortem:\n{postmortem}"
    )
    g.done(verified)


if __name__ == "__main__":
    print(regression_hunter._graph.to_air())
