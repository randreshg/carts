#!/bin/bash

# Docker Build Script for CARTS Multi-Node Testing
# This script builds the base Docker image with all dependencies installed
# Run this ONCE to create the base image with CARTS pre-built

set -e

# Parse command line arguments
FORCE_MODE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        "--force"|"-f")
            FORCE_MODE=true
            shift
            ;;
        "--help"|"-h")
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --force, -f              Force rebuild everything from scratch"
            echo "  --help, -h               Show this help"
            echo ""
            echo "Examples:"
            echo "  $0                       # Smart build: update existing or build new"
            echo "  $0 --force               # Force rebuild everything from scratch"
            echo ""
            exit 0
            ;;
        *)
            echo "Error: Unknown option: $1"
            echo "Use '$0 --help' for available options"
            exit 1
            ;;
    esac
done

# Source common print functions
source "$(dirname "$0")/../tools/config/carts-print.sh"

carts_header "CARTS Docker Build"
carts_info "Building CARTS Docker base image with all dependencies"

# Check if components already exist
BASE_IMAGE_EXISTS=false
BUILT_IMAGE_EXISTS=false
WORKSPACE_EXISTS=false

if docker images | grep -q "arts-node-base"; then
    BASE_IMAGE_EXISTS=true
fi

if docker images | grep -q "arts-node.*built"; then
    BUILT_IMAGE_EXISTS=true
fi

if docker volume inspect carts-workspace >/dev/null 2>&1; then
    WORKSPACE_EXISTS=true
fi

# Determine build strategy
if [[ "$FORCE_MODE" == "true" ]]; then
    carts_info "Force mode enabled - rebuilding everything from scratch"
elif [[ "$BASE_IMAGE_EXISTS" == "true" && "$BUILT_IMAGE_EXISTS" == "true" && "$WORKSPACE_EXISTS" == "true" ]]; then
    carts_info "All Docker components already exist!"
    carts_line
    carts_info "Using incremental update strategy..."
    carts_info "Run './docker-update.sh' to pull latest changes and rebuild components"
    carts_info "Or run './docker-build.sh --force' to rebuild everything from scratch"
    exit 0
elif [[ "$BASE_IMAGE_EXISTS" == "true" && "$BUILT_IMAGE_EXISTS" == "true" ]]; then
    carts_info "Docker images exist, but workspace may need updating"
    carts_info "Using incremental update strategy..."
    # Fall through to update logic
elif [[ "$BASE_IMAGE_EXISTS" == "true" ]]; then
    carts_info "Base image exists, building CARTS components..."
    # Fall through to build logic
else
    carts_info "No existing components found, performing full build..."
fi

# Change to docker directory
cd "$(dirname "$0")"

# Build the base Docker image with dependencies
if [[ "$BASE_IMAGE_EXISTS" == "false" || "$FORCE_MODE" == "true" ]]; then
    carts_step "Building base Docker image (arts-node-base)"
    docker build -t arts-node-base .
else
    carts_info "Using existing base image: arts-node-base"
fi

# Start builder container with access to all system threads
carts_step "Starting builder container"
docker rm -f arts-node-builder >/dev/null 2>&1 || true

# Detect CPU cores 
DOCKER_CPUS=$(docker run --rm ubuntu:22.04 nproc 2>/dev/null || echo "2")
docker run -d --name arts-node-builder --hostname arts-node-builder --network bridge \
    --cpus="$DOCKER_CPUS" arts-node-base >/dev/null 2>&1

# Wait for builder to be ready
carts_info "Waiting for builder to be ready"
sleep 5

# Clone and build CARTS in builder 
if [[ "$BUILT_IMAGE_EXISTS" == "false" || "$FORCE_MODE" == "true" ]]; then
    carts_step "Cloning and building CARTS in builder (using $DOCKER_CPUS threads)"
    GIT_URL=${GIT_URL:-https://github.com/randreshg/carts.git}
    GIT_BRANCH=${GIT_BRANCH:-mlir}

    docker exec arts-node-builder bash -c "\
        set -e; \
        echo 'Setting up CARTS workspace...'; \
        rm -rf /opt/carts && mkdir -p /opt/carts; \
        git config --global init.defaultBranch main; \
        git config --global pull.rebase false; \
        git config --global core.preloadindex true; \
        git config --global core.fscache true; \
        git config --global gc.auto 256; \
        git clone --branch $GIT_BRANCH --depth 1 --single-branch --no-tags --progress $GIT_URL /opt/carts; \
        cd /opt/carts; \
        export MAKEFLAGS='-j$DOCKER_CPUS'; \
        export CMAKE_BUILD_PARALLEL_LEVEL=$DOCKER_CPUS; \
        python3 -u tools/setup/carts-setup.py; \
    "
else
    carts_info "Using existing CARTS installation from previous build"
fi

# Create shared workspace volume
carts_step "Creating shared workspace volume"
if ! docker volume inspect carts-workspace >/dev/null 2>&1; then
    docker volume create carts-workspace >/dev/null
    carts_info "Created shared volume: carts-workspace"
else
    carts_info "Using existing shared volume: carts-workspace"
fi

# Initialize the volume with CARTS from the builder
if [[ "$WORKSPACE_EXISTS" == "false" || "$FORCE_MODE" == "true" ]]; then
    if [[ "$BUILT_IMAGE_EXISTS" == "false" || "$FORCE_MODE" == "true" ]]; then
        carts_step "Initializing shared volume with CARTS installation"
        docker run --rm -v carts-workspace:/dest arts-node-builder bash -c "cp -a /opt/carts/. /dest/" >/dev/null
        carts_success "Shared volume initialized with CARTS"
    else
        carts_step "Initializing shared volume with CARTS installation"
        # Copy from existing built image instead of builder container
        docker run --rm -v carts-workspace:/dest arts-node:built bash -c "cp -a /opt/carts/. /dest/" >/dev/null
        carts_success "Shared volume initialized with CARTS"
    fi
else
    carts_info "Using existing workspace volume"
fi

# Commit builder as the final reusable image
if [[ "$BUILT_IMAGE_EXISTS" == "false" || "$FORCE_MODE" == "true" ]]; then
    carts_step "Committing builder to arts-node:built image"
    docker commit arts-node-builder arts-node:built >/dev/null
else
    carts_info "Using existing arts-node:built image"
fi

# Clean up builder container
carts_step "Cleaning up builder container"
docker stop arts-node-builder >/dev/null
docker rm arts-node-builder >/dev/null

carts_header "Build Complete"
if [[ "$FORCE_MODE" == "true" ]]; then
    carts_success "Force rebuild completed successfully!"
else
    carts_success "Smart build completed successfully!"
fi

carts_info "Docker components status:"
carts_info "  - arts-node-base: $(docker images | grep -q "arts-node-base" && echo " Available" || echo " Missing")"
carts_info "  - arts-node:built: $(docker images | grep -q "arts-node.*built" && echo " Available" || echo " Missing")"
carts_info "  - carts-workspace: $(docker volume inspect carts-workspace >/dev/null 2>&1 && echo " Available" || echo " Missing")"

carts_next_steps \
    "Run './docker-run.sh' to start multi-node containers" \
    "Run './docker-update.sh' to pull latest changes and rebuild" \
    "Workspace volume 'carts-workspace' is ready for multi-node testing"

carts_info "To verify the build:"
carts_info "  docker images | grep arts-node"
carts_info "  docker volume ls | grep carts-workspace"
