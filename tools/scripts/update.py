"""Update command logic for CARTS CLI."""

from pathlib import Path
from typing import Dict, List, Optional

import typer

from carts_styles import print_error, print_info, print_step, print_success, print_warning
from scripts.config import get_config
from scripts.run import run_subprocess


ARTS_REQUIRED_NESTED_SUBMODULES = ["third_party/hwloc"]


def _get_repo_hash(repo_dir: Path) -> Optional[str]:
    """Get current HEAD hash for a git repository."""
    result = run_subprocess(
        ["git", "rev-parse", "HEAD"],
        cwd=repo_dir,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        return None
    return result.stdout.strip()


def _clean_git_tree(repo_dir: Path, label: str) -> None:
    """Discard local changes and untracked files in a git repository."""
    print_step(f"Cleaning local changes in {label}...")

    result = run_subprocess(
        ["git", "reset", "--hard", "HEAD"],
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


def _run_git(cwd: Path, args: List[str], fail_message: str) -> None:
    """Run a git command and exit on failure."""
    result = run_subprocess(
        args,
        cwd=cwd,
        check=False,
    )
    if result.returncode != 0:
        print_error(fail_message)
        raise typer.Exit(1)


def _get_pinned_submodule_hash(carts_dir: Path, submodule: str) -> Optional[str]:
    """Get the submodule commit pinned by the current CARTS HEAD."""
    result = run_subprocess(
        ["git", "ls-tree", "HEAD", submodule],
        cwd=carts_dir,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        return None
    parts = result.stdout.strip().split()
    if len(parts) < 3:
        return None
    return parts[2]


def _update_submodule_checkout(carts_dir: Path, submodule: str) -> None:
    """Update a top-level submodule and only the nested pieces required by v2."""
    recursive = submodule != "external/arts"
    args = ["git", "submodule", "update", "--init"]
    if recursive:
        args.append("--recursive")
    args.append(submodule)
    _run_git(
        carts_dir,
        args,
        f"Failed to update {submodule}",
    )

    if submodule != "external/arts":
        return

    arts_dir = carts_dir / submodule
    _run_git(
        arts_dir,
        ["git", "submodule", "sync", "--recursive"],
        "Failed to sync nested ARTS submodule metadata",
    )
    _run_git(
        arts_dir,
        ["git", "submodule", "update", "--init", *ARTS_REQUIRED_NESTED_SUBMODULES],
        "Failed to update required ARTS nested submodules",
    )


def update(
    arts: bool = typer.Option(
        False, "--arts", "-a", help="Update ARTS submodule"),
    polygeist: bool = typer.Option(
        False, "--polygeist", "-p", help="Update Polygeist submodule"),
    benchmarks: bool = typer.Option(
        False, "--benchmarks", "-b", help="Update benchmarks submodule"),
    force: bool = typer.Option(
        False, "--force", "-f", help="Discard local changes before updating"),
):
    """Update CARTS, submodules, and rebuild affected components."""
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

    # Pull latest CARTS repo changes first.
    carts_before_hash = _get_repo_hash(carts_dir)
    print_step("Pulling latest CARTS changes...")
    _run_git(
        carts_dir,
        ["git", "pull"],
        "Failed to pull CARTS repository",
    )
    carts_after_hash = _get_repo_hash(carts_dir)
    carts_changed = (
        carts_before_hash is not None
        and carts_after_hash is not None
        and carts_before_hash != carts_after_hash
    )

    # Keep submodule URLs in sync with .gitmodules before updating.
    print_step("Synchronizing submodule metadata...")
    _run_git(
        carts_dir,
        ["git", "submodule", "sync", "--recursive"],
        "Failed to sync submodule metadata",
    )

    before_hashes: Dict[str, Optional[str]] = {}
    for submodule in submodules:
        submodule_path = carts_dir / submodule
        before_hashes[submodule] = _get_submodule_hash(submodule_path)

    changed_submodules: List[str] = []
    for submodule in submodules:
        print_step(f"Updating {submodule} to CARTS-pinned commit...")
        _update_submodule_checkout(carts_dir, submodule)

        submodule_path = carts_dir / submodule
        after_hash = _get_submodule_hash(submodule_path)
        if after_hash is None:
            print_error(f"Could not read updated hash for {submodule}")
            raise typer.Exit(1)

        pinned_hash = _get_pinned_submodule_hash(carts_dir, submodule)
        if pinned_hash is None:
            print_error(f"Could not read pinned hash for {submodule}")
            raise typer.Exit(1)
        if after_hash != pinned_hash:
            print_error(
                f"{submodule} did not match pinned commit: expected {pinned_hash}, got {after_hash}"
            )
            raise typer.Exit(1)

        if before_hashes.get(submodule) != after_hash:
            changed_submodules.append(submodule)

    print_success(f"Updated {len(submodules)} submodule(s)")

    if changed_submodules:
        changed_list = ", ".join(changed_submodules)
        print_info(f"Changed submodules: {changed_list}")
    else:
        print_info("Submodule commits were already at pinned revisions")
    if carts_changed:
        print_info("CARTS repository commit changed")
    else:
        print_info("CARTS repository commit unchanged")

    # Build requested/updated components so binaries are always in sync
    # with the checked-out commits after update.
    polygeist_changed = "external/Polygeist" in changed_submodules
    arts_changed = "external/arts" in changed_submodules
    selected_polygeist = "external/Polygeist" in submodules
    selected_arts = "external/arts" in submodules

    if selected_polygeist and (polygeist_changed or force):
        print_step("Rebuilding Polygeist...")
        result = run_subprocess(
            ["make", "polygeist"],
            cwd=carts_dir,
            check=False,
        )
        if result.returncode != 0:
            print_error("Failed to rebuild Polygeist")
            raise typer.Exit(1)

    if selected_arts and (arts_changed or force):
        print_step("Rebuilding ARTS...")
        result = run_subprocess(
            ["make", "arts"],
            cwd=carts_dir,
            check=False,
        )
        if result.returncode != 0:
            print_error("Failed to rebuild ARTS")
            raise typer.Exit(1)

    rebuild_carts = force or carts_changed or arts_changed or polygeist_changed
    if rebuild_carts:
        print_step("Rebuilding CARTS...")
        result = run_subprocess(
            ["make", "build"],
            cwd=carts_dir,
            check=False,
        )
        if result.returncode != 0:
            print_error("Failed to rebuild CARTS")
            raise typer.Exit(1)

    print_success("Update complete")
