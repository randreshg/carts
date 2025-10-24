#!/bin/bash

# Docker Run Script for CARTS Multi-Node Testing
# This script loads the pre-built image and optionally runs ARTS examples across multiple containers

set -e

# Source common print functions
source "$(dirname "$0")/../tools/config/carts-print.sh"

# Parse arguments
EXAMPLE_FILE=""
BUILD_ARGS=""
RUNTIME_ARGS=""

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
carts_info "Starting CARTS multi-node containers"

# Change to docker directory
cd "$(dirname "$0")"

# Check if the built image exists
if ! docker images | grep -q "arts-node.*built"; then
    carts_error "arts-node:built image not found!"
    carts_warning "Please run './docker-build.sh' first to build the base image."
    exit 1
fi

# Clean up any existing containers
carts_step "Cleaning up existing containers"
docker rm -f arts-node-1 arts-node-2 arts-node-3 arts-node-4 arts-node-5 arts-node-6 >/dev/null 2>&1 || true

# Start all containers from prebuilt image
carts_step "Starting 6 ARTS node containers"
docker run -d --name arts-node-1 --hostname arts-node-1 --network bridge -e ARTS_NODE_ID=0 arts-node:built >/dev/null 2>&1
docker run -d --name arts-node-2 --hostname arts-node-2 --network bridge -e ARTS_NODE_ID=1 arts-node:built >/dev/null 2>&1
docker run -d --name arts-node-3 --hostname arts-node-3 --network bridge -e ARTS_NODE_ID=2 arts-node:built >/dev/null 2>&1
docker run -d --name arts-node-4 --hostname arts-node-4 --network bridge -e ARTS_NODE_ID=3 arts-node:built >/dev/null 2>&1
docker run -d --name arts-node-5 --hostname arts-node-5 --network bridge -e ARTS_NODE_ID=4 arts-node:built >/dev/null 2>&1
docker run -d --name arts-node-6 --hostname arts-node-6 --network bridge -e ARTS_NODE_ID=5 arts-node:built >/dev/null 2>&1

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

carts_header "Multi-Node Setup Complete"
carts_success "CARTS multi-node containers are ready!"
carts_container_status

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
    carts_substep "Directory: $CONTAINER_FILE_DIR"
    carts_substep "File: $FILE_NAME"
    carts_substep "Binary: $BASE_NAME"

    # ARTS config file path inside the container
    REMOTE_ARTS_CONFIG="/opt/carts/docker/arts-docker.cfg"

    # First: build the binary on the master node
    REMOTE_BUILD_CMD="cd '$CONTAINER_FILE_DIR' && carts execute '$FILE_NAME' --run-args --arts-config=$REMOTE_ARTS_CONFIG"
    if [[ -n "$BUILD_ARGS" ]]; then
        REMOTE_BUILD_CMD+=" $BUILD_ARGS"
    fi
    carts_build "Build command: $REMOTE_BUILD_CMD"
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

    # Then: copy the built binary to all other nodes so SSH launcher can start it remotely
    # Only copy to numbered arts-node containers (1-6), not builder/update containers
    OTHER_NODES=$(docker ps --format '{{.Names}}' | grep '^arts-node-[1-6]$' | grep -v "$MASTER_CONTAINER" || true)
    for node in $OTHER_NODES; do
        NODE_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$node")
        carts_info "Copying binary to $node ($NODE_IP): $CONTAINER_FILE_DIR/$BASE_NAME"
        docker exec "$MASTER_CONTAINER" bash -c "scp -o StrictHostKeyChecking=no -o LogLevel=ERROR '$CONTAINER_FILE_DIR/$BASE_NAME' root@$node:'$CONTAINER_FILE_DIR/'"
        if [[ $? -ne 0 ]]; then
            carts_error "Failed to copy binary to $node"
            exit 1
        fi
    done

    # Finally: run the binary on the master node (ARTS will SSH to others with config from environment)
    REMOTE_RUN_CMD="cd '$CONTAINER_FILE_DIR' && artsConfig=$REMOTE_ARTS_CONFIG ./$BASE_NAME"
    if [[ -n "$RUNTIME_ARGS" ]]; then
        REMOTE_RUN_CMD+=" $RUNTIME_ARGS"
    fi
    carts_build "Run command: $REMOTE_RUN_CMD"
    docker exec "$MASTER_CONTAINER" bash -c "$REMOTE_RUN_CMD"
fi
