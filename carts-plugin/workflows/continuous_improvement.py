#!/usr/bin/env python3
"""WF15 — Continuous Improvement (META workflow).

Reads accumulated knowledge from ALL prior workflow executions, identifies
cross-cutting patterns, proposes improvements, negotiates priorities,
plans execution, verifies feasibility, and reflects on the improvement
process.

Topology:
  5x query_memory (parallel)  -->  merge  -->  reason(pattern analysis)
  -->  3x parallel improvement proposals  -->  negotiate priorities
  -->  plan  -->  verify feasibility  -->  reflect  -->  update_memory
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import arts_runtime_expert, ir_canonicalizer, pipeline_architect


@compile()
def continuous_improvement(g: GraphRecorder, improvement_focus: str):
    """Mine accumulated workflow knowledge for cross-cutting improvement opportunities."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- spawn improvement team ---
    architect = g.spawn("architect", profile=claude, cwd=cwd)
    runtime = g.spawn("runtime_expert", profile=claude, cwd=cwd)
    canonicalizer = g.spawn("canonicalizer", profile=claude, cwd=cwd)

    # --- 5 parallel memory queries across all workflow categories ---
    pipeline_knowledge = g.query_memory(
        query="All pipeline analysis results: stage ordering issues, parallelism findings, "
        "optimization opportunities discovered across workflow executions",
        key="pipeline_history",
    )
    debug_knowledge = g.query_memory(
        query="All debugging sessions: root causes, fix patterns, recurring failure modes, "
        "EDT lifecycle bugs, DB access conflicts",
        key="debug_history",
    )
    optimization_knowledge = g.query_memory(
        query="All optimization results: canonicalization patterns applied, performance gains, "
        "EDT fusion success rates, DB partitioning decisions",
        key="optimization_history",
    )
    safety_knowledge = g.query_memory(
        query="All safety verification results: concurrency bugs found, epoch deadlocks, "
        "race conditions, mode conflict detections",
        key="safety_history",
    )
    architecture_knowledge = g.query_memory(
        query="All architecture decisions: ADRs created, design negotiations, feature lifecycle "
        "outcomes, cross-cutting concerns identified",
        key="architecture_history",
    )

    # --- merge all knowledge ---
    all_knowledge = g.merge(
        "accumulated_knowledge",
        [
            pipeline_knowledge,
            debug_knowledge,
            optimization_knowledge,
            safety_knowledge,
            architecture_knowledge,
        ],
    )

    # --- reason about cross-cutting patterns ---
    pattern_analysis = g.reason(
        "Analyze accumulated knowledge from all prior CARTS workflow executions to identify "
        "cross-cutting improvement patterns.\n\n"
        "Improvement focus: {improvement_focus}\n\n"
        "Accumulated knowledge:\n{all_knowledge}\n\n"
        "Identify:\n"
        "1. Recurring bug patterns: which types of bugs appear repeatedly? What root causes "
        "are most common?\n"
        "2. Pipeline bottlenecks: which stages are involved in the most issues? Are there "
        "stage-ordering problems that keep resurfacing?\n"
        "3. Missing test coverage: which failure modes were not caught by existing tests?\n"
        "4. Optimization gaps: which canonicalization patterns are missing? Where is EDT "
        "fusion failing?\n"
        "5. Architecture debt: which design decisions have caused the most downstream problems?\n"
        "6. Process improvements: which debugging/analysis steps could be automated?\n"
        "7. Cross-domain interactions: EDT bugs triggered by DB mode + epoch timing, etc.\n\n"
        "Rank findings by frequency and impact.",
    )

    # --- 3 parallel improvement proposals ---
    pipeline_improvements = g.ask(
        "Propose pipeline-level improvements based on pattern analysis.\n\n"
        "Pattern analysis:\n{pattern_analysis}\n"
        "Focus: {improvement_focus}\n\n"
        "For each proposal:\n"
        "- Problem statement with frequency data from past sessions\n"
        "- Root cause in the pipeline architecture\n"
        "- Proposed fix: specific stages to modify, reorder, or add\n"
        "- Expected impact: which recurring bugs would this prevent?\n"
        "- Risk assessment: what could break?\n"
        "- Effort estimate: which files need changes?",
        agent=pipeline_architect,
    )

    runtime_improvements = g.ask(
        "Propose runtime-level improvements based on pattern analysis.\n\n"
        "Pattern analysis:\n{pattern_analysis}\n"
        "Focus: {improvement_focus}\n\n"
        "For each proposal:\n"
        "- Problem statement from recurring runtime bugs\n"
        "- Root cause in EDT/DB/epoch handling\n"
        "- Proposed fix: specific passes or runtime APIs to improve\n"
        "- Static checks that could catch these bugs at compile time\n"
        "- Runtime assertions that could detect violations early\n"
        "- Impact on ARTS runtime performance",
        agent=arts_runtime_expert,
    )

    ir_improvements = g.ask(
        "Propose IR canonicalization improvements based on pattern analysis.\n\n"
        "Pattern analysis:\n{pattern_analysis}\n"
        "Focus: {improvement_focus}\n\n"
        "For each proposal:\n"
        "- Missing canonicalization pattern with PatternBenefit score\n"
        "- Normal form violations observed in past debugging sessions\n"
        "- New DRR rewrite rules that would catch common anti-patterns\n"
        "- Existing patterns that need updated preconditions\n"
        "- Semantics preservation argument for each proposed pattern\n"
        "- Estimated reduction in IR size or optimization opportunities unlocked",
        agent=ir_canonicalizer,
    )

    # --- negotiate improvement priorities ---
    prioritized = g.negotiate(
        parties=["architect", "runtime_expert", "canonicalizer"],
        proposal=(
            "Negotiate a prioritized improvement plan from the three proposal sets.\n\n"
            "Pipeline proposals:\n{pipeline_improvements}\n\n"
            "Runtime proposals:\n{runtime_improvements}\n\n"
            "IR canonicalization proposals:\n{ir_improvements}\n\n"
            "Resolve conflicts:\n"
            "1. Pipeline changes that affect runtime behavior\n"
            "2. Canonicalization patterns that depend on pipeline stage ordering\n"
            "3. Resource contention: which improvements can be done in parallel?\n\n"
            "Produce a ranked list of improvements with:\n"
            "- Priority (P0/P1/P2)\n"
            "- Dependencies between improvements\n"
            "- Recommended execution order\n"
            "- Assigned domain (pipeline/runtime/IR)"
        ),
        max_rounds=3,
    )

    # --- plan execution ---
    execution_plan = g.plan(
        goal=(
            "Create an execution plan for the prioritized improvements.\n\n"
            "Prioritized improvements:\n{prioritized}\n\n"
            "For each P0 and P1 item:\n"
            "1. Specific files to modify with expected changes\n"
            "2. Tests to add (FileCheck lit tests)\n"
            "3. Documentation updates (ADR if architectural)\n"
            "4. Ordering constraints: which improvements must land first\n"
            "5. Rollback plan if improvement causes regressions\n"
            "6. Verification criteria: how to confirm the improvement worked"
        ),
    )

    # --- verify feasibility ---
    feasibility = g.verify(
        claim="The improvement plan is feasible and addresses the highest-impact patterns",
        evidence=(
            "Pattern analysis:\n{pattern_analysis}\n\n"
            "Prioritized improvements:\n{prioritized}\n\n"
            "Execution plan:\n{execution_plan}\n\n"
            "Verify:\n"
            "1. P0 items address the most frequently recurring bug patterns\n"
            "2. No circular dependencies in the execution order\n"
            "3. Each improvement has a clear verification criterion\n"
            "4. Rollback plans exist for high-risk changes\n"
            "5. Test plan covers the failure modes each improvement targets\n"
            "6. The plan is achievable without breaking the existing 18-stage pipeline"
        ),
    )

    # --- reflect on the improvement process ---
    reflection = g.reflect(trace_id="continuous_improvement")

    # --- persist everything ---
    g.update_memory(
        data=feasibility,
        key="continuous_improvement",
        space="episodic",
    )

    g.print(
        "=== Continuous Improvement Report ===\n\n"
        "Focus: {improvement_focus}\n\n"
        "Pattern analysis:\n{pattern_analysis}\n\n"
        "Prioritized improvements:\n{prioritized}\n\n"
        "Execution plan:\n{execution_plan}\n\n"
        "Feasibility:\n{feasibility}\n\n"
        "Reflection:\n{reflection}"
    )
    g.done(feasibility)


if __name__ == "__main__":
    print(continuous_improvement._graph.to_air())
