# Benchmark Failure Classification

Classify the failing benchmark before editing compiler code.

## 1. OpenMP Baseline Also Fails

Likely benchmark bug, unsupported source assumption, or bad verification harness.

## 2. OpenMP Works, ARTS Wrong Checksum

Likely compiler miscompile.

First tools:
- `carts-miscompile-triage`
- `scripts/rerun-benchmark.sh`
- `scripts/locate-run-artifacts.sh`

## 3. OpenMP Works, ARTS Times Out

Could be runtime, bad coarsening, epoch serialization, or partition overhead.

First tools:
- `carts-runtime-triage`
- `carts-benchmark-triage`

## 4. Single-Node Works, Multi-Node Fails

Switch to `carts-distributed-triage`.
