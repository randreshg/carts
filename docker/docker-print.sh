#!/bin/bash

# CARTS Docker Print Functions
# This script provides consistent colored output functions for Docker scripts
# Source this file in Docker scripts: source "$(dirname "$0")/docker-print.sh"

# Minimal color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Simple wrapper functions
carts_info() { echo -e "${CYAN}[INFO]${NC} $1"; }
carts_error() { echo -e "${RED}[ERROR]${NC} $1"; }
carts_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
carts_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
carts_step() { echo -e "${CYAN}->${NC} ${BOLD}$1${NC}"; }
carts_header() { echo -e "\n${BOLD}${CYAN}=== $1 ===${NC}\n"; }
carts_line() { echo -e "${CYAN}- - - - - - - - - - - - - - - - - - - - -${NC}"; }
carts_next_steps() {
    echo -e "${CYAN}Next steps:${NC}"
    while [[ $# -gt 0 ]]; do
        echo -e "  ${CYAN}•${NC} $1"
        shift
    done
}

carts_container_status() {
    echo -e "${CYAN}Container status:${NC}"
    docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" | grep -E "^(NAMES|arts-node-)" || echo -e "  ${YELLOW}No containers running${NC}"
}
