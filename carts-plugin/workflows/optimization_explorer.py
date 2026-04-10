#!/usr/bin/env python3
"""WF5 -- Optimization Explorer: competitive optimization pass search.

Four specialized optimizer agents each propose a different optimization strategy
from their respective philosophy (loop, data layout, EDT fusion, epoch
elimination). After parallel proposals, a cross-evaluation synthesizes insights
and a negotiate round forces consensus on the highest-impact optimization.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import (
    arts_runtime_expert,
    concurrency_analyst,
    ir_canonicalizer,
    performance_oracle,
)


@compile()
def optimization_explorer(
    g: GraphRecorder,
    optimization_goal: str,
    ir_sample: str,
):
    """Explore optimization strategies via competitive multi-agent proposals and negotiation."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Phase 1: Decompose the optimization goal ---
    goal_analysis = g.plan(
        goal=(
            "Analyze this optimization goal for the CARTS compiler and identify "
            "the key optimization dimensions to explore.\n\n"
            "Goal: {optimization_goal}\n\n"
            "IR sample:\n{ir_sample}\n\n"
            "Identify:\n"
            "1. What is the performance bottleneck? (scheduling, memory, synchronization, granularity)\n"
            "2. Which pipeline stages are most relevant?\n"
            "3. What are the competing optimization strategies?\n"
            "4. What constraints must any optimization preserve? (correctness, semantics, ordering)"
        ),
        agent=performance_oracle,
    )

    # --- Phase 2: Four parallel optimization proposals ---
    team = g.team("optimizers")
    loop_opt = team.add("loop_optimizer", profile=claude, cwd=cwd)
    layout_opt = team.add("data_layout_optimizer", profile=claude, cwd=cwd)
    fusion_opt = team.add("edt_fusion_optimizer", profile=claude, cwd=cwd)
    epoch_opt = team.add("epoch_eliminator", profile=claude, cwd=cwd)

    # Each agent proposes from its optimization philosophy
    loop_proposal = g.ask(
        "Propose a LOOP OPTIMIZATION strategy for this goal.\n\n"
        "Goal analysis: {goal_analysis}\n"
        "IR sample:\n{ir_sample}\n\n"
        "Focus on:\n"
        "- Loop reordering and interchange for better data locality\n"
        "- Loop tiling to match EDT granularity to cache hierarchy\n"
        "- Loop fusion to reduce EDT creation overhead\n"
        "- Loop distribution to expose independent iterations as parallel EDTs\n"
        "- Polyhedral transformations if applicable\n\n"
        "Provide: (1) the proposed transformation, (2) expected impact on EDT count "
        "and scheduling overhead, (3) correctness argument, (4) implementation sketch "
        "as MLIR rewrite patterns.",
        agent=performance_oracle,
    )

    layout_proposal = g.ask(
        "Propose a DATA LAYOUT OPTIMIZATION strategy for this goal.\n\n"
        "Goal analysis: {goal_analysis}\n"
        "IR sample:\n{ir_sample}\n\n"
        "Focus on:\n"
        "- DataBlock partitioning strategy (H1 heuristic tuning)\n"
        "- DB access mode relaxation: can any RW be downgraded to RO or COMMUTE?\n"
        "- DB coalescing: merge small DBs into fewer, larger allocations\n"
        "- Memory layout transformations: AoS to SoA for vectorization\n"
        "- Reducing cross-node data migration by locality-aware DB placement\n\n"
        "Provide: (1) the proposed transformation, (2) expected impact on memory traffic "
        "and DB acquire/release overhead, (3) correctness argument, (4) implementation sketch.",
        agent=arts_runtime_expert,
    )

    fusion_proposal = g.ask(
        "Propose an EDT FUSION OPTIMIZATION strategy for this goal.\n\n"
        "Goal analysis: {goal_analysis}\n"
        "IR sample:\n{ir_sample}\n\n"
        "Focus on:\n"
        "- EDT fusion: merge adjacent EDTs with compatible DB access modes\n"
        "- CPS chain shortening: eliminate intermediate continuation EDTs\n"
        "- EDT inlining: fold trivial EDTs into their parents\n"
        "- Granularity coarsening: merge fine-grained EDTs to reduce scheduling overhead\n"
        "- Scheduling hint insertion for locality-aware execution\n\n"
        "Provide: (1) the proposed transformation, (2) expected impact on EDT count "
        "and scheduling overhead, (3) correctness argument (no new races), (4) implementation sketch.",
        agent=ir_canonicalizer,
    )

    epoch_proposal = g.ask(
        "Propose an EPOCH ELIMINATION/OPTIMIZATION strategy for this goal.\n\n"
        "Goal analysis: {goal_analysis}\n"
        "IR sample:\n{ir_sample}\n\n"
        "Focus on:\n"
        "- Dead epoch elimination: remove epochs that synchronize nothing\n"
        "- Epoch narrowing: reduce scope to only the EDTs that truly need synchronization\n"
        "- Epoch merging: combine redundant barriers\n"
        "- Epoch hoisting/sinking: move barriers to reduce critical path length\n"
        "- Replacing epochs with fine-grained EDT dependencies where possible\n\n"
        "Provide: (1) the proposed transformation, (2) expected impact on synchronization "
        "overhead and critical path, (3) correctness argument (no lost synchronization), "
        "(4) implementation sketch.",
        agent=concurrency_analyst,
    )

    # --- Phase 3: Synchronize all proposals ---
    all_proposals = g.wait_all(
        "all_proposals",
        [loop_proposal, layout_proposal, fusion_proposal, epoch_proposal],
    )

    # --- Phase 4: Cross-evaluation ---
    cross_eval = g.think(
        "Cross-evaluate all four optimization proposals for the CARTS compiler.\n\n"
        "Goal: {optimization_goal}\n\n"
        "Loop optimization:\n{loop_proposal}\n\n"
        "Data layout optimization:\n{layout_proposal}\n\n"
        "EDT fusion optimization:\n{fusion_proposal}\n\n"
        "Epoch elimination:\n{epoch_proposal}\n\n"
        "Analyze:\n"
        "1. Which proposals are complementary vs. conflicting?\n"
        "2. Rank by estimated impact on total runtime (quantify if possible)\n"
        "3. Rank by implementation complexity and risk\n"
        "4. Can any proposals be composed? In what order?\n"
        "5. Which proposal has the best effort-to-impact ratio?\n\n"
        "Produce a ranked recommendation with justification.",
        agent=performance_oracle,
    )

    # --- Phase 5: Negotiate to select the winner ---
    consensus = g.negotiate(
        parties=["loop_optimizer", "data_layout_optimizer", "edt_fusion_optimizer", "epoch_eliminator"],
        proposal=(
            "Based on the cross-evaluation, select the single highest-impact "
            "optimization to implement first for goal: {optimization_goal}\n\n"
            "Cross-evaluation:\n{cross_eval}\n\n"
            "Each party must argue for their approach, acknowledge trade-offs, "
            "and ultimately agree on ONE winner. The selected optimization must "
            "have a clear correctness argument and a feasible implementation plan.\n\n"
            "If two optimizations compose well, you may propose a combined approach "
            "but must justify the added complexity."
        ),
        max_rounds=3,
    )

    # --- Phase 6: Persist the decision ---
    g.update_memory(
        data=consensus,
        key="optimization_decision",
        space="episodic",
    )

    g.print(
        "=== OPTIMIZATION EXPLORATION COMPLETE ===\n\n"
        "Goal: {optimization_goal}\n\n"
        "Cross-evaluation:\n{cross_eval}\n\n"
        "Consensus decision:\n{consensus}"
    )
    g.done(consensus)


if __name__ == "__main__":
    print(optimization_explorer._graph.to_air())
