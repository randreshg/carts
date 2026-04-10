#!/usr/bin/env python3
"""WF13 — Master Debugger (META workflow).

Orchestrates comprehensive debugging by recalling prior debug sessions from
memory, triaging the bug, delegating sub-analyses to specialized
sub-workflows (EDT, DataBlock, Epoch), and synthesizing a comprehensive
diagnostic report.

Topology:
  5x query_memory  -->  merge  -->  triage  -->  3x delegate(EDT/DB/Epoch)
  -->  wait_all  -->  merge  -->  reason(report)  -->  reflect
  -->  pause(checkpoint)  -->  update_memory
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import arts_runtime_expert, concurrency_analyst, debugger_detective


@compile()
def master_debugger(g: GraphRecorder, bug_report: str, affected_files: str):
    """Meta-debugging workflow that recalls prior sessions, triages, and delegates to sub-analyses."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- spawn orchestrator ---
    orchestrator = g.spawn("orchestrator", profile=claude, cwd=cwd)

    # --- recall prior debugging knowledge (5 parallel memory queries) ---
    prior_edt = g.query_memory(
        query="Prior EDT lifecycle debugging sessions, root causes, and fixes",
        key="edt_debug_history",
    )
    prior_db = g.query_memory(
        query="Prior DataBlock access mode bugs, RO/RW/REDUCE/COMMUTE conflicts found",
        key="db_debug_history",
    )
    prior_epoch = g.query_memory(
        query="Prior epoch synchronization bugs, deadlocks, and barrier issues",
        key="epoch_debug_history",
    )
    prior_regressions = g.query_memory(
        query="Prior regression hunts, root causes, and patterns",
        key="regression_history",
    )
    prior_concurrency = g.query_memory(
        query="Prior concurrency bugs, race conditions, and happens-before violations",
        key="concurrency_history",
    )

    # --- merge prior knowledge ---
    prior_knowledge = g.merge(
        "prior_knowledge",
        [prior_edt, prior_db, prior_epoch, prior_regressions, prior_concurrency],
    )

    # --- triage the bug ---
    triage = g.ask(
        "Triage this bug report using accumulated debugging knowledge from prior sessions.\n\n"
        "Bug report:\n{bug_report}\n\n"
        "Affected files:\n{affected_files}\n\n"
        "Prior debugging knowledge:\n{prior_knowledge}\n\n"
        "Perform triage:\n"
        "1. Does this match any known bug patterns from prior sessions?\n"
        "2. Classify the primary failure domain: EDT lifecycle, DataBlock access, or Epoch sync\n"
        "3. Estimate severity: crash, miscompile, performance regression, or cosmetic\n"
        "4. Identify which pipeline stages are likely involved based on affected files\n"
        "5. Determine if this is a variant of a previously-fixed bug (regression?)\n"
        "6. Assign priority and recommended investigation order for sub-analyses",
        agent=debugger_detective,
    )

    # --- delegate three sub-analyses ---
    edt_analysis = g.delegate(
        task_spec=(
            "Investigate EDT lifecycle aspects of this bug.\n\n"
            "Bug report:\n{bug_report}\n"
            "Affected files:\n{affected_files}\n"
            "Triage:\n{triage}\n\n"
            "Focus on:\n"
            "- EDT create/schedule/execute/complete/destroy state machine\n"
            "- CPS chain integrity and continuation wiring\n"
            "- EDT scheduling order and dependency violations\n"
            "- EDT fusion correctness in edt-transforms/edt-opt stages"
        ),
        target_agent="edt_sub_analyst",
    )

    db_analysis = g.delegate(
        task_spec=(
            "Investigate DataBlock access aspects of this bug.\n\n"
            "Bug report:\n{bug_report}\n"
            "Affected files:\n{affected_files}\n"
            "Triage:\n{triage}\n\n"
            "Focus on:\n"
            "- DB access mode correctness: RO, RW, REDUCE, COMMUTE\n"
            "- Mode constant usage: ARTS_DB_PIN=2, ARTS_DB_ONCE=3\n"
            "- create-dbs and db-opt stage behavior\n"
            "- db-partitioning H1 heuristic decisions\n"
            "- Concurrent DB access conflicts between EDTs"
        ),
        target_agent="db_sub_analyst",
    )

    epoch_analysis = g.delegate(
        task_spec=(
            "Investigate epoch synchronization aspects of this bug.\n\n"
            "Bug report:\n{bug_report}\n"
            "Affected files:\n{affected_files}\n"
            "Triage:\n{triage}\n\n"
            "Focus on:\n"
            "- epoch_create/epoch_signal/epoch_wait balance\n"
            "- Nested epoch deadlock potential\n"
            "- Cross-epoch DB sharing without explicit migration\n"
            "- Epoch scoping: too wide (unnecessary serialization) or too narrow (missed sync)\n"
            "- Epoch barrier insertion correctness in the epochs stage"
        ),
        target_agent="epoch_sub_analyst",
    )

    # --- wait for all sub-analyses ---
    all_done = g.wait_all("sub_analyses_complete", [edt_analysis, db_analysis, epoch_analysis])

    # --- merge sub-analysis results ---
    merged_analyses = g.merge("merged_analyses", [edt_analysis, db_analysis, epoch_analysis])

    # --- reason a comprehensive diagnostic report ---
    report = g.reason(
        "Synthesize a comprehensive diagnostic report from all sub-analyses.\n\n"
        "Original triage:\n{triage}\n\n"
        "EDT analysis:\n{edt_analysis}\n"
        "DataBlock analysis:\n{db_analysis}\n"
        "Epoch analysis:\n{epoch_analysis}\n\n"
        "Prior knowledge context:\n{prior_knowledge}\n\n"
        "Produce a structured report covering:\n"
        "1. Executive summary: one-paragraph root cause\n"
        "2. Detailed findings from each sub-analysis\n"
        "3. Cross-cutting interactions (e.g., EDT bug triggered by DB mode + epoch timing)\n"
        "4. Comparison with prior similar bugs from memory\n"
        "5. Recommended fix with specific file locations and code changes\n"
        "6. Test plan: what tests to add/modify to prevent recurrence\n"
        "7. Risk assessment: what other code paths might have the same vulnerability",
    )

    # --- reflect on the debugging process ---
    reflection = g.reflect(trace_id="master_debugger")

    # --- pause for human review ---
    g.pause(
        message=(
            "Master debugging analysis complete. Review the comprehensive report "
            "and reflection before applying changes.\n\n"
            "Report:\n{report}\n\n"
            "Process reflection:\n{reflection}"
        ),
        checkpoint_id="master_debug_review",
    )

    # --- persist findings ---
    g.update_memory(
        data=report,
        key="master_debug_session",
        space="episodic",
    )

    g.print(
        "=== Master Debug Report ===\n\n"
        "{report}\n\n"
        "=== Process Reflection ===\n\n"
        "{reflection}"
    )
    g.done(report)


if __name__ == "__main__":
    print(master_debugger._graph.to_air())
