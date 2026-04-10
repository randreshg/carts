#!/usr/bin/env python3
"""WF8 -- DataBlock Access Mode Safety Auditor.

Cross-references static DataBlock access mode declarations (RO/RW/REDUCE/COMMUTE)
against the ARTS runtime concurrency model to detect unsafe access patterns.

Two parallel analysis branches:
  - Static analysis: extract DB access modes from source IR
  - Runtime model analysis: infer concurrent EDT scheduling from the code structure

A reasoning step cross-references both analyses, a verify op validates the
findings, and a guard gates on a definitive safety verdict (halts on unsafe).
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import arts_runtime_expert, concurrency_analyst


@compile()
def datablock_safety_auditor(g: GraphRecorder, source_file: str):
    """Audit DataBlock access modes for concurrency safety."""

    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn two specialist agents ---
    static_agent = g.spawn("static_analyst", profile=claude, cwd=cwd)
    runtime_agent = g.spawn("runtime_modeler", profile=claude, cwd=cwd)

    # --- Parallel analysis branches ---
    static_analysis = g.ask(
        "Perform static analysis of DataBlock access modes in {source_file}.\n\n"
        "For every arts.db_acquire operation in the source IR:\n"
        "  1. Extract the DB identifier (SSA value or named DB)\n"
        "  2. Extract the access mode: RO, RW, REDUCE, or COMMUTE\n"
        "  3. Identify the enclosing EDT that performs this acquire\n"
        "  4. Identify the enclosing epoch scope (if any)\n"
        "  5. Track the matching arts.db_release\n\n"
        "Produce a table: [DB_ID, EDT_ID, Mode, Epoch_Scope, Acquire_Line, Release_Line]\n\n"
        "Also flag any anomalies:\n"
        "  - Acquire without matching release\n"
        "  - Multiple acquires of the same DB within one EDT\n"
        "  - Mode changes between acquire and release (should never happen)\n"
        "  - RW access on a DB that could safely be RO (overly permissive mode)",
        agent=concurrency_analyst,
    )

    runtime_model = g.ask(
        "Build the EDT concurrency model for {source_file}.\n\n"
        "Analyze the source to determine which EDTs can execute concurrently:\n"
        "  1. Build the EDT dependency graph from CPS chains and epoch barriers\n"
        "  2. Identify EDT pairs that are NOT ordered by any dependency\n"
        "  3. For each unordered pair, list the DataBlocks both EDTs access\n"
        "  4. For shared DataBlocks, record both access modes\n\n"
        "Produce:\n"
        "  - EDT dependency graph (adjacency list with edge type: cps/epoch/data)\n"
        "  - Concurrent EDT pairs: [(EDT_A, EDT_B, shared_DBs)]\n"
        "  - For each shared DB: (DB_ID, mode_in_A, mode_in_B)\n\n"
        "Use the ARTS mode compatibility rules:\n"
        "  - RO || RO => SAFE\n"
        "  - RO || RW => UNSAFE (data race)\n"
        "  - RW || RW => UNSAFE (data race)\n"
        "  - REDUCE || REDUCE => SAFE (runtime handles reduction)\n"
        "  - COMMUTE || COMMUTE => SAFE (runtime serializes)\n"
        "  - REDUCE || RW => UNSAFE\n"
        "  - COMMUTE || RW => UNSAFE",
        agent=arts_runtime_expert,
    )

    # --- Cross-reference static modes against runtime concurrency model ---
    cross_reference = g.reason(
        "Cross-reference the static DB access modes with the runtime concurrency "
        "model to identify definitive safety violations.\n\n"
        "=== Static Analysis (DB access modes per EDT) ===\n{static_analysis}\n\n"
        "=== Runtime Concurrency Model (which EDTs run concurrently) ===\n{runtime_model}\n\n"
        "For each concurrent EDT pair that shares a DataBlock:\n"
        "  1. Look up the static access modes from the first analysis\n"
        "  2. Apply the ARTS mode compatibility matrix\n"
        "  3. Classify as SAFE, UNSAFE, or SUSPICIOUS\n\n"
        "UNSAFE = definitive data race (e.g., RO||RW on same DB in concurrent EDTs)\n"
        "SUSPICIOUS = potentially unsafe depending on runtime scheduling "
        "(e.g., modes are safe but epoch scoping is ambiguous)\n\n"
        "Produce a final verdict: SAFE (no issues), UNSAFE (at least one definitive "
        "race), or SUSPICIOUS (potential issues requiring manual review).\n\n"
        "For each UNSAFE finding, provide:\n"
        "  - The two EDT IDs and the shared DB ID\n"
        "  - Both access modes and why they conflict\n"
        "  - Source line references\n"
        "  - Suggested fix (e.g., add epoch barrier, downgrade RW to RO, partition DB)",
        agent=concurrency_analyst,
    )

    # --- Verify the cross-reference analysis ---
    verified = g.verify(
        claim=(
            "The safety audit correctly applies the ARTS mode compatibility "
            "matrix: every UNSAFE finding involves a provably concurrent EDT pair "
            "with incompatible DB access modes, and no false negatives exist for "
            "RO||RW or RW||RW conflicts among concurrent EDTs."
        ),
        evidence="{cross_reference}",
        agent=arts_runtime_expert,
    )

    # --- Guard: halt if unsafe ---
    g.guard(
        condition="The safety verdict is SAFE or SUSPICIOUS (not UNSAFE)",
        on_fail="halt",
        source=verified,
    )

    # --- Store results ---
    g.update_memory(
        data=verified,
        key="datablock_safety_audit",
        space="episodic",
    )

    g.print("=== DataBlock Safety Audit ===\n{verified}")
    g.done(verified)


if __name__ == "__main__":
    print(datablock_safety_auditor._graph.to_air())
