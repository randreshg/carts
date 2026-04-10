#!/usr/bin/env python3
"""WF10 -- CPS Chain Pattern Analyzer.

Analyzes Continuation-Passing Style (CPS) chain patterns in CARTS-compiled
code. CPS chains are the fundamental EDT sequencing mechanism in ARTS:
each EDT's continuation slot points to the next EDT to execute.

Uses conditional branching to route simple chains (depth < threshold) to
a lightweight analysis path and complex chains (depth >= threshold) to a
deep analysis path with full dependency tracking.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import arts_runtime_expert, concurrency_analyst


@compile()
def cps_chain_analyzer(g: GraphRecorder, source_file: str, chain_depth_threshold: int):
    """Analyze CPS chain patterns with conditional routing by complexity."""

    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn two specialist agents ---
    runtime_agent = g.spawn("runtime_expert", profile=claude, cwd=cwd)
    concurrency_agent = g.spawn("concurrency_expert", profile=claude, cwd=cwd)

    # --- Step 1: Extract all CPS chains from source ---
    chain_extraction = g.ask(
        "Extract all CPS (Continuation-Passing Style) chains from {source_file}.\n\n"
        "A CPS chain is a sequence of EDTs linked by continuation slots: "
        "EDT_A's continuation points to EDT_B, which points to EDT_C, etc.\n\n"
        "For each chain found:\n"
        "  1. Chain ID (auto-generated sequential)\n"
        "  2. EDT sequence: [EDT_1 -> EDT_2 -> ... -> EDT_N]\n"
        "  3. Chain depth (number of EDTs in the chain)\n"
        "  4. DataBlocks accessed by each EDT in the chain (with modes)\n"
        "  5. Whether the chain crosses an epoch boundary\n"
        "  6. Source line range\n\n"
        "Return a JSON array of chain descriptors.",
        agent=arts_runtime_expert,
    )

    # --- Step 2: Classify each chain by complexity ---
    classify = g.think(
        "Classify each CPS chain from the extraction by complexity.\n\n"
        "Chain data:\n{chain_extraction}\n\n"
        "Depth threshold: {chain_depth_threshold}\n\n"
        "For each chain, assign:\n"
        "  - complexity: 'simple' if depth < {chain_depth_threshold}, "
        "'complex' if depth >= {chain_depth_threshold}\n"
        "  - risk_factors: list any of these that apply:\n"
        "    * cross_epoch: chain crosses an epoch boundary\n"
        "    * shared_db: multiple EDTs in chain access the same DB\n"
        "    * mode_escalation: DB mode changes along the chain (RO -> RW)\n"
        "    * fan_out: an EDT in the chain spawns additional EDTs outside the chain\n\n"
        "Produce two lists: simple_chains and complex_chains, each with chain IDs "
        "and risk factors.",
        agent=arts_runtime_expert,
    )

    # --- Step 3: Branch on complexity ---
    branch = g.branch(
        condition_node=classify,
        true_label="simple",
        false_label="complex",
    )

    # --- Simple path: lightweight structural analysis ---
    simple_analysis = g.ask(
        "Perform lightweight analysis on the simple CPS chains "
        "(depth < {chain_depth_threshold}).\n\n"
        "Classification:\n{classify}\n\n"
        "For simple chains, check:\n"
        "  1. Correct continuation wiring: each EDT's continuation slot points "
        "to the next EDT in the chain (no dangling or circular references)\n"
        "  2. DB mode consistency: if EDT_A acquires DB_X as RW, and EDT_B "
        "(continuation) acquires the same DB_X, it sees the completed write\n"
        "  3. No unnecessary chains: flag single-EDT 'chains' that could be "
        "direct function calls instead of CPS\n\n"
        "Return a pass/fail per chain with brief justification.",
        agent=arts_runtime_expert,
    )

    # --- Complex path: deep dependency and deadlock analysis ---
    complex_analysis = g.ask(
        "Perform deep analysis on the complex CPS chains "
        "(depth >= {chain_depth_threshold}).\n\n"
        "Classification:\n{classify}\n\n"
        "Full chain data:\n{chain_extraction}\n\n"
        "For complex chains, analyze:\n"
        "  1. Dependency tracking: build the full data-flow graph through the "
        "chain, tracking which DB values flow from EDT to EDT\n"
        "  2. Epoch interaction: if the chain crosses an epoch boundary, verify "
        "that epoch_signal occurs before the cross-boundary continuation fires\n"
        "  3. Deadlock detection: check for circular dependencies where EDT_A "
        "waits on EDT_B's completion but EDT_B's continuation depends on EDT_A\n"
        "  4. Fan-out safety: if mid-chain EDTs spawn additional work, verify "
        "the spawned EDTs don't create ordering violations with downstream "
        "chain members\n"
        "  5. Optimization opportunities: identify chains that could be fused "
        "(adjacent EDTs with compatible DB access and no external dependencies)\n\n"
        "For each finding, provide the chain ID, EDT IDs involved, and evidence.",
        agent=concurrency_analyst,
    )

    # --- Merge both paths ---
    merged = g.merge("merge_analyses", [simple_analysis, complex_analysis])

    # --- Verify combined results ---
    verified = g.verify(
        claim=(
            "All CPS chains have been analyzed: every chain from the extraction "
            "appears in either the simple or complex analysis results, no chain "
            "is analyzed twice, and all reported deadlock risks involve provably "
            "circular dependencies."
        ),
        evidence="{merged}",
        agent=arts_runtime_expert,
    )

    # --- Store results ---
    g.update_memory(
        data=verified,
        key="cps_chain_analysis",
        space="episodic",
    )

    g.print("=== CPS Chain Analysis ===\n{verified}")
    g.done(verified)


if __name__ == "__main__":
    print(cps_chain_analyzer._graph.to_air())
