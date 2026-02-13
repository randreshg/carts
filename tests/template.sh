#!/bin/bash

# Main experiment (CARTS vs. OMP in single node) + multi-node scaling
carts benchmarks slurm-run \
    --nodes=1,2 \
    --arts-config=configs/arts-3T.cfg \
    --profile=configs/profile-timing.cfg \
    --size=small \
    --runs=5 \
    --time=00:01:00 \
    --port-start=50000

# Workload profiling with both ARTS counters and Linux HW counters
carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts-3T.cfg \
    --profile=configs/profile-workload.cfg \
    --size=small \
    --runs=1 \
    --time=00:01:00 \
    --perf \
    --port-start=51000

# Single node multi-thread scaling with 2T threads (ARTS vs. OMP)
carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts-2T.cfg \
    --profile=configs/profile-timing.cfg \
    --size=small \
    --runs=5 \
    --time=00:01:00 \
    --port-start=52000
