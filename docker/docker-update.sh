#!/bin/bash

# Docker Update Script for CARTS Multi-Node Testing
# This script pulls latest changes and rebuilds CARTS components

set -e

# Source common print functions
source "$(dirname "$0")/../tools/config/carts-print.sh"

# Parse command line arguments
FORCE_MODE=false
FORCE_ARTS=false
FORCE_POLYGEIST=false
FORCE_LLVM=false
FORCE_CARTS=false
SKIP_PULL=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        "--force"|"-f")
            FORCE_MODE=true
            shift
            ;;
        "--arts"|"-a")
            FORCE_ARTS=true
            shift
            ;;
        "--polygeist"|"-p")
            FORCE_POLYGEIST=true
            shift
            ;;
        "--llvm"|"-l")
            FORCE_LLVM=true
            shift
            ;;
        "--carts"|"-c")
            FORCE_CARTS=true
            shift
            ;;
        "--no-pull"|"-n")
            SKIP_PULL=true
            shift
            ;;
        "--help"|"-h")
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --force, -f        Force rebuild mode (rebuilds even without changes)"
            echo "  --arts, -a         Force rebuild ARTS runtime"
            echo "  --polygeist, -p    Force rebuild Polygeist compiler"
            echo "  --llvm, -l         Force rebuild LLVM compiler"
            echo "  --carts, -c        Force rebuild CARTS framework"
            echo "  --no-pull, -n      Skip pulling changes from repositories"
            echo "  --help, -h         Show this help"
            echo ""
            echo "Examples:"
            echo "  $0                # Auto-pull changes and update if needed"
            echo "  $0 --arts -f      # Force rebuild ARTS"
            echo "  $0 --llvm -f      # Force rebuild LLVM"
            echo "  $0 -f             # Force rebuild everything"
            echo "  $0 --no-pull      # Update without pulling changes"
            exit 0
            ;;
        *)
            carts_error "Unknown option: $1"
            carts_info "Use '$0 --help' for available options"
            exit 1
            ;;
    esac
done

carts_header "CARTS Docker Update"
if [[ "$FORCE_MODE" == "true" ]]; then
    carts_info "Updating CARTS components in Docker (FORCE MODE)"
elif [[ "$SKIP_PULL" == "true" ]]; then
    carts_info "Updating CARTS components in Docker (skipping pull)"
else
    carts_info "Updating CARTS components in Docker (auto-pulling changes)"
fi

# Automatically handle repository updates (unless skipped)
if [[ "$SKIP_PULL" == "false" ]]; then
    carts_step "Checking for and pulling repository changes"
    
    # Get the current directory (should be the docker directory)
    DOCKER_DIR="$(cd "$(dirname "$0")" && pwd)"
    CARTS_DIR="$(dirname "$DOCKER_DIR")"
    
    # Function to automatically handle repository changes
    auto_update_repo() {
        local repo_path="$1"
        local repo_name="$2"
        
        if [[ ! -d "$repo_path/.git" ]]; then
            carts_warning "$repo_name ($repo_path): Not a git repository"
            return 0
        fi
        
        # Check for local changes
        local has_changes=false
        if ! git -C "$repo_path" diff-index --quiet HEAD -- 2>/dev/null; then
            has_changes=true
        fi
        if ! git -C "$repo_path" diff --cached --quiet 2>/dev/null; then
            has_changes=true
        fi
        
        if [[ "$has_changes" == "true" ]]; then
            carts_warning "$repo_name: Local changes detected, stashing them..."
            git -C "$repo_path" stash push -m "Auto-stash before docker update $(date)" || {
                carts_error "$repo_name: Failed to stash changes"
                return 1
            }
            carts_success "$repo_name: Local changes stashed"
        fi
        
        # Always try to pull latest changes
        carts_info "$repo_name: Checking for remote changes..."
        git -C "$repo_path" fetch origin || {
            carts_error "$repo_name: Failed to fetch from origin"
            return 1
        }
        
        # Get current branch
        local current_branch=$(git -C "$repo_path" branch --show-current)
        if [[ -n "$current_branch" ]]; then
            # Check if there are remote changes
            local behind_count=$(git -C "$repo_path" rev-list --count HEAD..origin/"$current_branch" 2>/dev/null || echo "0")
            if [[ "$behind_count" -gt 0 ]]; then
                carts_info "$repo_name: Pulling $behind_count new commits..."
                git -C "$repo_path" pull origin "$current_branch" || {
                    carts_error "$repo_name: Failed to pull changes"
                    return 1
                }
                carts_success "$repo_name: Updated to latest changes"
            else
                carts_success "$repo_name: Already up to date"
            fi
        else
            carts_warning "$repo_name: No current branch, skipping pull"
        fi
        
        return 0
    }
    
    # Update CARTS main repository
    if ! auto_update_repo "$CARTS_DIR" "CARTS"; then
        carts_error "Failed to update CARTS repository"
        exit 1
    fi
    
    carts_line
fi

# Skip local changes check since we handle them automatically above
carts_step "Proceeding with Docker update"

# Change to docker directory
cd "$(dirname "$0")"

# Check if the built image exists
if ! docker images | grep -q "arts-node.*built"; then
    carts_error "arts-node:built image not found!"
    carts_warning "Please run './docker-build.sh' first to build the base image."
    exit 1
fi

# Detect CPU cores
DOCKER_CPUS=$(docker run --rm ubuntu:22.04 nproc 2>/dev/null || echo "2")
carts_step "Starting update container with full CPU access"
docker rm -f arts-node-update >/dev/null 2>&1 || true
docker run -d --name arts-node-update --hostname arts-node-update --network bridge \
    --cpus="$DOCKER_CPUS" arts-node:built

# Wait for container to be ready
carts_info "Waiting for update container to be ready"
sleep 5

# Update CARTS components using Git submodules
carts_step "Updating submodules and checking for changes"
GIT_BRANCH=${GIT_BRANCH:-mlir}

# Update submodules and check for changes
carts_info "Updating Git submodules..."
SUBMODULE_CHANGES=$(docker exec arts-node-update bash -c "
    set -e
    cd /opt/carts

    # Check current submodule commits before update
    declare -A before_commits
    while read -r commit path; do
        submodule_name=\$(basename \"\$path\")
        before_commits[\$submodule_name]=\"\$commit\"
    done < <(git submodule status --quiet | awk '{print \$1, \$2}')

    # Update submodules to latest remote versions
    git submodule update --remote --quiet 2>/dev/null || true

    # Check which submodules changed by comparing commits
    git submodule status --quiet | while read -r line; do
        commit=\$(echo \"\$line\" | awk '{print \$1}')
        path=\$(echo \"\$line\" | awk '{print \$2}')
        submodule_name=\$(basename \"\$path\")
        before_commit=\${before_commits[\$submodule_name]}
        if [[ \"\$commit\" != \"\$before_commit\" ]]; then
            echo \"\$submodule_name:changed\"
        fi
    done

    # Always check if main repo has changes (from our auto-pull above)
    if ! git diff-index --quiet HEAD --; then
        echo 'CARTS:changed'
    fi
" 2>/dev/null || echo "ERROR: Failed to update submodules")

# Parse submodule changes
BUILD_LLVM=false
BUILD_POLY=false
BUILD_ARTS=false
BUILD_CARTS=false

while read -r change; do
    case "$change" in
        "Polygeist:changed"|"llvm-project:changed")
            BUILD_LLVM=true
            BUILD_POLY=true
            ;;
        "arts:changed")
            BUILD_ARTS=true
            ;;
        "CARTS:changed")
            BUILD_CARTS=true
            ;;
    esac
done <<< "$SUBMODULE_CHANGES"

# Apply force flags (overrides change detection)
if [[ "$FORCE_MODE" == "true" ]]; then
    BUILD_LLVM=true
    BUILD_POLY=true
    BUILD_ARTS=true
    BUILD_CARTS=true
elif [[ "$FORCE_ARTS" == "true" ]]; then
    BUILD_ARTS=true
elif [[ "$FORCE_POLYGEIST" == "true" ]]; then
    BUILD_POLY=true
elif [[ "$FORCE_LLVM" == "true" ]]; then
    BUILD_LLVM=true
    BUILD_POLY=true  # LLVM changes require rebuilding Polygeist
elif [[ "$FORCE_CARTS" == "true" ]]; then
    BUILD_CARTS=true
fi

carts_line
if [[ "$FORCE_MODE" == "true" ]]; then
    carts_info "Build Mode: FORCE (rebuilding everything)"
else
    carts_info "Submodule Update Results:"
fi

# Show what changed
changed_any=false
if [[ "$BUILD_LLVM" == "true" ]]; then
    carts_warning "LLVM/Polygeist: Changes detected or force rebuild"
    changed_any=true
fi
if [[ "$BUILD_ARTS" == "true" ]]; then
    carts_warning "ARTS: Changes detected or force rebuild"
    changed_any=true
fi
if [[ "$BUILD_CARTS" == "true" ]]; then
    carts_warning "CARTS: Changes detected or force rebuild"
    changed_any=true
fi

if [[ "$changed_any" == "false" ]]; then
    carts_success "No submodule changes detected."
fi

# Show force rebuild indicators
if [[ "$FORCE_ARTS" == "true" ]]; then
    carts_warning "ARTS: Force rebuild enabled"
fi
if [[ "$FORCE_POLYGEIST" == "true" ]]; then
    carts_warning "Polygeist: Force rebuild enabled"
fi
if [[ "$FORCE_LLVM" == "true" ]]; then
    carts_warning "LLVM: Force rebuild enabled"
fi
if [[ "$FORCE_CARTS" == "true" ]]; then
    carts_warning "CARTS: Force rebuild enabled"
fi
carts_line

# If nothing changed and no force flags, skip
if [[ "$BUILD_LLVM" == "false" && "$BUILD_POLY" == "false" && "$BUILD_ARTS" == "false" && "$BUILD_CARTS" == "false" ]]; then
    carts_success "No changes detected. Skipping build process."
    carts_step "Cleaning up update container"
    docker stop arts-node-update >/dev/null
    docker rm arts-node-update >/dev/null
    carts_header "Update Complete"
    carts_success "No changes needed - arts-node:built image is already up to date!"
    exit 0
fi

carts_step "Changes detected. Proceeding with selective rebuild..."
# Rebuild components that changed
docker exec arts-node-update bash -c "\
    set -e; \
    cd /opt/carts; \
    export MAKEFLAGS='-j$DOCKER_CPUS'; \
    [[ \"$BUILD_LLVM\" == \"true\" ]] && { carts build --llvm; }; \
    [[ \"$BUILD_POLY\" == \"true\" ]] && { carts build --polygeist; }; \
    [[ \"$BUILD_ARTS\" == \"true\" ]] && { carts build --arts; }; \
    [[ \"$BUILD_LLVM\" == \"true\" || \"$BUILD_POLY\" == \"true\" || \"$BUILD_ARTS\" == \"true\" || \"$BUILD_CARTS\" == \"true\" ]] && { carts build; } \
"

# Commit updated container as new image
carts_step "Committing updated container to arts-node:built image"
docker commit arts-node-update arts-node:built >/dev/null

# Clean up update container
carts_step "Cleaning up update container"
docker stop arts-node-update >/dev/null
docker rm arts-node-update >/dev/null

# Run docker-run.sh
./docker-run.sh

carts_header "Update Complete"
carts_success "CARTS Docker update completed successfully!"

