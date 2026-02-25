#!/usr/bin/env python3
"""
CARTS CLI - Compiler for Asynchronous Runtime Systems
A modern Python CLI with rich terminal output.
"""

from pathlib import Path
from typing import List, Optional
import os
import sys

import typer

# Import from scripts modules
from scripts.config import (
    get_config,
    set_verbose,
    is_verbose,
)
from scripts.run import run_subprocess
from scripts.build import build as build_cmd
from scripts.compile import (
    compile_cmd,
    cgeist as cgeist_cmd,
    clang as clang_cmd,
    mlir_translate as mlir_translate_cmd,
)
from scripts.docker import (
    docker_callback,
    docker_build,
    docker_start,
    docker_update,
    docker_stop,
    docker_clean as docker_clean_cmd,
    docker_commit,
    docker_status,
    docker_exec,
)
from scripts.test import test as test_cmd, check as check_cmd
from scripts.format import format_sources as format_cmd
from scripts.clean import run_local_clean, run_full_clean
from scripts.update import update as update_cmd
from scripts.examples import examples_app
from scripts.install import install as install_cmd


# ============================================================================
# CLI Application
# ============================================================================

app = typer.Typer(
    name="carts",
    help="CARTS - Compiler for Asynchronous Runtime Systems",
    add_completion=False,
    no_args_is_help=True,
)

# Docker subcommand group
docker_app = typer.Typer(help="Docker operations for multi-node execution")
app.add_typer(docker_app, name="docker")

# Examples subcommand group
app.add_typer(examples_app, name="examples")


# ============================================================================
# Command Registration
# ============================================================================

# Build
app.command()(build_cmd)

# Compile pipeline
app.command(
    name="compile",
    context_settings={
        "allow_extra_args": True,
        "allow_interspersed_args": False,
        "ignore_unknown_options": True,
    },
)(compile_cmd)

app.command(
    context_settings={"allow_extra_args": True, "allow_interspersed_args": False}
)(cgeist_cmd)

app.command(
    context_settings={"allow_extra_args": True, "allow_interspersed_args": False}
)(clang_cmd)

app.command(name="mlir-translate")(mlir_translate_cmd)

# Testing
app.command(name="test")(test_cmd)
app.command(name="check")(check_cmd)

# Formatting
app.command(name="format")(format_cmd)

# Install (deps → submodules → build)
app.command(name="install")(install_cmd)

# Update submodules
app.command(name="update")(update_cmd)

# Docker subcommands
docker_app.callback(invoke_without_command=True)(docker_callback)
docker_app.command(name="build")(docker_build)
docker_app.command(name="start")(docker_start)
docker_app.command(name="update")(docker_update)
docker_app.command(name="stop")(docker_stop)
docker_app.command(name="clean")(docker_clean_cmd)
docker_app.command(name="commit")(docker_commit)
docker_app.command(name="status")(docker_status)
docker_app.command(
    name="exec",
    context_settings={"allow_extra_args": True, "allow_interspersed_args": False},
)(docker_exec)


# ============================================================================
# Command Callbacks
# ============================================================================

@app.callback(invoke_without_command=True)
def main_callback(
    ctx: typer.Context,
    verbose: bool = typer.Option(
        False, "--verbose", "-v", help="Enable verbose output"),
    help_flag: bool = typer.Option(
        False, "--help", "-h", is_eager=True, help="Show help and exit"),
):
    """CARTS - Compiler for Asynchronous Runtime Systems"""
    # Handle -h/--help globally
    if help_flag:
        if ctx.invoked_subcommand is None:
            typer.echo(ctx.get_help())
            raise typer.Exit()

    set_verbose(verbose)
    if verbose:
        os.environ["CARTS_VERBOSE"] = "1"


# ============================================================================
# Benchmarks Command
# ============================================================================

@app.command(
    context_settings={
        "allow_extra_args": True,
        "allow_interspersed_args": False,
        "ignore_unknown_options": True,
    }
)
def benchmarks(
    ctx: typer.Context,
    help_flag: bool = typer.Option(
        False, "--help", "-h", is_eager=True, help="Show help"),
):
    """Build and manage CARTS benchmarks.

    Commands: list, run, build, clean

    Examples:
      carts benchmarks list
      carts benchmarks run polybench/2mm --size small
      carts benchmarks build --suite polybench
      carts benchmarks clean --all
    """
    config = get_config()
    benchmark_runner = (
        config.carts_dir / "external" / "carts-benchmarks"
        / "scripts" / "benchmark_runner.py"
    )

    if not benchmark_runner.is_file():
        from carts_styles import print_error
        print_error(f"Benchmark runner not found at {benchmark_runner}")
        raise typer.Exit(1)

    # Use current Python interpreter (already in poetry env) for faster startup
    cmd = [sys.executable, str(benchmark_runner)]

    # Pass --help to the benchmark runner to show its commands
    if help_flag:
        cmd.append("--help")

    # Pass through all extra args (e.g., run, --verbose, --size, etc.)
    if ctx.args:
        cmd.extend(ctx.args)

    # Inject PYTHONPATH so benchmarks can import carts_styles
    tools_dir = str(Path(__file__).parent)
    existing_pythonpath = os.environ.get("PYTHONPATH", "")
    pythonpath = f"{tools_dir}:{existing_pythonpath}" if existing_pythonpath else tools_dir

    result = run_subprocess(cmd, check=False, env={"PYTHONPATH": pythonpath})
    raise typer.Exit(result.returncode)


# Clean Command
# ============================================================================

@app.command()
def clean(
    all_builds: bool = typer.Option(
        False, "--all", "-a", help="Clean all build directories (LLVM, Polygeist, ARTS, CARTS)"),
    docker_clean: bool = typer.Option(
        False, "--docker", "-d", help="Clean Docker artifacts"),
):
    """Clean generated files in current directory."""
    if all_builds:
        run_full_clean()
    elif docker_clean:
        docker_clean_cmd()
    else:
        run_local_clean()


# ============================================================================
# Entry Point
# ============================================================================

def main():
    """Main entry point."""
    # Set LLVM_SYMBOLIZER_PATH for better stack traces
    config = get_config()
    symbolizer = config.get_llvm_tool("llvm-symbolizer")
    if symbolizer.is_file():
        os.environ["LLVM_SYMBOLIZER_PATH"] = str(symbolizer)

    app()


if __name__ == "__main__":
    main()
