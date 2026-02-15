"""Docker subcommands for CARTS CLI."""

from pathlib import Path
from typing import List, Optional, Tuple

import typer

from carts_styles import print_error, print_info
from scripts.config import get_config
from scripts.run import run_subprocess


def _get_docker_script(name: str) -> Tuple[Path, Path]:
    """Get docker script path, raising error if not found.

    Returns (docker_dir, script_path) tuple for use with run_subprocess.
    """
    config = get_config()
    docker_dir = config.carts_dir / "docker"

    if not docker_dir.is_dir():
        print_error(f"Docker directory not found at {docker_dir}")
        raise typer.Exit(1)

    script = docker_dir / f"docker-{name}.sh"
    if not script.is_file():
        print_error(f"docker-{name}.sh not found in {docker_dir}")
        raise typer.Exit(1)

    return docker_dir, script


def _run_docker_clean() -> None:
    """Clean Docker containers and images."""
    config = get_config()
    docker_dir = config.carts_dir / "docker"

    if not docker_dir.is_dir():
        print_error(f"Docker directory not found at {docker_dir}")
        raise typer.Exit(1)

    clean_script = docker_dir / "docker-clean.sh"
    if not clean_script.is_file():
        print_error(f"docker-clean.sh not found in {docker_dir}")
        raise typer.Exit(1)

    print_info("Cleaning Docker multi-node setup...")
    run_subprocess(["./docker-clean.sh"], cwd=docker_dir, check=False)


def docker_callback(ctx: typer.Context):
    """Docker operations for multi-node execution."""
    if ctx.invoked_subcommand is None:
        from carts_styles import console
        console.print(
            "Docker command requires a subcommand. Use --help for options.")
        raise typer.Exit(1)


def docker_run(
    file: Optional[Path] = typer.Argument(None, help="File to run in Docker"),
    args: Optional[List[str]] = typer.Argument(None, help="Runtime arguments"),
    slurm: bool = typer.Option(
        False, "--slurm", help="Use Slurm launcher instead of SSH"),
):
    """Run example in Docker containers."""
    docker_dir, _ = _get_docker_script("run")

    cmd = ["./docker-run.sh"]
    if slurm:
        cmd.append("--slurm")
    if file:
        cmd.append(str(file))
    if args:
        cmd.extend(args)

    result = run_subprocess(cmd, cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


def docker_build(
    force: bool = typer.Option(
        False, "--force", "-f", help="Force rebuild from scratch"),
):
    """Build Docker image and workspace."""
    docker_dir, _ = _get_docker_script("build")

    cmd = ["./docker-build.sh"]
    if force:
        cmd.append("--force")

    result = run_subprocess(cmd, cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


def docker_update(
    arts: bool = typer.Option(False, "--arts", "-a", help="Rebuild ARTS"),
    polygeist: bool = typer.Option(
        False, "--polygeist", "-p", help="Rebuild Polygeist"),
    llvm: bool = typer.Option(False, "--llvm", "-l", help="Rebuild LLVM"),
    carts_rebuild: bool = typer.Option(
        False, "--carts", "-c", help="Rebuild CARTS"),
    force: bool = typer.Option(False, "--force", "-f", help="Force rebuild"),
    debug_mode: bool = typer.Option(
        False, "--debug", help="Build with debug logging"),
    info_mode: bool = typer.Option(
        False, "--info", help="Build with info logging"),
):
    """Update Docker containers."""
    docker_dir, _ = _get_docker_script("update")

    cmd = ["./docker-update.sh"]
    if force:
        cmd.append("--force")
    if arts:
        if debug_mode:
            cmd.append("--arts=debug")
        elif info_mode:
            cmd.append("--arts=info")
        else:
            cmd.append("--arts")
    if polygeist:
        cmd.append("--polygeist")
    if llvm:
        cmd.append("--llvm")
    if carts_rebuild:
        cmd.append("--carts")

    result = run_subprocess(cmd, cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


def docker_kill():
    """Kill all processes in running ARTS containers."""
    docker_dir, _ = _get_docker_script("kill")
    print_info("Killing all processes in ARTS containers...")
    result = run_subprocess(["./docker-kill.sh"], cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


def docker_clean_cmd():
    """Clean Docker containers and images."""
    _run_docker_clean()
