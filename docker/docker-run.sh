#!/bin/bash

# Docker Run Script for CARTS Multi-Node Testing
# This script loads the pre-built image and optionally runs ARTS examples across multiple containers

set -e

# Source common print functions
source "$(dirname "$0")/docker-print.sh"

# Parse arguments
EXAMPLE_FILE=""
BUILD_ARGS=""
RUNTIME_ARGS=""
USE_SLURM=false

# First pass: extract --slurm flag from arguments
ARGS=()
for arg in "$@"; do
    if [[ "$arg" == "--slurm" ]]; then
        USE_SLURM=true
    else
        ARGS+=("$arg")
    fi
done
set -- "${ARGS[@]}"

if [[ $# -gt 0 ]]; then
    EXAMPLE_FILE="$1"
    shift

    # Parse remaining arguments, using -- as separator between build and runtime args
    while [[ $# -gt 0 ]]; do
        if [[ "$1" == "--" ]]; then
            shift
            # Everything after -- goes to runtime args
            RUNTIME_ARGS="$*"
            break
        else
            # Everything before -- goes to build args
            BUILD_ARGS="$BUILD_ARGS $1"
            shift
        fi
    done
fi

carts_header "CARTS Multi-Node Containers"
if [[ "$USE_SLURM" == true ]]; then
    carts_info "Slurm mode enabled"
fi
if [[ -n "$EXAMPLE_FILE" ]]; then
    carts_info "Starting CARTS multi-node containers and running example: $EXAMPLE_FILE"
else
    carts_info "Starting fresh CARTS multi-node containers (no example file provided)"
    carts_info "This will kill any existing containers and start new ones"
fi

# Change to docker directory
cd "$(dirname "$0")"

# Check if the base image exists
if ! docker images | grep -q "arts-node-base"; then
    carts_error "arts-node-base image not found!"
    carts_warning "Please run './docker-build.sh' first to build the base image."
    exit 1
fi

# Clean up any existing containers using docker-kill.sh
carts_step "Cleaning up existing containers"
carts_info "Calling docker-kill.sh to kill processes..."

# Call docker-kill.sh script for thorough cleanup
if [[ -f "docker-kill.sh" ]]; then
    # Run docker-kill.sh and capture its output
    if ./docker-kill.sh; then
        carts_info "docker-kill.sh completed successfully"
    else
        carts_info "No running containers to kill"
    fi
else
    carts_error "docker-kill.sh not found!"
    exit 1
fi

# Remove any stopped containers that might still exist
STOPPED_CONTAINERS=$(docker ps -a --format '{{.Names}}' | grep '^arts-node-' | sort)
if [[ -n "$STOPPED_CONTAINERS" ]]; then
    carts_info "Removing stopped containers: $STOPPED_CONTAINERS"
    docker rm -f $STOPPED_CONTAINERS >/dev/null 2>&1 || true
fi
carts_success "Existing containers cleaned up"

# Create shared volume if it doesn't exist
carts_step "Setting up shared workspace volume"
if ! docker volume inspect carts-workspace >/dev/null 2>&1; then
    docker volume create carts-workspace >/dev/null
    carts_info "Created new shared volume: carts-workspace"

    # Initialize the volume with CARTS from the built image
    carts_info "Initializing shared volume with CARTS installation"
    docker run --rm -v carts-workspace:/dest arts-node:built bash -c "cp -a /opt/carts/. /dest/" >/dev/null
    carts_success "Shared volume initialized with CARTS"
else
    carts_info "Using existing shared volume: carts-workspace"
fi

# Start all containers from prebuilt image with shared volume
carts_step "Starting 6 ARTS node containers with shared workspace"
docker run -d --name arts-node-1 --hostname arts-node-1 --network bridge -e ARTS_NODE_ID=0 -v carts-workspace:/opt/carts arts-node-base >/dev/null 2>&1
docker run -d --name arts-node-2 --hostname arts-node-2 --network bridge -e ARTS_NODE_ID=1 -v carts-workspace:/opt/carts arts-node-base >/dev/null 2>&1
docker run -d --name arts-node-3 --hostname arts-node-3 --network bridge -e ARTS_NODE_ID=2 -v carts-workspace:/opt/carts arts-node-base >/dev/null 2>&1
docker run -d --name arts-node-4 --hostname arts-node-4 --network bridge -e ARTS_NODE_ID=3 -v carts-workspace:/opt/carts arts-node-base >/dev/null 2>&1
docker run -d --name arts-node-5 --hostname arts-node-5 --network bridge -e ARTS_NODE_ID=4 -v carts-workspace:/opt/carts arts-node-base >/dev/null 2>&1
docker run -d --name arts-node-6 --hostname arts-node-6 --network bridge -e ARTS_NODE_ID=5 -v carts-workspace:/opt/carts arts-node-base >/dev/null 2>&1

# Wait for containers to be ready
carts_info "Waiting for containers to be ready"
sleep 3

# Get all arts-node containers
ALL_NODES=$(docker ps --format '{{.Names}}' | grep '^arts-node-' | sort)

# Setup hostname resolution in /etc/hosts for all containers
carts_step "Setting up hostname resolution"
for node_i in $ALL_NODES; do
    # Build hosts entries for all nodes
    HOSTS_ENTRIES=""
    for node_j in $ALL_NODES; do
        NODE_J_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$node_j")
        HOSTS_ENTRIES="${HOSTS_ENTRIES}${NODE_J_IP} ${node_j}\n"
    done
    # Add entries to /etc/hosts
    docker exec "$node_i" bash -c "echo -e \"$HOSTS_ENTRIES\" >> /etc/hosts"
done

# Test SSH connectivity between first two nodes (if they exist)
FIRST_NODE=$(echo "$ALL_NODES" | head -1)
SECOND_NODE=$(echo "$ALL_NODES" | head -2 | tail -1)
if [ -n "$FIRST_NODE" ] && [ -n "$SECOND_NODE" ] && [ "$FIRST_NODE" != "$SECOND_NODE" ]; then
    docker exec "$FIRST_NODE" bash -c "ssh -o StrictHostKeyChecking=no -o LogLevel=ERROR root@$SECOND_NODE 'echo SSH test successful'" >/dev/null 2>&1
fi

# Start Slurm daemons if --slurm flag was provided
if [[ "$USE_SLURM" == true ]]; then
    carts_step "Starting Slurm cluster daemons"

    # Copy slurm.conf from shared volume to /etc/slurm on all nodes
    # (in case image was not rebuilt yet, ensures latest config)
    for node in $ALL_NODES; do
        docker exec "$node" bash -c "cp /opt/carts/docker/slurm/slurm.conf /etc/slurm/slurm.conf 2>/dev/null || true"
    done

    # Fix permissions for munge and slurm directories
    for node in $ALL_NODES; do
        docker exec "$node" bash -c "
            chown -R munge:munge /etc/munge /var/log/munge /run/munge 2>/dev/null || true
            chmod 400 /etc/munge/munge.key
            chmod 755 /var/spool/slurmctld /var/spool/slurmd /var/log/slurm
        "
    done

    # Start munged on all nodes
    carts_info "Starting munge authentication on all nodes"
    for node in $ALL_NODES; do
        docker exec "$node" bash -c "munged --force" 2>/dev/null
    done

    # Start slurmctld on arts-node-1 (controller)
    carts_info "Starting Slurm controller on arts-node-1"
    docker exec arts-node-1 bash -c "slurmctld" 2>/dev/null
    sleep 1

    # Start slurmd on all nodes
    carts_info "Starting Slurm workers on all nodes"
    for node in $ALL_NODES; do
        docker exec "$node" bash -c "slurmd" 2>/dev/null
    done
    sleep 2

    # Verify Slurm cluster
    carts_info "Verifying Slurm cluster status:"
    docker exec arts-node-1 sinfo || carts_warning "sinfo failed - Slurm may need a moment to initialize"

    carts_success "Slurm cluster is running"
fi

carts_header "Multi-Node Setup Complete"
carts_success "CARTS multi-node containers are ready!"
if [[ "$USE_SLURM" == true ]]; then
    carts_info "Slurm cluster active - use 'docker exec arts-node-1 sinfo' to check status"
fi
carts_container_status

if [[ -z "$EXAMPLE_FILE" ]]; then
    carts_info "No example file provided - containers are ready for manual use"
    carts_info "You can now SSH into any container or run examples manually"
fi

# If an example file was provided, execute it
if [[ -n "$EXAMPLE_FILE" ]]; then
    carts_header "Executing Example"
    carts_info "Executing example: $EXAMPLE_FILE"

    # Find the test file using find command
    CARTS_DIR="$(cd .. && pwd)"
    TEST_FILE=""

    # First, try the file as provided (handles absolute paths and relative paths with directories)
    if [[ -f "$EXAMPLE_FILE" ]]; then
        TEST_FILE="$(realpath "$EXAMPLE_FILE")"
    else
        # Use find to search in tests and examples directories
        TEST_FILE=$(find "${CARTS_DIR}/tests" "${CARTS_DIR}/examples" -name "$EXAMPLE_FILE" -type f 2>/dev/null | head -1)

        # If not found, try with basename only (in case full path was provided)
        if [[ -z "$TEST_FILE" ]]; then
            BASENAME_FILE=$(basename "$EXAMPLE_FILE")
            TEST_FILE=$(find "${CARTS_DIR}/tests" "${CARTS_DIR}/examples" -name "$BASENAME_FILE" -type f 2>/dev/null | head -1)
        fi
    fi

    if [[ -z "$TEST_FILE" ]]; then
        carts_error "Example file '$EXAMPLE_FILE' not found in tests or examples directories"
        exit 1
    fi

    carts_success "Found test file: $TEST_FILE"

    # Execute the pipeline and run the binary inside the master container
    MASTER_CONTAINER="arts-node-1"
    FILE_DIR=$(dirname "$TEST_FILE")
    FILE_NAME=$(basename "$TEST_FILE")
    BASE_NAME="${FILE_NAME%.*}"

    # Convert host path to container path
    RELATIVE_PATH="${FILE_DIR#*/carts/}"
    CONTAINER_FILE_DIR="/opt/carts/$RELATIVE_PATH"

    carts_info "Executing inside master container ($MASTER_CONTAINER):"
    carts_info "  Directory: $CONTAINER_FILE_DIR"
    carts_info "  File: $FILE_NAME"
    carts_info "  Binary: $BASE_NAME"

    # Select ARTS config based on mode
    if [[ "$USE_SLURM" == true ]]; then
        REMOTE_ARTS_CONFIG="/opt/carts/docker/arts-docker-slurm.cfg"
    else
        REMOTE_ARTS_CONFIG="/opt/carts/docker/arts-docker.cfg"
    fi

    # First: build the binary on the master node
    REMOTE_BUILD_CMD="cd '$CONTAINER_FILE_DIR' && carts compile '$FILE_NAME' --run-args --arts-config=$REMOTE_ARTS_CONFIG"
    if [[ -n "$BUILD_ARGS" ]]; then
        REMOTE_BUILD_CMD+=" $BUILD_ARGS"
    fi
    carts_info "Build command: $REMOTE_BUILD_CMD"
    docker exec "$MASTER_CONTAINER" bash -c "$REMOTE_BUILD_CMD"
    if [[ $? -ne 0 ]]; then
        carts_error "Failed to build binary on master node"
        exit 1
    fi

    # Verify the binary was created
    docker exec "$MASTER_CONTAINER" bash -c "test -f '$CONTAINER_FILE_DIR/$BASE_NAME'"
    if [[ $? -ne 0 ]]; then
        carts_error "Binary '$BASE_NAME' was not created successfully"
        exit 1
    fi

    carts_success "Binary built successfully in shared workspace - visible to all nodes"

    # Run the binary
    if [[ "$USE_SLURM" == true ]]; then
        # Launch via srun across all 6 nodes
        REMOTE_RUN_CMD="cd '$CONTAINER_FILE_DIR' && srun --nodes=6 --ntasks-per-node=1 --cpus-per-task=4 env artsConfig=$REMOTE_ARTS_CONFIG ./$BASE_NAME"
        if [[ -n "$RUNTIME_ARGS" ]]; then
            REMOTE_RUN_CMD+=" $RUNTIME_ARGS"
        fi
        carts_info "Slurm run command: $REMOTE_RUN_CMD"
        docker exec "$MASTER_CONTAINER" bash -c "$REMOTE_RUN_CMD"
    else
        # Run directly on master node (ARTS will SSH to others with config from environment)
        # No need to copy binaries - all nodes share the same /opt/carts workspace
        REMOTE_RUN_CMD="cd '$CONTAINER_FILE_DIR' && artsConfig=$REMOTE_ARTS_CONFIG ./$BASE_NAME"
        if [[ -n "$RUNTIME_ARGS" ]]; then
            REMOTE_RUN_CMD+=" $RUNTIME_ARGS"
        fi
        carts_info "Run command: $REMOTE_RUN_CMD"
        docker exec "$MASTER_CONTAINER" bash -c "$REMOTE_RUN_CMD"
    fi
fi
