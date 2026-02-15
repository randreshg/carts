"""Clean command logic for CARTS CLI."""

from pathlib import Path
import os
import shutil

from carts_styles import print_debug, print_info, print_success
from scripts.config import is_verbose


# Files to clean
CLEAN_FILE_PATTERNS = [
    "*.mlir", "*.ll", "*.o", "*.so", "*.dylib", "*.a", "*.exe",
    "DbGraph_*.dot", "*.log", "*.tmp", ".artsPrintLock",
    ".carts-metadata.json", "*_arts_metadata.mlir", "core",
]

CLEAN_DIR_PATTERNS = [
    "build", "cmake-build-*", ".cache", "introspection", "counter", "counters",
]


def run_local_clean() -> None:
    """Clean local generated files."""
    print_info("Cleaning generated files in current directory...")

    removed_count = 0
    cwd = Path.cwd()

    # Remove files matching patterns
    for pattern in CLEAN_FILE_PATTERNS:
        for f in cwd.glob(pattern):
            if f.is_file():
                if is_verbose():
                    print_debug(f"Removing file: {f}")
                f.unlink()
                removed_count += 1

    # Remove directories matching patterns
    for pattern in CLEAN_DIR_PATTERNS:
        for d in cwd.glob(pattern):
            if d.is_dir():
                if is_verbose():
                    print_debug(f"Removing directory: {d}")
                shutil.rmtree(d)
                removed_count += 1

    # Remove executables that look like CARTS compilation outputs
    # Only remove files ending with _arts or _omp (our naming convention)
    for pattern in ["*_arts", "*_omp"]:
        for f in cwd.glob(pattern):
            if f.is_file() and os.access(f, os.X_OK):
                if is_verbose():
                    print_debug(f"Removing CARTS executable: {f}")
                f.unlink()
                removed_count += 1

    if removed_count == 0:
        print_info("No generated files found to clean")
    else:
        print_success(f"Cleaned {removed_count} files/directories")
