#!/usr/bin/env python3
"""WF9 -- Architecture Explorer: multi-perspective design space exploration.

Three domain experts (pipeline architect, runtime expert, dialect engineer)
each propose an architectural design from their perspective. A cross-critique
identifies tensions, a negotiate round forces a unified decision, and a
handoff transfers the result to the documentation scribe for an ADR.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude

from .experts import (
    arts_runtime_expert,
    documentation_scribe,
    mlir_dialect_engineer,
    pipeline_architect,
)


@compile()
def architecture_explorer(
    g: GraphRecorder,
    architecture_question: str,
    constraints: str,
):
    """Explore an architectural question from pipeline, runtime, and IR perspectives."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn agents ---
    architect_spawn = g.spawn("architect", profile=claude, cwd=cwd)
    runtime_spawn = g.spawn("runtime_expert", profile=claude, cwd=cwd)
    dialect_spawn = g.spawn("dialect_engineer", profile=claude, cwd=cwd)
    scribe_spawn = g.spawn("scribe", profile=claude, cwd=cwd)

    # --- Phase 1: Three parallel design proposals ---
    # Pipeline perspective
    pipeline_proposal = g.ask(
        "Propose an architectural design for this CARTS question from the "
        "PIPELINE perspective.\n\n"
        "Question: {architecture_question}\n"
        "Constraints: {constraints}\n\n"
        "Consider:\n"
        "- How does this affect the 18-stage pipeline ordering?\n"
        "- Which stages need modification or new stages?\n"
        "- What are the data-flow implications between stages?\n"
        "- Impact on compilation time and pipeline parallelism\n"
        "- Backward compatibility with existing .mlir inputs\n\n"
        "Provide a concrete proposal with:\n"
        "1. Proposed pipeline changes (stage additions, reorderings, modifications)\n"
        "2. Data-flow diagram (ASCII art) showing stage interactions\n"
        "3. Trade-offs and risks\n"
        "4. Estimated implementation effort",
        agent=pipeline_architect,
    )

    # Runtime perspective
    runtime_proposal = g.ask(
        "Propose an architectural design for this CARTS question from the "
        "ARTS RUNTIME perspective.\n\n"
        "Question: {architecture_question}\n"
        "Constraints: {constraints}\n\n"
        "Consider:\n"
        "- How does this affect EDT lifecycle and scheduling?\n"
        "- Impact on DataBlock access modes and partitioning\n"
        "- Epoch synchronization implications\n"
        "- CPS chain structure changes\n"
        "- Distributed execution and data migration effects\n"
        "- Runtime configuration requirements\n\n"
        "Provide a concrete proposal with:\n"
        "1. Proposed runtime changes (new EDT patterns, DB modes, epoch semantics)\n"
        "2. EDT dependency graph (ASCII art) for a representative case\n"
        "3. Trade-offs: performance vs correctness vs complexity\n"
        "4. Estimated implementation effort",
        agent=arts_runtime_expert,
    )

    # IR/Dialect perspective
    ir_proposal = g.ask(
        "Propose an architectural design for this CARTS question from the "
        "MLIR DIALECT perspective.\n\n"
        "Question: {architecture_question}\n"
        "Constraints: {constraints}\n\n"
        "Consider:\n"
        "- What new operations, types, or attributes are needed in the ARTS dialect?\n"
        "- How do these fit into the existing type system (!arts.edt, !arts.db, !arts.epoch)?\n"
        "- Canonicalization and optimization opportunities\n"
        "- Lowering path to LLVM IR\n"
        "- TableGen/ODS design choices\n"
        "- Impact on existing rewrite patterns\n\n"
        "Provide a concrete proposal with:\n"
        "1. Proposed dialect changes (new ops, types, attributes in TableGen notation)\n"
        "2. Type system diagram showing relationships\n"
        "3. Trade-offs: expressiveness vs compilation complexity\n"
        "4. Estimated implementation effort",
        agent=mlir_dialect_engineer,
    )

    # --- Phase 2: Cross-critique ---
    cross_critique = g.think(
        "Cross-critique the three architectural proposals for: {architecture_question}\n\n"
        "Pipeline proposal:\n{pipeline_proposal}\n\n"
        "Runtime proposal:\n{runtime_proposal}\n\n"
        "IR/Dialect proposal:\n{ir_proposal}\n\n"
        "Constraints: {constraints}\n\n"
        "Analyze:\n"
        "1. Where do the proposals AGREE? (These are likely strong design points.)\n"
        "2. Where do they CONFLICT? (These require resolution.)\n"
        "3. What does each proposal miss that another covers?\n"
        "4. Are there implicit assumptions that contradict the constraints?\n"
        "5. Can the proposals be composed into a unified design? What would that look like?\n"
        "6. Which proposal best satisfies the constraints?\n\n"
        "Produce a synthesis document identifying agreements, conflicts, and a "
        "recommended direction.",
        agent=pipeline_architect,
    )

    # --- Phase 3: Negotiate unified design ---
    unified_design = g.negotiate(
        parties=["pipeline_architect", "arts_runtime_expert", "mlir_dialect_engineer"],
        proposal=(
            "Reach consensus on a unified architectural design for: {architecture_question}\n\n"
            "Cross-critique:\n{cross_critique}\n\n"
            "Constraints: {constraints}\n\n"
            "Each party must:\n"
            "1. Acknowledge the cross-critique findings\n"
            "2. Defend the essential elements of their proposal\n"
            "3. Concede on points where another perspective is stronger\n"
            "4. Propose specific compromises for conflicting elements\n\n"
            "The final design must be a single coherent architecture that all parties "
            "agree on. It must respect all stated constraints."
        ),
        max_rounds=3,
    )

    # --- Phase 4: Handoff to documentation scribe ---
    g.handoff(from_agent=architect_spawn, to_agent=scribe_spawn)
    unified_design >> scribe_spawn

    # --- Phase 5: Scribe produces ADR ---
    adr = g.ask(
        "Write an Architecture Decision Record (ADR) documenting the design decision "
        "for: {architecture_question}\n\n"
        "Unified design:\n{unified_design}\n\n"
        "Cross-critique:\n{cross_critique}\n\n"
        "Constraints: {constraints}\n\n"
        "ADR format:\n"
        "# ADR-NNN: <Title>\n\n"
        "## Status\nProposed\n\n"
        "## Context\n"
        "- The architectural question and why it matters\n"
        "- Constraints that shaped the decision\n"
        "- Key tensions between pipeline, runtime, and IR perspectives\n\n"
        "## Decision\n"
        "- The unified design, stated as concrete commitments\n"
        "- Rationale for each major decision point\n"
        "- How conflicts between perspectives were resolved\n\n"
        "## Consequences\n"
        "- Positive: what this enables\n"
        "- Negative: what this costs or constrains\n"
        "- Risks: what could go wrong and mitigations\n\n"
        "## Alternatives Considered\n"
        "- Summarize each perspective's original proposal\n"
        "- Why the alternatives were not chosen\n\n"
        "Write for the developer who joins next month. Document the WHY.",
        agent=documentation_scribe,
    )

    # --- Phase 6: Persist ---
    g.update_memory(
        data=adr,
        key="architecture_decision",
        space="episodic",
    )

    g.print(
        "=== ARCHITECTURE EXPLORATION COMPLETE ===\n\n"
        "Question: {architecture_question}\n\n"
        "Unified design:\n{unified_design}\n\n"
        "ADR:\n{adr}"
    )
    g.done(adr)


if __name__ == "__main__":
    print(architecture_explorer._graph.to_air())
