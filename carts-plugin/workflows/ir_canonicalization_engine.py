#!/usr/bin/env python3
"""WF6 -- IR Canonicalization Engine: iterative pattern discovery and verification.

The IR canonicalizer analyzes dialect operations, proposes rewrite patterns, then
enters a loop where the codegen specialist implements each pattern, the
canonicalizer verifies semantics preservation, and a guard gates continuation.
"""

import os

from apxm import compile, GraphRecorder
from apxm._generated.agents import claude, codex

from .experts import codegen_specialist, ir_canonicalizer


@compile()
def ir_canonicalization_engine(
    g: GraphRecorder,
    dialect_name: str,
    dialect_ops_file: str,
):
    """Discover, implement, and verify canonicalization patterns for a CARTS dialect."""
    cwd = os.environ.get("CARTS_HOME", os.getcwd())

    # --- Spawn agents ---
    canon_spawn = g.spawn("canonicalizer", profile=claude, cwd=cwd)
    codegen_spawn = g.spawn("codegen", profile=codex, cwd=cwd)

    # --- Phase 1: Analyze dialect operations ---
    op_analysis = g.ask(
        "Analyze the operations defined in {dialect_ops_file} for the '{dialect_name}' "
        "MLIR dialect in the CARTS compiler.\n\n"
        "For each operation, identify:\n"
        "1. Current canonicalization patterns (if any) in ArtsOps.cpp\n"
        "2. Missing patterns that would improve IR quality:\n"
        "   - Dead code elimination opportunities (e.g., db_create with no acquires)\n"
        "   - Redundant operation folding (e.g., consecutive same-mode acquires)\n"
        "   - Identity removal (e.g., db_acquire(RO) on constant data)\n"
        "   - Commutativity normalization (e.g., canonical operand ordering)\n"
        "   - Constant folding opportunities\n"
        "3. PatternBenefit ranking for each proposed pattern\n"
        "4. Preconditions that must hold for each pattern\n\n"
        "Be exhaustive. List every operation and its canonicalization status.",
        agent=ir_canonicalizer,
    )

    # --- Phase 2: Propose pattern set ---
    pattern_spec = g.think(
        "From the analysis of '{dialect_name}' operations, design a complete set "
        "of canonicalization patterns.\n\n"
        "Analysis:\n{op_analysis}\n\n"
        "For each pattern, specify:\n"
        "- Name (e.g., FoldRedundantAcquire)\n"
        "- Input pattern (DAG match)\n"
        "- Output pattern (replacement)\n"
        "- PatternBenefit score (higher = applied first)\n"
        "- Semantics preservation argument\n"
        "- Whether it should be a fold() method, getCanonicalizationPatterns(), or DRR\n\n"
        "Order patterns by benefit score descending. Group by operation.",
        agent=ir_canonicalizer,
    )

    # --- Phase 3: Iterative implement-verify-guard loop ---
    ls = g.loop_start(count=3)
    pattern_spec >> ls

    # Codegen specialist implements patterns
    impl = g.ask(
        "Implement the canonicalization patterns for the '{dialect_name}' dialect.\n\n"
        "Pattern specification:\n{pattern_spec}\n\n"
        "Requirements:\n"
        "- Add fold() methods to operation definitions where appropriate\n"
        "- Add getCanonicalizationPatterns() overrides for complex patterns\n"
        "- Use DRR in .td files for simple 1:1 rewrites\n"
        "- Follow LLVM style: 2-space indent, CamelCase types\n"
        "- Use AttrNames constants, never hardcoded attribute strings\n"
        "- Set PatternBenefit according to the spec\n"
        "- Source location: lib/arts/dialect/ArtsOps.cpp\n\n"
        "Produce complete, compilable C++ code for each pattern.",
        agent=codegen_specialist,
    )
    ls >> impl

    # Verify semantics preservation
    verification = g.verify(
        claim=(
            "The implemented canonicalization patterns for '{dialect_name}' "
            "preserve program semantics: for all valid inputs, the transformed IR "
            "is observationally equivalent to the original."
        ),
        evidence=(
            "Pattern specification:\n{pattern_spec}\n\n"
            "Implementation:\n{impl}\n\n"
            "Verify each pattern:\n"
            "1. Does the implementation match the spec's input/output pattern?\n"
            "2. Are all preconditions checked before applying the pattern?\n"
            "3. Could the pattern fire in an unexpected context and break semantics?\n"
            "4. Are there interactions between patterns that could cause infinite loops?\n"
            "5. Is the PatternBenefit ordering correct to avoid priority conflicts?\n\n"
            "For each pattern, state PASS or FAIL with explanation."
        ),
        agent=ir_canonicalizer,
    )

    # Guard: only continue if verification passed
    g.guard(
        condition="All canonicalization patterns passed semantics verification",
        on_fail="halt",
        source=verification,
    )

    le = g.loop_end()
    verification >> le
    le >> ls  # back edge

    # --- Phase 4: Final consolidated output ---
    final = g.ask(
        "Produce the final consolidated canonicalization implementation for the "
        "'{dialect_name}' dialect.\n\n"
        "Implementation:\n{impl}\n\n"
        "Verification results:\n{verification}\n\n"
        "Produce:\n"
        "1. Complete ArtsOps.cpp changes (fold methods + canonicalization patterns)\n"
        "2. Any .td DRR pattern files\n"
        "3. FileCheck lit tests for each pattern in tests/contracts/\n"
        "4. A summary table: pattern name, benefit, status (PASS/FAIL)\n\n"
        "Only include patterns that passed verification.",
        agent=codegen_specialist,
    )
    le >> final

    g.update_memory(
        data=final,
        key=f"canonicalization_{dialect_name}",
        space="episodic",
    )

    g.print(
        "=== IR CANONICALIZATION COMPLETE ===\n\n"
        "Dialect: {dialect_name}\n"
        "Source: {dialect_ops_file}\n\n"
        "Results:\n{final}"
    )
    g.done(final)


if __name__ == "__main__":
    print(ir_canonicalization_engine._graph.to_air())
