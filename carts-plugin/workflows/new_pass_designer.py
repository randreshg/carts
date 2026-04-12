#!/usr/bin/env python3
"""WF3 -- New Pass Designer: full lifecycle for creating a new CARTS MLIR pass.

Decomposes the task via plan(), runs architect + dialect engineer in parallel for
design, codegen + test engineer in parallel for implementation, then iterates
through review loops before handing off to the documentation scribe.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude, codex

from .experts import (
    codegen_specialist,
    documentation_scribe,
    mlir_dialect_engineer,
    pipeline_architect,
    test_engineer,
)


@compile()
def new_pass_designer(
    g: GraphRecorder,
    pass_name: str,
    pass_description: str,
    target_dialect: str,
):
    """Design, implement, test, and document a new CARTS compiler pass."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn agents ---
    architect_spawn = g.spawn("architect", profile=claude, cwd=cwd)
    dialect_spawn = g.spawn("dialect_engineer", profile=claude, cwd=cwd)
    codegen_spawn = g.spawn("codegen", profile=codex, cwd=cwd)
    tester_spawn = g.spawn("test_engineer", profile=claude, cwd=cwd)
    scribe_spawn = g.spawn("scribe", profile=claude, cwd=cwd)

    # --- Phase 1: Goal decomposition ---
    decomposition = g.plan(
        goal=(
            "Design and implement a new CARTS MLIR pass called '{pass_name}' "
            "targeting the {target_dialect} dialect.\n\n"
            "Description: {pass_description}\n\n"
            "Decompose into:\n"
            "1. Pipeline integration analysis -- where in the 18-stage pipeline does this pass belong?\n"
            "2. ODS operation/type design -- what new ops, types, or attributes are needed?\n"
            "3. C++ pass implementation -- FunctionPass or OperationPass, rewrite patterns\n"
            "4. FileCheck lit tests -- contract tests, edge cases, regression tests\n"
            "5. Documentation -- ADR, API docs, usage examples"
        ),
        agent=pipeline_architect,
    )

    # --- Phase 2: Parallel design ---
    # Architect: pipeline placement and data-flow analysis
    arch_design = g.think(
        "Analyze where pass '{pass_name}' fits in the CARTS pipeline.\n\n"
        "Plan: {decomposition}\n\n"
        "Determine:\n"
        "- Which pipeline stage(s) this pass should run before/after\n"
        "- What IR dialect(s) the pass consumes and produces\n"
        "- What analyses (DbAnalysis, EdtAnalysis) it needs\n"
        "- Ordering constraints with existing passes\n"
        "- Whether it can run in parallel with other stages\n\n"
        "Output a pipeline integration spec with concrete stage names from "
        "`carts pipeline --json`.",
        agent=pipeline_architect,
    )

    # Dialect engineer: ODS design for new operations
    dialect_design = g.think(
        "Design the MLIR ODS definitions needed for pass '{pass_name}' "
        "in the {target_dialect} dialect.\n\n"
        "Plan: {decomposition}\n\n"
        "Specify:\n"
        "- New operations (TableGen ODS): name, operands, results, attributes, traits\n"
        "- New types or attributes if needed\n"
        "- Canonicalization patterns with PatternBenefit rankings\n"
        "- Verifier constraints\n"
        "- DRR rewrite rules if applicable\n\n"
        "Follow LLVM style. Use AttrNames::Operation constants, never hardcoded strings.",
        agent=mlir_dialect_engineer,
    )

    # --- Phase 3: Parallel implementation from spec ---
    design_merged = g.merge("design_spec", [arch_design, dialect_design])

    # Codegen specialist implements the pass
    implementation = g.ask(
        "Implement the CARTS pass '{pass_name}' based on these specifications:\n\n"
        "Pipeline integration:\n{arch_design}\n\n"
        "Dialect design:\n{dialect_design}\n\n"
        "Requirements:\n"
        "- Create the pass header in include/arts/passes/\n"
        "- Create the pass source in lib/arts/passes/\n"
        "- Register in Passes.td and Passes.h\n"
        "- Add to the pipeline in tools/compile/Compile.cpp\n"
        "- Use AM->getDbAnalysis() and AM->getEdtAnalysis() for graph access\n"
        "- Thread-safe: no global/static mutable state\n"
        "- LLVM style: 2-space indent, CamelCase types, camelCase variables\n\n"
        "Produce complete, compilable C++ files.",
        agent=codegen_specialist,
    )

    # Test engineer writes tests from the spec (independent of implementation)
    tests = g.ask(
        "Create FileCheck lit tests for the CARTS pass '{pass_name}'.\n\n"
        "Pipeline spec:\n{arch_design}\n\n"
        "Dialect spec:\n{dialect_design}\n\n"
        "Write tests in the per-dialect test directory (e.g. lib/arts/dialect/sde/test/, "
        "lib/arts/dialect/core/test/, lib/arts/dialect/rt/test/) covering:\n"
        "- Basic functionality: does the pass transform IR correctly?\n"
        "- Edge cases: empty input, single-element, degenerate configurations\n"
        "- Negative tests: CHECK-NOT for patterns that should NOT appear\n"
        "- Integration: pass interacts correctly with adjacent pipeline stages\n\n"
        "Use the format:\n"
        "// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg "
        "--pipeline <stage> | %FileCheck %s\n\n"
        "Test the contract, not the implementation.",
        agent=test_engineer,
    )

    # --- Phase 4: Iterative review loop (3 rounds) ---
    impl_merged = g.merge("impl_bundle", [implementation, tests])

    ls = g.loop_start(count=3)
    impl_merged >> ls

    review = g.think(
        "Review iteration for pass '{pass_name}'.\n\n"
        "Implementation:\n{implementation}\n\n"
        "Tests:\n{tests}\n\n"
        "Check:\n"
        "1. Does the implementation match the pipeline integration spec?\n"
        "2. Are all ODS-defined operations handled?\n"
        "3. Thread safety: any global/static mutable state?\n"
        "4. Are AttrNames constants used (no hardcoded strings)?\n"
        "5. Do tests cover the contract adequately?\n"
        "6. Edge cases: empty loops, single-element DBs, zero-trip loops?\n\n"
        "Provide specific, actionable feedback. If the implementation is correct, "
        "say LGTM with no changes needed.",
        agent=pipeline_architect,
    )

    le = g.loop_end()
    ls >> review
    review >> le
    le >> ls  # back edge

    # --- Phase 5: Documentation ---
    docs = g.ask(
        "Write documentation for the new CARTS pass '{pass_name}'.\n\n"
        "Implementation:\n{implementation}\n\n"
        "Review feedback:\n{review}\n\n"
        "Produce:\n"
        "1. An ADR (Architecture Decision Record) explaining why this pass exists, "
        "what alternatives were considered, and the consequences of the design.\n"
        "2. API documentation (Doxygen-style) for the pass header.\n"
        "3. A usage section showing how to invoke the pass via `carts compile --pipeline`.\n"
        "4. Cross-references to related passes, tests, and dialect definitions.\n\n"
        "Write for the developer who joins next month. Document the WHY.",
        agent=documentation_scribe,
    )
    le >> docs

    # --- Phase 6: Persist to memory ---
    g.update_memory(
        data=docs,
        key=f"pass_design_{pass_name}",
        space="episodic",
    )

    g.print(
        "=== NEW PASS DESIGN COMPLETE ===\n\n"
        "Pass: {pass_name}\n"
        "Target dialect: {target_dialect}\n\n"
        "Design:\n{arch_design}\n\n"
        "Documentation:\n{docs}"
    )
    g.done(docs)


if __name__ == "__main__":
    print(new_pass_designer._graph.to_air())
