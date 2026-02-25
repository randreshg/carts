"""Update command logic for CARTS CLI."""

from pathlib import Path
from typing import Dict, List, Optional, Set

import typer

from carts_styles import print_info, print_step, print_success, print_warning, print_error
from scripts.config import get_config
from scripts.run import run_subprocess


def _clean_git_tree(repo_dir: Path, label: str) -> None:
    """Discard local changes and untracked files in a git repository."""
    print_step(f"Cleaning local changes in {label}...")

    result = run_subprocess(
        ["git", "checkout", "."],
        cwd=repo_dir,
        check=False,
    )
    if result.returncode != 0:
        print_error(f"Failed to clean {label}")
        raise typer.Exit(1)

    result = run_subprocess(
        ["git", "clean", "-fd"],
        cwd=repo_dir,
        check=False,
    )
    if result.returncode != 0:
        print_error(f"Failed to clean {label}")
        raise typer.Exit(1)


def _get_submodule_hash(submodule_path: Path) -> Optional[str]:
    """Get current HEAD hash for a submodule path."""
    result = run_subprocess(
        ["git", "-C", str(submodule_path), "rev-parse", "HEAD"],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        return None
    return result.stdout.strip()


def update(
    arts: bool = typer.Option(
        False, "--arts", "-a", help="Update ARTS submodule"),
    polygeist: bool = typer.Option(
        False, "--polygeist", "-p", help="Update Polygeist submodule"),
    benchmarks: bool = typer.Option(
        False, "--benchmarks", "-b", help="Update benchmarks submodule"),
    init: bool = typer.Option(
        False, "--init", help="Initialize submodules if not present"),
    force: bool = typer.Option(
        False, "--force", "-f", help="Discard local changes before updating"),
):
    """Update git submodules to latest remote commits."""
    config = get_config()
    carts_dir = config.carts_dir

    # If no specific flags, update all
    update_all = not (arts or polygeist or benchmarks)

    submodules: List[str] = []
    if update_all or arts:
        submodules.append("external/arts")
    if update_all or polygeist:
        submodules.append("external/Polygeist")
    if update_all or benchmarks:
        submodules.append("external/carts-benchmarks")

    if force:
        print_warning("Force mode enabled: discarding local changes before update")
        _clean_git_tree(carts_dir, "CARTS repository")
        for submodule in submodules:
            submodule_path = carts_dir / submodule
            if not submodule_path.exists():
                print_warning(f"Skipping cleanup for {submodule} (not initialized)")
                continue
            if _get_submodule_hash(submodule_path) is None:
                print_warning(f"Skipping cleanup for {submodule} (not initialized)")
                continue
            _clean_git_tree(submodule_path, submodule)

    # Pull latest CARTS repo changes
    print_info("Pulling latest CARTS changes...")
    result = run_subprocess(
        ["git", "pull"],
        cwd=carts_dir,
        check=False,
    )
    if result.returncode != 0:
        print_error("Failed to pull CARTS repository")
        raise typer.Exit(1)

    before_hashes: Dict[str, Optional[str]] = {}
    for submodule in submodules:
        submodule_path = carts_dir / submodule
        before_hashes[submodule] = _get_submodule_hash(submodule_path)
        if before_hashes[submodule] is None:
            print_warning(f"Could not read current hash for {submodule}")

    base_cmd = ["git", "submodule", "update", "--remote"]
    if init:
        base_cmd.extend(["--init", "--recursive"])

    changed_submodules: Set[str] = set()
    for submodule in submodules:
        print_info(f"Updating {submodule}...")
        result = run_subprocess(
            base_cmd + [submodule],
            cwd=carts_dir,
            check=False,
        )
        if result.returncode != 0:
            print_error(f"Failed to update {submodule}")
            raise typer.Exit(1)

        submodule_path = carts_dir / submodule
        after_hash = _get_submodule_hash(submodule_path)
        if after_hash is None:
            print_warning(f"Could not read updated hash for {submodule}")
        if before_hashes.get(submodule) != after_hash:
            changed_submodules.add(submodule)

    print_success(f"Updated {len(submodules)} submodule(s)")

    if not changed_submodules:
        print_info("Already up to date")
        return

    if "external/Polygeist" in changed_submodules:
        print_step("Rebuilding Polygeist...")
        result = run_subprocess(
            ["make", "polygeist"],
            cwd=carts_dir,
            check=False,
        )
        if result.returncode != 0:
            print_error("Failed to rebuild Polygeist")
            raise typer.Exit(1)

        if (carts_dir / "build" / "build.ninja").is_file():
            print_step("Rebuilding carts-compile-only after Polygeist update...")
            result = run_subprocess(
                ["make", "carts-compile-only"],
                cwd=carts_dir,
                check=False,
            )
            if result.returncode != 0:
                print_error("Failed to rebuild carts-compile-only")
                raise typer.Exit(1)

    if "external/arts" in changed_submodules:
        print_step("Rebuilding ARTS...")
        result = run_subprocess(
            ["make", "arts"],
            cwd=carts_dir,
            check=False,
        )
        if result.returncode != 0:
            print_error("Failed to rebuild ARTS")
            raise typer.Exit(1)
