#!/usr/bin/env python3
"""WF14 — Full Feature Lifecycle (META workflow).

End-to-end feature development: explores prior work, plans, runs parallel
design by three architects, negotiates a unified design, checkpoints,
implements/tests/documents in parallel, runs a review loop, verifies, and
reflects.  Exercises ALL major APXM operations.

Topology:
  query_memory  -->  plan  -->  3x parallel design  -->  negotiate
  -->  checkpoint  -->  3x parallel (implement | test | document)
  -->  wait_all  -->  loop(review, count=2)  -->  verify  -->  reflect
  -->  update_memory
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude, codex

from .experts import (
    arts_runtime_expert,
    codegen_specialist,
    documentation_scribe,
    mlir_dialect_engineer,
    pipeline_architect,
    test_engineer,
)


@compile()
def full_feature_lifecycle(
    g: GraphRecorder,
    feature_name: str,
    feature_spec: str,
    target_dialect: str,
):
    """Full lifecycle: design, negotiate, implement, test, document, review, verify."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- spawn all agents ---
    architect_spawn = g.spawn("architect", profile=claude, cwd=cwd)
    dialect_spawn = g.spawn("dialect_engineer", profile=claude, cwd=cwd)
    runtime_spawn = g.spawn("runtime_expert", profile=claude, cwd=cwd)
    codegen_spawn = g.spawn("coder", profile=codex, cwd=cwd)
    tester_spawn = g.spawn("tester", profile=claude, cwd=cwd)
    scribe_spawn = g.spawn("scribe", profile=claude, cwd=cwd)

    # --- recall prior ADRs and design decisions ---
    prior_adrs = g.query_memory(
        query="Prior architecture decision records and design rationale for {target_dialect}",
        key="prior_adrs",
    )

    # --- plan the feature ---
    feature_plan = g.plan(
        goal=(
            "Decompose the following feature into implementation tasks.\n\n"
            "Feature: {feature_name}\n"
            "Specification:\n{feature_spec}\n"
            "Target dialect: {target_dialect}\n\n"
            "Prior ADRs:\n{prior_adrs}\n\n"
            "Break down into:\n"
            "1. Dialect changes (ODS, TableGen, canonicalization patterns)\n"
            "2. Pipeline integration (which stages need modification, ordering constraints)\n"
            "3. Runtime support (EDT/DB/epoch changes if any)\n"
            "4. Codegen (C++ pass implementation, LLVM lowering)\n"
            "5. Testing (lit tests, edge cases, regression matrix)\n"
            "6. Documentation (ADR, API docs, knowledge base)\n"
            "Identify cross-cutting concerns and ordering dependencies."
        ),
    )

    # --- three parallel design proposals ---
    pipeline_design = g.ask(
        "Design the pipeline integration for feature '{feature_name}'.\n\n"
        "Plan:\n{feature_plan}\n"
        "Target dialect: {target_dialect}\n\n"
        "Address:\n"
        "- Which pipeline stages need modification (reference `carts pipeline --json`)\n"
        "- Stage ordering constraints: can this be done without reordering?\n"
        "- New stage required? If so, where in the 18-stage sequence?\n"
        "- Cross-stage data flow: what does this feature produce/consume?\n"
        "- Parallelism impact: does this affect concurrent stage execution?",
        agent=pipeline_architect,
    )

    dialect_design = g.ask(
        "Design the MLIR dialect changes for feature '{feature_name}'.\n\n"
        "Plan:\n{feature_plan}\n"
        "Target dialect: {target_dialect}\n\n"
        "Address:\n"
        "- New operations needed in ArtsOps.td (ODS definitions)\n"
        "- New types or attributes in ArtsTypes.td\n"
        "- Canonicalization patterns with PatternBenefit scores\n"
        "- DRR rewrite rules for optimizations\n"
        "- Pass registration in Passes.td\n"
        "- Use AttrNames::Operation constants, not hardcoded strings",
        agent=mlir_dialect_engineer,
    )

    runtime_design = g.ask(
        "Design the ARTS runtime integration for feature '{feature_name}'.\n\n"
        "Plan:\n{feature_plan}\n"
        "Target dialect: {target_dialect}\n\n"
        "Address:\n"
        "- EDT lifecycle changes: new states or transitions?\n"
        "- DB access mode requirements: new modes or mode interactions?\n"
        "- Epoch synchronization impact\n"
        "- CPS chain modifications\n"
        "- Runtime call interface: which ARTS APIs to invoke\n"
        "- Thread-safety: AnalysisManager access patterns",
        agent=arts_runtime_expert,
    )

    # --- negotiate unified design ---
    unified_design = g.negotiate(
        parties=["architect", "dialect_engineer", "runtime_expert"],
        proposal=(
            "Negotiate a unified design that resolves conflicts between the three proposals.\n\n"
            "Pipeline design:\n{pipeline_design}\n\n"
            "Dialect design:\n{dialect_design}\n\n"
            "Runtime design:\n{runtime_design}\n\n"
            "Resolve:\n"
            "1. Pipeline ordering vs dialect dependency conflicts\n"
            "2. Runtime API constraints that affect dialect op semantics\n"
            "3. Performance trade-offs between pipeline integration approaches\n"
            "Produce a single, consistent design document."
        ),
        max_rounds=3,
    )

    # --- checkpoint before implementation ---
    design_checkpoint = g.checkpoint("design_approved")

    # --- three parallel implementation streams ---
    implementation = g.ask(
        "Implement feature '{feature_name}' following the unified design.\n\n"
        "Unified design:\n{unified_design}\n\n"
        "Produce complete, compilable C++17 code:\n"
        "- Pass implementation in lib/arts/passes/\n"
        "- Headers in include/arts/passes/\n"
        "- Pipeline registration in tools/compile/Compile.cpp\n"
        "- Use LLVM coding style, AttrNames constants, AM->getDbAnalysis()\n"
        "- Ensure thread-safety: no global/static mutable state",
        agent=codegen_specialist,
    )

    tests = g.ask(
        "Create comprehensive tests for feature '{feature_name}'.\n\n"
        "Unified design:\n{unified_design}\n\n"
        "Create:\n"
        "- FileCheck lit tests in tests/contracts/ using %carts-compile substitution\n"
        "- Edge case coverage: empty loops, single-element DBs, degenerate partitions\n"
        "- Regression test from the feature spec's examples\n"
        "- Test matrix: vary input dimensions, access modes, EDT counts\n"
        "- Negative tests: verify proper error messages for invalid inputs\n"
        "Test the contract, not the implementation.",
        agent=test_engineer,
    )

    documentation = g.ask(
        "Write documentation for feature '{feature_name}'.\n\n"
        "Unified design:\n{unified_design}\n\n"
        "Produce:\n"
        "- ADR (Architecture Decision Record): Title, Status, Context, Decision, Consequences\n"
        "- API documentation: Doxygen-style for new C++ classes and functions\n"
        "- Knowledge base article for docs/compiler/\n"
        "- Include concrete examples and cross-references to tests and source files\n"
        "- Document the WHY, not just the WHAT",
        agent=documentation_scribe,
    )

    # --- wait for all implementation streams ---
    impl_done = g.wait_all("implementation_complete", [implementation, tests, documentation])

    # --- review loop (2 iterations) ---
    loop_start = g.loop_start(count=2)

    review = g.reason(
        "Review iteration for feature '{feature_name}'.\n\n"
        "Implementation:\n{implementation}\n"
        "Tests:\n{tests}\n"
        "Documentation:\n{documentation}\n\n"
        "Review checklist:\n"
        "1. Code correctness: does implementation match unified design?\n"
        "2. Test coverage: are all design requirements tested?\n"
        "3. Documentation accuracy: does doc match actual implementation?\n"
        "4. Style compliance: LLVM coding style, no hardcoded strings\n"
        "5. Thread-safety: no global/static mutable state\n"
        "6. Cross-cutting: pipeline ordering preserved, dialect ops consistent\n\n"
        "Output specific issues to fix in each area.",
    )

    loop_end = g.loop_end()
    loop_start >> review
    review >> loop_end
    loop_end >> loop_start

    # --- final verification ---
    verified = g.verify(
        claim="Feature '{feature_name}' is complete, correct, tested, and documented",
        evidence=(
            "Unified design:\n{unified_design}\n\n"
            "Implementation:\n{implementation}\n"
            "Tests:\n{tests}\n"
            "Documentation:\n{documentation}\n"
            "Review findings:\n{review}\n\n"
            "Verify:\n"
            "1. All design requirements are implemented\n"
            "2. All tests pass (FileCheck lit tests)\n"
            "3. Documentation is complete and accurate\n"
            "4. No regressions introduced\n"
            "5. Feature integrates cleanly into the 18-stage pipeline"
        ),
    )

    # --- reflect on the process ---
    reflection = g.reflect(trace_id="full_feature_lifecycle")

    # --- persist to memory ---
    g.update_memory(
        data=verified,
        key="feature_lifecycle",
        space="episodic",
    )

    g.print(
        "=== Feature Lifecycle Complete ===\n\n"
        "Feature: {feature_name}\n"
        "Target dialect: {target_dialect}\n\n"
        "Verification:\n{verified}\n\n"
        "Reflection:\n{reflection}"
    )
    g.done(verified)


if __name__ == "__main__":
    print(full_feature_lifecycle._graph.to_air())
