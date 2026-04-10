#!/usr/bin/env python3
"""WF1 -- Pipeline Topology Explorer.

Spawns three pipeline_architect perspectives in parallel to analyze the
CARTS 18-stage compiler pipeline from three complementary angles:
  1. Stage inventory (what exists, inputs/outputs per stage)
  2. Dependency graph (ordering constraints, data-flow edges)
  3. Parallelism map (which stages can run concurrently)

The three analyses merge, a verify op cross-checks the combined topology,
and the result is stored in shared memory for downstream workflows.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import pipeline_architect


@compile()
def pipeline_explorer(g: GraphRecorder, pipeline_root: str):
    """Explore and map the full CARTS pipeline topology."""

    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn three architect instances for parallel analysis ---
    arch1 = g.spawn("arch_inventory", profile=claude, cwd=cwd)
    arch2 = g.spawn("arch_dependencies", profile=claude, cwd=cwd)
    arch3 = g.spawn("arch_parallelism", profile=claude, cwd=cwd)

    # --- Three parallel analysis branches ---
    inventory = g.ask(
        "Enumerate every stage in the CARTS 18-stage compiler pipeline rooted at "
        "{pipeline_root}. For each stage produce:\n"
        "  - Stage name (canonical, from `carts pipeline --json`)\n"
        "  - Input dialect(s) consumed\n"
        "  - Output dialect(s) produced\n"
        "  - Key IR transformations applied\n"
        "  - Source file location (lib/arts/passes/...)\n\n"
        "Return a structured JSON array with one object per stage.",
        agent=pipeline_architect,
    )

    dependencies = g.think(
        "Analyze the stage ordering constraints in the CARTS pipeline at "
        "{pipeline_root}. Build a directed dependency graph where each edge "
        "represents a hard ordering requirement between stages.\n\n"
        "For each edge, explain WHY reordering would cause miscompilation. "
        "Specifically identify:\n"
        "  - Data dependencies (stage A produces IR that stage B consumes)\n"
        "  - Semantic dependencies (stage A establishes invariants stage B relies on)\n"
        "  - Analysis dependencies (stage A populates analysis results stage B queries)\n\n"
        "Output the graph as a JSON adjacency list with edge annotations.",
        agent=pipeline_architect,
    )

    parallelism = g.reason(
        "Given the CARTS pipeline at {pipeline_root}, determine which stages "
        "can safely execute concurrently without violating correctness.\n\n"
        "For each candidate parallel pair:\n"
        "  1. Verify they have no data dependency (neither consumes the other's output)\n"
        "  2. Verify they have no shared mutable analysis state\n"
        "  3. Verify they operate on disjoint IR regions or use read-only access\n"
        "  4. Estimate speedup from parallelizing this pair\n\n"
        "Produce a parallelism map: list of stage-sets that can run concurrently, "
        "with confidence levels (high/medium/low) and justifications.",
        agent=pipeline_architect,
    )

    # --- Merge the three perspectives ---
    topology = g.merge("merge_topology", [inventory, dependencies, parallelism])

    # --- Verify the combined topology for internal consistency ---
    verified = g.verify(
        claim=(
            "The merged pipeline topology is internally consistent: "
            "every stage referenced in the dependency graph appears in the inventory, "
            "every parallel pair has no path in the dependency graph, and "
            "the total stage count matches the canonical 18-stage pipeline."
        ),
        evidence="{topology}",
        agent=pipeline_architect,
    )

    # --- Store validated topology in shared memory ---
    g.update_memory(
        data=verified,
        key="pipeline_topology",
        space="episodic",
    )

    g.print("=== Pipeline Topology ===\n{verified}")
    g.done(verified)


if __name__ == "__main__":
    print(pipeline_explorer._graph.to_air())
