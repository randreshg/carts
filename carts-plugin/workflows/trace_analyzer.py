#!/usr/bin/env python3
"""WF4 -- Large ARTS Trace Analyzer.

Leverages Gemini's 1M+ context window to ingest an entire ARTS scheduling
trace (100K+ events), then fans out to three specialist agents who analyze
the trace from different angles:
  - Performance Oracle (Gemini): throughput, critical path, hotspots
  - Concurrency Analyst (Claude): race conditions, scheduling hazards
  - ARTS Runtime Expert (Claude): correctness, lifecycle violations

Results are synthesized via a reasoning step, verified, and stored.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude, gemini

from .experts import arts_runtime_expert, concurrency_analyst, performance_oracle


@compile()
def trace_analyzer(g: GraphRecorder, trace_path: str, performance_target: str):
    """Analyze a large ARTS scheduling trace from multiple expert angles."""

    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn three specialist agents ---
    perf_agent = g.spawn("perf_oracle", profile=gemini, cwd=cwd)
    concurrency_agent = g.spawn("concurrency_analyst", profile=claude, cwd=cwd)
    runtime_agent = g.spawn("runtime_expert", profile=claude, cwd=cwd)

    # --- Gemini ingests the full trace (1M+ context) ---
    trace_ingest = g.ask(
        "Load the ARTS scheduling trace at {trace_path} in its entirety. "
        "This trace contains EDT create/schedule/execute/complete events with "
        "timestamps, DB acquire/release events, and epoch signal/wait events.\n\n"
        "Produce a structured summary including:\n"
        "  - Total event count and time span\n"
        "  - EDT count by type (compute, continuation, barrier)\n"
        "  - DB access event count by mode (RO, RW, REDUCE, COMMUTE)\n"
        "  - Epoch count and average scope width\n"
        "  - Top-10 longest EDT executions with timestamps\n"
        "  - Any anomalous patterns (gaps, bursts, ordering violations)\n\n"
        "Preserve all raw data needed for downstream specialist analysis.",
        agent=performance_oracle,
    )

    # --- Fan-out to three specialist analyses (all depend on trace_ingest) ---
    perf_analysis = g.ask(
        "Perform a deep performance analysis of the ARTS trace:\n\n"
        "Trace summary:\n{trace_ingest}\n\n"
        "Performance target: {performance_target}\n\n"
        "Analyze:\n"
        "  1. Critical path: identify the longest chain of dependent EDTs and "
        "its total execution time\n"
        "  2. Parallelism utilization: ratio of actual concurrent EDTs to "
        "available cores over time\n"
        "  3. Top-5 hottest EDTs: execution time, DB access overhead, "
        "scheduling latency breakdown\n"
        "  4. EDT granularity analysis: are tasks too fine (scheduling overhead > "
        "compute) or too coarse (lost parallelism)?\n"
        "  5. DB access overhead: acquire/release latency distribution, mode "
        "upgrade costs, partition effects\n"
        "  6. Gap analysis vs performance target: what needs to change to hit "
        "{performance_target}?\n\n"
        "Provide quantitative metrics with percentages and absolute times.",
        agent=performance_oracle,
    )

    concurrency_analysis = g.ask(
        "Analyze the ARTS trace for concurrency hazards:\n\n"
        "Trace summary:\n{trace_ingest}\n\n"
        "Build the happens-before graph from EDT dependencies and check:\n"
        "  1. Race conditions: concurrent EDT pairs accessing the same DB with "
        "incompatible modes (RO||RW or RW||RW)\n"
        "  2. Scheduling hazards: EDTs scheduled before their dependencies complete\n"
        "  3. Epoch violations: cross-epoch DB sharing without explicit migration\n"
        "  4. CPS chain ordering: continuation EDTs seeing incomplete writes\n"
        "  5. Deadlock potential: circular dependencies in EDT scheduling or "
        "nested epoch waits\n\n"
        "For each hazard found, provide:\n"
        "  - EDT IDs and timestamps involved\n"
        "  - DB IDs and access modes in conflict\n"
        "  - Severity (critical/warning/info)\n"
        "  - Suggested fix",
        agent=concurrency_analyst,
    )

    correctness_analysis = g.ask(
        "Analyze the ARTS trace for runtime correctness violations:\n\n"
        "Trace summary:\n{trace_ingest}\n\n"
        "Check EDT lifecycle and DB protocol compliance:\n"
        "  1. EDT lifecycle: every EDT follows create -> schedule -> execute -> "
        "complete -> destroy (no skipped states)\n"
        "  2. DB protocol: every acquire has a matching release, no double-acquire "
        "without release, no release without acquire\n"
        "  3. Epoch protocol: every epoch_create has a matching epoch_wait, "
        "signal count matches expected EDT count\n"
        "  4. Memory safety: no DB access after release, no EDT execution after "
        "destroy\n"
        "  5. Distribution correctness: data migration completes before remote "
        "EDT accesses migrated DB\n\n"
        "For each violation, provide the event sequence that demonstrates it.",
        agent=arts_runtime_expert,
    )

    # --- Reason: synthesize all three analyses ---
    synthesis = g.reason(
        "Synthesize the following three specialist analyses of the same ARTS trace "
        "into a unified assessment:\n\n"
        "=== Performance Analysis ===\n{perf_analysis}\n\n"
        "=== Concurrency Analysis ===\n{concurrency_analysis}\n\n"
        "=== Correctness Analysis ===\n{correctness_analysis}\n\n"
        "Produce a ranked list of findings by impact:\n"
        "  1. CRITICAL: correctness violations that produce wrong results\n"
        "  2. HIGH: concurrency hazards that may cause non-determinism\n"
        "  3. MEDIUM: performance bottlenecks blocking the target of "
        "{performance_target}\n"
        "  4. LOW: optimization opportunities\n\n"
        "For each finding, cross-reference evidence from multiple analyses. "
        "If the performance and concurrency analyses agree on a hotspot, that "
        "increases confidence. If they disagree, flag the discrepancy.",
        agent=arts_runtime_expert,
    )

    # --- Verify the synthesis ---
    verified = g.verify(
        claim=(
            "The synthesized trace analysis is consistent: all referenced EDT "
            "and DB IDs exist in the original trace, severity rankings are "
            "justified by evidence, and no critical correctness violation is "
            "classified below HIGH."
        ),
        evidence="{synthesis}",
        agent=concurrency_analyst,
    )

    # --- Store results ---
    g.update_memory(
        data=verified,
        key="trace_analysis",
        space="episodic",
    )

    g.print("=== Trace Analysis ===\n{verified}")
    g.done(verified)


if __name__ == "__main__":
    print(trace_analyzer._graph.to_air())
