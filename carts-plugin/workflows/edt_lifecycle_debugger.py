#!/usr/bin/env python3
"""WF2 — EDT Lifecycle Debugger.

Three-way parallel investigation of an EDT lifecycle bug using a debugger
detective, ARTS runtime expert, and concurrency analyst.  The agents
independently investigate, then negotiate consensus on root cause, verify
the proposed fix, and gate on a human review checkpoint before persisting
findings to memory.

Topology:
  3x parallel investigation  -->  negotiate root cause  -->  verify fix
  -->  guard(halt)  -->  pause(human review)  -->  update_memory
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import arts_runtime_expert, concurrency_analyst, debugger_detective


@compile()
def edt_lifecycle_debugger(g: GraphRecorder, bug_description: str, trace_file: str):
    """Debug an EDT lifecycle issue through multi-agent investigation and negotiated consensus."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- spawn the investigation team ---
    team = g.team("debug_team")
    detective_spawn = team.add("detective", profile=claude, cwd=cwd)
    runtime_spawn = team.add("runtime_expert", profile=claude, cwd=cwd)
    concurrency_spawn = team.add("concurrency_expert", profile=claude, cwd=cwd)

    # --- three parallel investigations ---
    detective_analysis = g.ask(
        "Investigate this EDT lifecycle bug using your 7-step debugging methodology.\n\n"
        "Bug description: {bug_description}\n"
        "Trace file: {trace_file}\n\n"
        "Steps:\n"
        "1. Reproduce the failure from the trace file\n"
        "2. Classify: crash, miscompile, hang, or assertion failure?\n"
        "3. Bisect the pipeline to find the first divergent stage\n"
        "4. Inspect the EDT lifecycle state transitions in the divergent IR\n"
        "5. Identify the specific EDT operation (create/schedule/execute/complete/destroy) "
        "where behavior deviates from spec\n"
        "6. Construct a minimal reproducer\n"
        "7. Propose a root cause hypothesis with supporting evidence",
        agent=debugger_detective,
    )

    runtime_analysis = g.ask(
        "Analyze this EDT lifecycle bug from the ARTS runtime perspective.\n\n"
        "Bug description: {bug_description}\n"
        "Trace file: {trace_file}\n\n"
        "Focus on:\n"
        "- EDT state machine transitions: is the lifecycle sequence valid?\n"
        "- DataBlock access modes at each EDT lifecycle stage\n"
        "- CPS chain integrity: are continuations correctly wired?\n"
        "- DB mode constants (ARTS_DB_PIN=2, ARTS_DB_ONCE=3) — are they used correctly?\n"
        "- Epoch scoping around the failing EDT: is epoch_signal/epoch_wait balanced?\n"
        "- Runtime configuration (arts.cfg) that might affect EDT scheduling",
        agent=arts_runtime_expert,
    )

    concurrency_analysis = g.ask(
        "Perform concurrency analysis on this EDT lifecycle bug.\n\n"
        "Bug description: {bug_description}\n"
        "Trace file: {trace_file}\n\n"
        "Analysis steps:\n"
        "1. Build the happens-before graph from the trace\n"
        "2. Identify all EDT pairs that can execute concurrently\n"
        "3. Overlay DB access modes on each concurrent pair\n"
        "4. Check for RO||RW or RW||RW conflicts\n"
        "5. Verify epoch barrier correctness: no cross-epoch DB sharing without migration\n"
        "6. Check CPS chain ordering: continuations see completed writes\n"
        "7. Detect nested epoch deadlock potential (inner wait blocks outer signal)",
        agent=concurrency_analyst,
    )

    # --- negotiate consensus on root cause ---
    root_cause = g.negotiate(
        parties=["detective", "runtime_expert", "concurrency_expert"],
        proposal=(
            "Based on the three independent analyses, negotiate a consensus root cause "
            "for this EDT lifecycle bug.\n\n"
            "Detective findings:\n{detective_analysis}\n\n"
            "Runtime analysis:\n{runtime_analysis}\n\n"
            "Concurrency analysis:\n{concurrency_analysis}\n\n"
            "Resolve any conflicting hypotheses. Produce a single agreed-upon:\n"
            "1. Root cause statement\n"
            "2. Affected pipeline stage(s)\n"
            "3. Specific EDT lifecycle violation\n"
            "4. Proposed fix with code location"
        ),
        max_rounds=3,
    )

    # --- verify the proposed fix ---
    fix_verified = g.verify(
        claim="The proposed fix resolves the EDT lifecycle bug without introducing regressions",
        evidence=(
            "Root cause and proposed fix:\n{root_cause}\n\n"
            "Verify by checking:\n"
            "1. The fix addresses the exact lifecycle state transition error\n"
            "2. No new DB access mode conflicts are introduced\n"
            "3. CPS chain ordering is preserved post-fix\n"
            "4. The fix handles edge cases: empty loops, single-element DBs, zero-trip loops\n"
            "5. The fix is consistent with the reduced test case from the detective's analysis"
        ),
    )

    # --- guard: halt if verification fails ---
    g.guard(
        condition="Fix verification passed and no regressions detected",
        on_fail="halt",
        source=fix_verified,
    )

    # --- pause for human review ---
    g.pause(
        message=(
            "EDT lifecycle debugging complete. Review the negotiated root cause and "
            "verified fix before applying changes.\n\n"
            "Root cause: {root_cause}\n"
            "Verification: {fix_verified}"
        ),
        checkpoint_id="edt_debug_review",
    )

    # --- persist findings ---
    g.update_memory(
        data=root_cause,
        key="edt_lifecycle_debug",
        space="episodic",
    )

    g.print(
        "=== EDT Lifecycle Debug Complete ===\n\n"
        "Root cause:\n{root_cause}\n\n"
        "Verification:\n{fix_verified}"
    )
    g.done(fix_verified)


if __name__ == "__main__":
    print(edt_lifecycle_debugger._graph.to_air())
