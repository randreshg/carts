#!/bin/bash

# Docker Commit Script for CARTS Multi-Node Testing
# This script commits the current state of arts-node-1 (including workspace volume)
# to update the arts-node:built image

set -e

# Source common print functions
source "$(dirname "$0")/docker-print.sh"

carts_header "CARTS Docker Commit"
carts_info "Committing arts-node-1 container state to arts-node:built image"

# Change to docker directory
cd "$(dirname "$0")"

# Check if arts-node-base exists
if ! docker images | grep -q "arts-node-base"; then
    carts_error "arts-node-base image not found!"
    carts_warning "Please run './docker-build.sh' first to build the base image."
    exit 1
fi

# Check if arts-node-1 container exists
if ! docker ps -a --format '{{.Names}}' | grep -q '^arts-node-1$'; then
    carts_error "arts-node-1 container not found!"
    carts_warning "Please run './docker-run.sh' first to start containers."
    exit 1
fi

# Check if workspace volume exists
if ! docker volume inspect carts-workspace >/dev/null 2>&1; then
    carts_error "carts-workspace volume not found!"
    carts_warning "Please run './docker-build.sh' first to create the workspace volume."
    exit 1
fi

carts_step "Creating temporary container from arts-node-base"
# Clean up any existing temp container
docker rm -f arts-node-commit-temp >/dev/null 2>&1 || true

# Create and start temporary container WITHOUT volume mount (so /opt/carts is part of filesystem)
# We use 'docker run -d' to start it in background so we can exec into it
docker run -d --name arts-node-commit-temp arts-node-base >/dev/null 2>&1

# Wait a moment for container to be ready
sleep 2

carts_step "Copying workspace content from volume to container filesystem"
# Ensure /opt/carts exists in temp container (it should from base image, but ensure it's empty)
docker exec arts-node-commit-temp bash -c "rm -rf /opt/carts/* /opt/carts/.* 2>/dev/null || true; mkdir -p /opt/carts" >/dev/null 2>&1

# Copy workspace content from arts-node-1 to temp container
# Docker doesn't support copying directly between containers, so we use a two-step process:
# 1. Copy from source container to a temporary location on host
# 2. Copy from host to destination container
carts_info "Copying workspace content (this may take a few minutes for large workspaces)..."
TEMP_COPY_DIR=$(mktemp -d)
trap "rm -rf $TEMP_COPY_DIR" EXIT

# Step 1: Copy from arts-node-1 to host
docker cp arts-node-1:/opt/carts/. "$TEMP_COPY_DIR/" >/dev/null 2>&1

# Step 2: Copy from host to temp container
docker cp "$TEMP_COPY_DIR/." arts-node-commit-temp:/opt/carts/ >/dev/null 2>&1

# Clean up temp directory
rm -rf "$TEMP_COPY_DIR"
trap - EXIT

# Verify the copy was successful by checking for key directories
if ! docker exec arts-node-commit-temp test -d /opt/carts/.git; then
    carts_error "Failed to copy workspace content to temporary container"
    carts_warning "Workspace may be empty or copy failed"
    docker rm -f arts-node-commit-temp >/dev/null 2>&1 || true
    exit 1
fi
carts_success "Workspace content copied successfully"

carts_step "Removing old arts-node:built image to free space"
# Remove old arts-node:built image to free space before committing new one
if docker images | grep -q "arts-node.*built"; then
    OLD_IMAGE_SIZE=$(docker images --format "{{.Size}}" arts-node:built 2>/dev/null | head -1)
    if [[ -n "$OLD_IMAGE_SIZE" ]]; then
        carts_info "Removing old arts-node:built image ($OLD_IMAGE_SIZE) to free space..."
        docker rmi arts-node:built >/dev/null 2>&1 || true
        carts_success "Old image removed, freed approximately $OLD_IMAGE_SIZE"
    fi
else
    carts_info "No existing arts-node:built image to remove"
fi

carts_step "Committing container to arts-node:built image"
# Commit the temporary container to arts-node:built
# Capture error output in case of failure
COMMIT_ERROR=$(docker commit arts-node-commit-temp arts-node:built 2>&1)
COMMIT_EXIT_CODE=$?

if [[ $COMMIT_EXIT_CODE -ne 0 ]]; then
    carts_error "Failed to commit container to arts-node:built image"
    carts_warning "Error: $COMMIT_ERROR"
    
    # Check if it's a disk space issue
    if echo "$COMMIT_ERROR" | grep -q "no space left on device"; then
        carts_error "Docker ran out of disk space!"
        carts_info "Try cleaning up Docker to free space:"
        carts_info "  docker system prune -a --volumes"
        carts_info "  docker builder prune -a"
        carts_info "  docker image prune -a"
    fi
    
    docker rm -f arts-node-commit-temp >/dev/null 2>&1 || true
    exit 1
fi

if docker images | grep -q "arts-node:built"; then
    carts_success "Successfully committed to arts-node:built image"
    
    # Show image size
    IMAGE_SIZE=$(docker images --format "{{.Size}}" arts-node:built | head -1)
    carts_info "Image size: $IMAGE_SIZE"
else
    carts_error "Failed to create arts-node:built image"
    docker rm -f arts-node-commit-temp >/dev/null 2>&1 || true
    exit 1
fi

carts_step "Cleaning up temporary container"
docker rm -f arts-node-commit-temp >/dev/null 2>&1 || true

carts_header "Commit Complete"
carts_success "arts-node:built image has been updated with latest workspace content!"

carts_info "The updated image includes:"
carts_info "  - All system dependencies from arts-node-base"
carts_info "  - Complete CARTS workspace from /opt/carts"
carts_info "  - All changes made in arts-node-1"

carts_next_steps \
    "Future volume initializations will use this updated image" \
    "Run './docker-run.sh' to restart containers (they'll continue using the volume)" \
    "The image is ready for distribution or backup"
