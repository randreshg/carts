#!/bin/bash

# Workload profiling with both ARTS counters and Linux HW counters
carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts.cfg \
    --profile=configs/profile-workload.cfg \
    --size=large \
    --runs=1 \
    --time=00:01:00 \
    --perf

# Single node multi-thread scaling with 64, 32, 16, 8, 4, and 2 threads (ARTS vs. OMP)
carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts.cfg \
    --profile=configs/profile-timing.cfg \
    --size=large \
    --runs=5 \
    --time=00:01:00

carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts-32T.cfg \
    --profile=configs/profile-timing.cfg \
    --size=large \
    --runs=5 \
    --time=00:02:00 \

carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts-16T.cfg \
    --profile=configs/profile-timing.cfg \
    --size=large \
    --runs=5 \
    --time=00:04:00

carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts-8T.cfg \
    --profile=configs/profile-timing.cfg \
    --size=large \
    --runs=5 \
    --time=00:08:00

carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts-4T.cfg \
    --profile=configs/profile-timing.cfg \
    --size=large \
    --runs=5 \
    --time=00:16:00

carts benchmarks slurm-run \
    --nodes=1 \
    --arts-config=configs/arts-2T.cfg \
    --profile=configs/profile-timing.cfg \
    --size=large \
    --runs=5 \
    --time=00:32:00

# Main experiment (CARTS vs. OMP in single node) + multi-node scaling
# In case, large dataset first
carts benchmarks slurm-run \
    --nodes=2,4,8,16,32 \
    --arts-config=configs/arts.cfg \
    --profile=configs/profile-timing.cfg \
    --size=large \
    --runs=5 \
    --time=00:10:00 \
    --exclude jacobi-task-dep \
    --exclude poisson-task

# Then extralarge dataset
carts benchmarks slurm-run \
    --nodes=1,2,4,8,16,32 \
    --arts-config=configs/arts.cfg \
    --profile=configs/profile-timing.cfg \
    --size=extralarge \
    --runs=5 \
    --time=00:10:00 \
    --exclude jacobi-task-dep \
    --exclude poisson-task

