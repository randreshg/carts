# Reproducer Reduction Checklist

## Preserve First

- exact failing stage
- exact checksum / verifier / FileCheck / crash signal
- exact command line and config

## Remove Aggressively

- unrelated arrays
- helper functions not in the failing slice
- independent loops
- extra EDTs and acquires
- attributes unrelated to the symptom

## Keep Aggressively

- the dep pattern
- the partition mode / full-range behavior
- `distribution_*` attrs if distribution strategy is relevant
- live metadata IDs only when the verifier or lowering consumes them as input facts

## Prefer the Smallest Test Form

- C/C++ only if the frontend matters
- MLIR if the failure is already after frontend lowering
- co-located dialect `.mlir` tests for stage-local compiler behavior
- `tests/e2e/*` or `samples/*` only when runtime execution is required
