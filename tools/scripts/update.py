"""Update command logic for CARTS CLI."""

from typing import List

import typer

from carts_styles import print_info, print_success, print_error
from scripts.config import get_config
from scripts.run import run_subprocess


def update(
    arts: bool = typer.Option(
        False, "--arts", "-a", help="Update ARTS submodule"),
    polygeist: bool = typer.Option(
        False, "--polygeist", "-p", help="Update Polygeist submodule"),
    benchmarks: bool = typer.Option(
        False, "--benchmarks", "-b", help="Update benchmarks submodule"),
    init: bool = typer.Option(
        False, "--init", help="Initialize submodules if not present"),
):
    """Update git submodules to latest remote commits."""
    config = get_config()
    carts_dir = config.carts_dir

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

    # If no specific flags, update all
    update_all = not (arts or polygeist or benchmarks)

    submodules: List[str] = []
    if update_all or arts:
        submodules.append("external/arts")
    if update_all or polygeist:
        submodules.append("external/Polygeist")
    if update_all or benchmarks:
        submodules.append("external/carts-benchmarks")

    base_cmd = ["git", "submodule", "update", "--remote"]
    if init:
        base_cmd.extend(["--init", "--recursive"])

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

    print_success(f"Updated {len(submodules)} submodule(s)")
