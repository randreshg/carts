# CARTS Docker Multi-Node Runtime

This directory contains Docker image assets and cluster configuration for running CARTS in a 6-node Slurm-backed local cluster.

## Prerequisites

1. Install and start Docker (or Colima on macOS).

```bash
brew install colima
colima start --cpu 16 --memory 32
```

2. Verify Docker is available:

```bash
docker ps
```

## Layout

- `Dockerfile`: Base image definition (`arts-node-base`)
- `slurm.conf`: Slurm cluster config copied into the image
- `arts-docker.cfg`: Primary ARTS runtime config for Docker cluster
- `arts-docker-2node.cfg`: 2-node variant used by tests
- `.dockerignore`: Minimal build context
- `scripts/docker_cli.py`: Python Docker orchestration implementation

## CLI Commands

All operations are exposed through the CARTS CLI:

```bash
carts docker build [--force]
carts docker start
carts docker status
carts docker exec -- sinfo
carts docker update [--arts|--polygeist|--llvm|--carts] [--force] [--debug|--info]
carts docker commit
carts docker stop
carts docker clean
```

### Command Semantics

- `carts docker build`: Builds `arts-node-base`, creates/initializes `carts-workspace`
- `carts docker start`: Starts 6 containers (`arts-node-1`..`arts-node-6`) and Slurm daemons
- `carts docker stop`: Stops running CARTS containers and Slurm processes
- `carts docker commit`: Captures current workspace into `arts-node:built`
- `carts docker clean`: Removes CARTS Docker containers, images, volumes, and prunes unused Docker state

## Typical Workflow

```bash
# One-time or when rebuilding image/workspace from scratch
carts docker build

# Start cluster
carts docker start

# Verify Slurm
carts docker exec -- sinfo
carts docker exec -- srun --nodes=6 hostname

# Run benchmark CLI through Slurm mode
carts docker exec -- carts benchmarks run --slurm polybench/gemm --nodes=1,2,4,6 --runs=3

# Stop or clean
carts docker stop
# carts docker clean
```

## Notes

- Slurm is always started by `carts docker start`; no SSH launcher path is used.
- Containers share one Docker volume at `/opt/carts` (`carts-workspace`).
- `carts docker exec` defaults to node 1; use `--node N` for another node.
