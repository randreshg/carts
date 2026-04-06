# Async Loop Strategies

This note records the runtime-first strategy split for epoch-bearing time-step
loops in `EpochOpt`.

## Goal

Choose between two asynchronous loop shapes without benchmark-specific
hardcoding:

- `advance_edt`
  - Prefer for epoch-only loops where the only cross-iteration action is
    "iteration finished, launch the next one".
  - Runtime-first target shape:
    `iter_edt -> inner_epoch/workers -> outer_epoch -> advance_edt`.
  - No self-recursive continuation EDT and no `arts_wait_on_handle`.

- `cps_chain`
  - Prefer when the loop needs an inner continuation stage that publishes
    sequential state before the next iteration can be launched.
  - Runtime-first target shape:
    `iter_edt -> inner_epoch/workers -> cont_edt -> advance_edt`.
  - Mutable sequential state should flow through dep-carried DBs, not raw
    pointers in `paramv`.

## Current Structural Conditions

`EpochHeuristics::evaluateAsyncLoopStrategy` classifies loops using the
following production-facing conditions:

- Reject async conversion when:
  - the loop has `iter_args`
  - the loop is nested
  - the trip count is statically `< 2`
  - the loop has no epoch slots
  - an epoch body is empty or unclonable
  - the loop has unsupported side effects
  - the loop has call-based sidecars that require explicit outlining

- Choose `advance_edt` when:
  - the loop body is structurally epoch-only
  - non-slot operations are limited to pure arithmetic or DB
    acquire/release/contract scaffolding
  - there are no sequential inter-epoch or tail sidecars

- Choose `cps_chain` when:
  - the loop has sequential sidecars between epoch slots
  - the loop has sequential tail sidecars after the last epoch
  - the loop needs an inner continuation stage to publish loop-carried state

## Why This Split Exists

The ARTS runtime example in `external/arts/examples/cpu/cps_chain.c` uses an
explicit `cont_edt` and `advance_edt` split. Collapsing those roles into one
self-recursive continuation EDT is a stronger transform and should only be used
when the loop structure actually needs it.

Epoch-only wavefront loops such as Seidel are better modeled as
`advance_edt` candidates. Loops with true sequential state publication belong
on the CPS path.
