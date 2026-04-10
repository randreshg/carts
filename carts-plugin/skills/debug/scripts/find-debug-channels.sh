#!/usr/bin/env bash
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git grep -n 'ARTS_DEBUG' -- include lib | sed -E 's#^.*/([^/]+)\.cpp:.*#\1#' | tr '[:upper:]' '[:lower:]' | sort -u
