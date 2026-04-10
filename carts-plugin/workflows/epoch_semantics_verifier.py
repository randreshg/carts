#!/usr/bin/env python3
"""WF12 -- Epoch Semantics Verifier.

Formally verifies four epoch invariants through a chain of sequential
verify operations, each checking a specific property:

  1. Completeness: every epoch_create has a matching epoch_wait, and
     the signal count matches the expected EDT count.
  2. Isolation: no DataBlock is shared across epoch boundaries without
     explicit migration or re-acquisition.
  3. Ordering: epoch_wait completes before any post-epoch EDT executes,
     and epoch_signal only fires after the signaling EDT completes its work.
  4. No nested deadlocks: inner epoch_wait does not block an outer
     epoch_signal, preventing circular barrier dependencies.

A reasoning step aggregates all four verdicts, a guard gates on the
overall result, and findings are stored in shared memory.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import arts_runtime_expert, concurrency_analyst


@compile()
def epoch_semantics_verifier(g: GraphRecorder, program_file: str):
    """Verify epoch synchronization invariants in CARTS-compiled programs."""

    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn two specialist agents ---
    runtime_agent = g.spawn("runtime_expert", profile=claude, cwd=cwd)
    concurrency_agent = g.spawn("concurrency_expert", profile=claude, cwd=cwd)

    # --- Extract epoch usage from source ---
    epoch_extraction = g.ask(
        "Extract all epoch-related operations from {program_file}.\n\n"
        "For each epoch in the program:\n"
        "  1. Epoch ID (from arts.epoch_create)\n"
        "  2. Source location of epoch_create\n"
        "  3. All arts.epoch_signal calls referencing this epoch, with:\n"
        "     - Signaling EDT ID\n"
        "     - Source location\n"
        "  4. The arts.epoch_wait call for this epoch, with:\n"
        "     - Waiting EDT ID\n"
        "     - Source location\n"
        "  5. Expected signal count (from epoch_create attributes or inferred "
        "from EDT fan-out)\n"
        "  6. Nesting depth (is this epoch inside another epoch's scope?)\n"
        "  7. DataBlocks accessed by EDTs within this epoch's scope\n\n"
        "Return a structured JSON array of epoch descriptors.",
        agent=arts_runtime_expert,
    )

    # --- Verify #1: Completeness ---
    v_completeness = g.verify(
        claim=(
            "Every epoch_create has exactly one matching epoch_wait, and the "
            "number of epoch_signal calls equals the expected signal count "
            "declared at epoch_create time. No orphaned epochs exist (created "
            "but never waited on), and no phantom waits exist (waiting on an "
            "epoch that was never created)."
        ),
        evidence="{epoch_extraction}",
        agent=arts_runtime_expert,
    )

    # --- Verify #2: Isolation (depends on completeness) ---
    v_isolation = g.verify(
        claim=(
            "No DataBlock is accessed by EDTs in different epoch scopes without "
            "explicit re-acquisition. Specifically: if EDT_A in epoch_1 acquires "
            "DB_X as RW, and EDT_B in epoch_2 also acquires DB_X, there must be "
            "an epoch_wait(epoch_1) that happens-before EDT_B's acquire. No DB "
            "handle leaks across epoch boundaries."
        ),
        evidence=(
            "Epoch data:\n{epoch_extraction}\n\n"
            "Completeness verdict:\n{v_completeness}"
        ),
        agent=concurrency_analyst,
    )

    # --- Verify #3: Ordering (depends on isolation) ---
    v_ordering = g.verify(
        claim=(
            "Epoch ordering is correctly enforced: (a) epoch_wait blocks until "
            "all expected signals arrive -- no early unblock, (b) epoch_signal "
            "is only called after the signaling EDT has completed all DB releases "
            "-- no signal-before-release, (c) no EDT scheduled after epoch_wait "
            "can begin execution before the wait completes -- the barrier is a "
            "true execution fence."
        ),
        evidence=(
            "Epoch data:\n{epoch_extraction}\n\n"
            "Isolation verdict:\n{v_isolation}"
        ),
        agent=arts_runtime_expert,
    )

    # --- Verify #4: No nested deadlocks (depends on ordering) ---
    v_no_deadlock = g.verify(
        claim=(
            "No nested epoch configuration can deadlock. Specifically: if "
            "epoch_inner is nested inside epoch_outer's scope, then "
            "epoch_wait(epoch_inner) does not block any EDT that needs to call "
            "epoch_signal(epoch_outer). The inner barrier must complete without "
            "requiring progress from the outer barrier, and vice versa. No "
            "circular dependency exists in the epoch nesting hierarchy."
        ),
        evidence=(
            "Epoch data:\n{epoch_extraction}\n\n"
            "Ordering verdict:\n{v_ordering}"
        ),
        agent=concurrency_analyst,
    )

    # --- Aggregate reasoning across all four verdicts ---
    aggregate = g.reason(
        "Aggregate the four epoch invariant verification results into a unified "
        "assessment.\n\n"
        "=== Completeness ===\n{v_completeness}\n\n"
        "=== Isolation ===\n{v_isolation}\n\n"
        "=== Ordering ===\n{v_ordering}\n\n"
        "=== No Nested Deadlocks ===\n{v_no_deadlock}\n\n"
        "Produce:\n"
        "  1. Overall verdict: PASS (all four invariants hold), FAIL (at least "
        "one invariant violated), or CONDITIONAL (invariants hold under stated "
        "assumptions)\n"
        "  2. For each invariant: PASS/FAIL with confidence level and evidence "
        "summary\n"
        "  3. If any invariant fails: root cause analysis tracing back to the "
        "specific epoch_create/signal/wait operations that violate it\n"
        "  4. Recommendations: specific code changes to fix violations\n\n"
        "The overall verdict must be FAIL if any individual invariant is FAIL.",
        agent=arts_runtime_expert,
    )

    # --- Guard: halt if epoch invariants are violated ---
    g.guard(
        condition="The overall epoch semantics verdict is PASS or CONDITIONAL (not FAIL)",
        on_fail="halt",
        source=aggregate,
    )

    # --- Store results ---
    g.update_memory(
        data=aggregate,
        key="epoch_semantics_verification",
        space="episodic",
    )

    g.print("=== Epoch Semantics Verification ===\n{aggregate}")
    g.done(aggregate)


if __name__ == "__main__":
    print(epoch_semantics_verifier._graph.to_air())
