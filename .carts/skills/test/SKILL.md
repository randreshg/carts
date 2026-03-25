---
name: carts-test
description: Run CARTS test suite, lit tests, or individual test files. Use when the user asks to test, run tests, validate, check, or verify changes.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob
argument-hint: [<test-file.mlir> | --suite contracts | -v]
---

# CARTS Test

Run the CARTS test suite. Run `carts test --help` and `carts lit --help` for latest options.

## Common Commands

```bash
carts test                                     # Run contract tests
carts test --suite all -v                      # All tests, verbose
carts lit tests/contracts/my_test.mlir         # Single test
carts lit -v tests/contracts/                  # Directory, verbose
carts lit -- --filter=pattern tests/contracts  # Filter by pattern
```

## Test Organization

- `tests/contracts/` — MLIR lit tests (compiler pass regression, FileCheck)
- `tests/examples/` — Integration tests (end-to-end compile + run)

## Writing New Tests

```mlir
// RUN: %carts-compile %s --O3 --arts-config %S/../examples/arts.cfg --pipeline <stage> | %FileCheck %s
// CHECK: expected_output
module { ... }
```

Key patterns:
- Use `%carts-compile` substitution (not raw binary paths)
- Use `%S` for test source directory (access sibling files like `arts.cfg`)
- Use `%FileCheck` for output verification
- Test one pipeline stage per test file for isolation
- Cover both positive (CHECK) and negative (CHECK-NOT) assertions

## Debugging Test Failures

```bash
# Run with verbose output to see FileCheck diff
carts lit -v tests/contracts/failing_test.mlir

# Run the command manually to inspect full output
carts compile tests/contracts/failing_test.mlir --pipeline=<stage>
```

## Instructions

When the user asks to test:
1. Run `carts test $ARGUMENTS` or `carts lit $ARGUMENTS`
2. Report pass/fail counts and failure details
3. For failures, read the failing test and analyze the issue
4. If the test needs updating after a pass change, update the CHECK lines
