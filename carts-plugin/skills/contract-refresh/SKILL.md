---
name: carts-contract-refresh
description: Refresh contract test fixture snapshots after IR-affecting pass changes. Use when contract tests fail due to legitimate IR changes, after refactoring passes, or when git status shows stale Output/ fixtures.
user-invocable: true
allowed-tools: Bash, Read, Write, Grep, Glob, Agent
argument-hint: [<test-pattern> | --all | --changed]
---

# CARTS Contract Fixture Refresh

## Purpose

After any IR-affecting pass change, contract test fixtures in
`tests/contracts/Output/` become stale. This skill automates the regeneration,
validation, and commit of updated fixture snapshots.

**This is the #1 most repetitive task** — ~5.5% of all commits are fixture
refreshes, often touching 10-30 `.mlir` files in a single batch.

## When to Use

- Contract tests fail after a legitimate pass change (not a bug)
- `git status` shows many modified files in `tests/contracts/Output/`
- After adding/modifying/reordering passes in the pipeline
- After `dekk carts test` reveals stale FileCheck expectations

## Workflow

### Phase 1: Identify Stale Fixtures

```bash
# See which fixture files changed
git diff --name-only -- tests/contracts/Output/

# Run full test suite to see failures
dekk carts test 2>&1 | grep -E "FAIL:|XFAIL:" | head -30

# Run specific subset
dekk carts lit tests/contracts/<category>/
```

### Phase 2: Regenerate Fixtures

For tests using `CARTS_COMPILE_WORKDIR`:
```bash
# Single test regeneration
dekk carts lit tests/contracts/<test>.mlir

# The workdir output is automatically captured in Output/<test>.mlir.tmp.compile/
```

For tests with inline CHECK patterns (no Output/ directory):
```bash
# Dump the actual output and compare
dekk carts compile tests/contracts/<test>.mlir --pipeline=<stage> > /tmp/actual.mlir
# Update CHECK lines in the test file to match actual output
```

### Phase 3: Validate

```bash
# Re-run affected tests to confirm they pass
dekk carts lit tests/contracts/<category>/

# Run full suite to catch cascading failures
dekk carts test
```

### Phase 4: Review and Commit

```bash
# Review changes are legitimate (not hiding bugs)
git diff -- tests/contracts/Output/ | head -200

# Stage only fixture files
git add tests/contracts/Output/
```

## Key Directories

```
tests/contracts/Output/                    # Fixture snapshots
tests/contracts/core/Output/               # Core dialect fixtures
tests/contracts/rt/Output/                 # RT dialect fixtures
tests/contracts/sde/Output/                # SDE dialect fixtures
tests/contracts/partitioning/safety/Output/ # Partition safety fixtures
```

## Config Files (for test regeneration)

| Config | When to Use |
|--------|-------------|
| `tests/contracts/inputs/arts_1t.cfg` | Single-worker isolation |
| `tests/contracts/inputs/arts_2t.cfg` | Minimal parallelism |
| `tests/contracts/inputs/arts_64t.cfg` | Large-scale validation |
| `tests/examples/arts.cfg` | Integration tests |

## Common Patterns

### Batch refresh after a pass change
```bash
dekk carts test 2>&1 | tee /tmp/test-results.txt
grep "FAIL:" /tmp/test-results.txt | wc -l  # Count failures
dekk carts test  # Regenerate all (fixtures auto-update on rerun)
git diff --stat -- tests/contracts/Output/  # Review scope
```

### Single-stage fixture refresh
```bash
# Only refresh fixtures for a specific pipeline stage
dekk carts lit tests/contracts/ -- --filter="<stage-name>"
```

## Instructions

When the user asks to refresh contract fixtures:

1. Run `dekk carts test` to identify all failures
2. Classify failures: legitimate IR change vs actual bug
3. For legitimate changes, re-run tests to regenerate fixtures
4. Validate by running the full suite again
5. Show `git diff --stat` for review
6. Stage and describe changes for commit (do NOT auto-commit)
