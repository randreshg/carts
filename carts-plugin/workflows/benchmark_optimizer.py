#!/usr/bin/env python3
"""WF7 -- Benchmark-Driven Optimization Loop: memory-augmented performance tuning.

Recalls previous optimization attempts from episodic memory, analyzes baseline
benchmarks, plans an optimization strategy, implements it, re-benchmarks in a
sandbox, and reflects on what worked. Each cycle builds on accumulated knowledge.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude, codex, gemini

from .experts import codegen_specialist, performance_oracle, pipeline_architect


@compile()
def benchmark_optimizer(
    g: GraphRecorder,
    benchmark_suite: str,
    baseline_results: str,
):
    """Iteratively optimize CARTS compiler output guided by benchmark results and memory."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn agents ---
    oracle_spawn = g.spawn("oracle", profile=gemini, cwd=cwd)
    architect_spawn = g.spawn("architect", profile=claude, cwd=cwd)
    codegen_spawn = g.spawn("codegen", profile=codex, cwd=cwd)

    # --- Phase 1: Recall previous optimization attempts ---
    history = g.query_memory(
        query=(
            "Previous optimization attempts for CARTS benchmarks. "
            "What optimizations were tried? What impact did they have? "
            "What failed and why?"
        ),
        key="benchmark_optimization_history",
    )

    # --- Phase 2: Analyze baseline with Gemini ---
    baseline_analysis = g.ask(
        "Analyze these CARTS benchmark results and identify optimization opportunities.\n\n"
        "Benchmark suite: {benchmark_suite}\n"
        "Baseline results:\n{baseline_results}\n\n"
        "Previous optimization history:\n{history}\n\n"
        "Perform:\n"
        "1. Identify the critical path for each benchmark (longest EDT chain)\n"
        "2. Measure parallelism utilization: actual concurrent EDTs vs available cores\n"
        "3. Find the top-5 hottest EDTs by execution time across all benchmarks\n"
        "4. Analyze EDT granularity: too fine (scheduling overhead) or too coarse (lost parallelism)\n"
        "5. Check DB access patterns: unnecessary RW modes, avoidable partitioning\n"
        "6. Compare against gcc -fopenmp baseline where available\n"
        "7. Flag any regressions from previous optimization attempts\n\n"
        "Rank bottlenecks by estimated impact on total runtime. Be quantitative.",
        agent=performance_oracle,
    )

    # --- Phase 3: Plan optimization strategy ---
    opt_plan = g.plan(
        goal=(
            "Design an optimization strategy for the CARTS compiler targeting "
            "benchmark suite '{benchmark_suite}'.\n\n"
            "Baseline analysis:\n{baseline_analysis}\n\n"
            "Previous attempts:\n{history}\n\n"
            "Requirements:\n"
            "- Focus on the highest-impact bottleneck identified in the analysis\n"
            "- Avoid repeating optimizations that previously failed\n"
            "- Propose a specific MLIR pass modification or new pass\n"
            "- Estimate expected speedup with justification\n"
            "- Identify risks and correctness concerns\n"
            "- Plan should be implementable in a single focused change"
        ),
        agent=pipeline_architect,
    )

    # --- Phase 4: Architect designs the optimization ---
    opt_design = g.ask(
        "Design the compiler optimization to implement.\n\n"
        "Optimization plan:\n{opt_plan}\n\n"
        "Baseline analysis:\n{baseline_analysis}\n\n"
        "Specify:\n"
        "- Which pipeline stage(s) to modify\n"
        "- What transformation to apply (rewrite pattern, analysis change, scheduling hint)\n"
        "- Input IR pattern to match\n"
        "- Output IR pattern to produce\n"
        "- Correctness argument: why the transformation preserves semantics\n"
        "- Which benchmarks should benefit most and by how much",
        agent=pipeline_architect,
    )

    # --- Phase 5: Checkpoint before implementation ---
    cp = g.checkpoint("pre_implementation")
    opt_design >> cp

    # --- Phase 6: Codex implements the optimization ---
    implementation = g.ask(
        "Implement this CARTS compiler optimization.\n\n"
        "Design spec:\n{opt_design}\n\n"
        "Requirements:\n"
        "- Modify or create pass files in lib/arts/passes/\n"
        "- Register in Passes.td if a new pass\n"
        "- Update pipeline in tools/compile/Compile.cpp if needed\n"
        "- LLVM style: 2-space indent, CamelCase types, camelCase variables\n"
        "- Use AttrNames constants, never hardcoded strings\n"
        "- Use AM->getDbAnalysis() and AM->getEdtAnalysis() for graph access\n"
        "- Thread-safe: no global/static mutable state\n\n"
        "Produce complete, compilable C++ code.",
        agent=codegen_specialist,
    )
    cp >> implementation

    # --- Phase 7: Re-benchmark in sandbox ---
    benchmark_run = g.execute(
        code=(
            "cd $CARTS_HOME && "
            "carts build && "
            f"carts benchmark {benchmark_suite} --json --output benchmark_results.json && "
            "cat benchmark_results.json"
        ),
    )
    implementation >> benchmark_run

    # --- Phase 8: Reflect on results ---
    reflection = g.reflect(
        trace_id="benchmark_optimizer",
    )
    benchmark_run >> reflection

    analysis = g.think(
        "Analyze the optimization results for benchmark suite '{benchmark_suite}'.\n\n"
        "Optimization plan:\n{opt_plan}\n\n"
        "Design:\n{opt_design}\n\n"
        "New benchmark results:\n{benchmark_run}\n\n"
        "Original baseline:\n{baseline_results}\n\n"
        "Reflection:\n{reflection}\n\n"
        "Determine:\n"
        "1. Did the optimization achieve the expected speedup? By how much?\n"
        "2. Were there any regressions in other benchmarks?\n"
        "3. What was the actual vs predicted impact?\n"
        "4. What lessons does this teach for future optimization attempts?\n"
        "5. Should this optimization be kept, reverted, or refined?",
        agent=performance_oracle,
    )

    # --- Phase 9: Persist results to memory ---
    g.update_memory(
        data=analysis,
        key="benchmark_optimization_history",
        space="episodic",
    )

    g.print(
        "=== BENCHMARK OPTIMIZATION COMPLETE ===\n\n"
        "Suite: {benchmark_suite}\n\n"
        "Plan:\n{opt_plan}\n\n"
        "Results:\n{analysis}"
    )
    g.done(analysis)


if __name__ == "__main__":
    print(benchmark_optimizer._graph.to_air())
