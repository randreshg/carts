#!/usr/bin/env python3
"""
CARTS Examples Runner

A CLI tool for running CARTS examples with rich terminal output.
Provides example discovery, execution, and result reporting.

Usage:
    carts examples --list                  # List all available examples
    carts examples <name>                  # Run a specific example
    carts examples --all                   # Run all examples
    carts examples --clean [name]          # Clean example artifacts
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import typer
from rich import box
from rich.console import Group
from rich.live import Live
from rich.panel import Panel
from rich.progress import (
    BarColumn,
    Progress,
    SpinnerColumn,
    TextColumn,
    TimeElapsedColumn,
)
from rich.table import Table
from rich.text import Text

# Import shared styles
try:
    from carts_styles import (
        Colors,
        Symbols,
        console,
        format_summary_line,
        print_debug,
        print_error,
        print_info,
        print_step,
        print_success,
        print_warning,
        status_symbol,
        VERSION,
    )
except ImportError:
    # Fallback if running standalone
    from rich.console import Console
    console = Console()
    VERSION = "0.1.0"

    class Colors:
        PASS = "green"
        FAIL = "red"
        DIM = "dim"

    class Symbols:
        PASS = "\u2713"
        FAIL = "\u2717"
        SKIP = "\u25cb"

    def print_step(m, **kw): console.print(f"-> {m}")
    def print_success(m): console.print(f"[green]OK[/green] {m}")
    def print_error(m): console.print(f"[red]ERROR[/red] {m}")
    def print_warning(m): console.print(f"[yellow]WARNING[/yellow] {m}")
    def print_info(m): console.print(f"[cyan]INFO[/cyan] {m}")
    def print_debug(m): console.print(f"[dim]DEBUG[/dim] {m}")
    def format_summary_line(p, f, s):
        return f"[green]{Symbols.PASS} {p}[/] passed  [red]{Symbols.FAIL} {f}[/] failed  [dim]{Symbols.SKIP} {s}[/] skipped"
    def status_symbol(s):
        if s in ("PASS", "pass"): return f"[green]{Symbols.PASS}[/]"
        elif s in ("FAIL", "fail"): return f"[red]{Symbols.FAIL}[/]"
        else: return f"[dim]{Symbols.SKIP}[/]"


# ============================================================================
# Constants
# ============================================================================

# Default runtime arguments for examples
EXAMPLE_ARGS: Dict[str, List[str]] = {
    "array": ["100"],
    "dotproduct": ["100"],
    "matrix": ["100"],
    "matrixmul": ["64", "16"],
    "rowdep": ["64"],
    "stencil": ["100"],
    "parallel_for/block": ["100"],
    "parallel_for/chunk": ["100"],
    "parallel_for/loops": ["100"],
    "parallel_for/reduction": ["100"],
    "parallel_for/single": ["100"],
    "parallel_for/stencil": ["100"],
}

# Default examples list (order matters for testing)
DEFAULT_EXAMPLES = [
    "array",
    "deps",
    "dotproduct",
    "jacobi",
    "matrix",
    "matrixmul",
    "parallel",
    "parallel_for/block",
    "parallel_for/chunk",
    "parallel_for/loops",
    "parallel_for/reduction",
    "parallel_for/single",
    "parallel_for/stencil",
    "rowdep",
    "smith-waterman",
    "stencil",
]

# Directories to skip when discovering examples
SKIP_DIRS = {".git", ".svn", ".hg", "build", "logs", "__pycache__", ".cache", ".claude"}

# File extensions for source files
SOURCE_EXTENSIONS = {".c", ".cpp"}


# ============================================================================
# Data Classes
# ============================================================================

@dataclass
class ExampleResult:
    """Result of running an example."""
    name: str
    build_status: str
    build_duration: float
    run_status: str
    run_duration: float
    exit_code: int
    output: str
    note: str
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


@dataclass
class ExampleInfo:
    """Information about an example."""
    name: str
    path: Path
    source_file: Path
    has_config: bool
    args: List[str]


# ============================================================================
# Examples Discovery
# ============================================================================

def get_examples_dir() -> Path:
    """Get the examples directory path."""
    # Try relative to this script first
    script_dir = Path(__file__).parent.resolve()

    # If in tools/, go up to carts root
    if script_dir.name == "tools":
        carts_dir = script_dir.parent
    else:
        carts_dir = script_dir

    examples_dir = carts_dir / "tests" / "examples"

    if not examples_dir.is_dir():
        # Try from environment
        carts_env = os.environ.get("CARTS_DIR")
        if carts_env:
            examples_dir = Path(carts_env) / "tests" / "examples"

    return examples_dir


def discover_examples(examples_dir: Path) -> List[ExampleInfo]:
    """Discover all available examples."""
    examples = []

    def scan_directory(base_dir: Path, prefix: str = "") -> None:
        if not base_dir.is_dir():
            return

        for item in sorted(base_dir.iterdir()):
            if item.name.startswith(".") or item.name in SKIP_DIRS:
                continue

            if item.is_dir():
                # Check if this directory has source files (is an example)
                source_files = list(item.glob("*.c")) + list(item.glob("*.cpp"))
                if source_files:
                    name = f"{prefix}{item.name}" if prefix else item.name
                    source_file = source_files[0]  # Take first source file
                    has_config = (item / "arts.cfg").is_file()
                    args = EXAMPLE_ARGS.get(name, [])

                    examples.append(ExampleInfo(
                        name=name,
                        path=item,
                        source_file=source_file,
                        has_config=has_config,
                        args=args,
                    ))
                else:
                    # Recurse into subdirectory
                    new_prefix = f"{prefix}{item.name}/" if prefix else f"{item.name}/"
                    scan_directory(item, new_prefix)

    scan_directory(examples_dir)
    return examples


def find_example(examples_dir: Path, name: str) -> Optional[ExampleInfo]:
    """Find a specific example by name."""
    examples = discover_examples(examples_dir)
    for ex in examples:
        if ex.name == name or ex.name.endswith(f"/{name}"):
            return ex
    return None


# ============================================================================
# Example Execution
# ============================================================================

def run_carts_execute(
    example: ExampleInfo,
    verbose: bool = False,
    timeout: int = 120,
) -> Tuple[bool, float, str]:
    """
    Run carts execute -O3 for an example.

    Returns: (success, duration_seconds, output)
    """
    start_time = time.time()

    # Find carts CLI
    script_dir = Path(__file__).parent.resolve()
    carts_cli = script_dir / "carts"

    if not carts_cli.is_file():
        return False, 0.0, "carts CLI not found"

    # Build command
    source_name = example.source_file.name
    cmd = [str(carts_cli), "execute", source_name, "-O3"]

    if verbose:
        print_debug(f"Running: {' '.join(cmd)} in {example.path}")

    try:
        result = subprocess.run(
            cmd,
            cwd=example.path,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        duration = time.time() - start_time
        output = result.stdout + result.stderr

        if result.returncode != 0:
            return False, duration, output

        return True, duration, output

    except subprocess.TimeoutExpired:
        return False, timeout, "Build timed out"
    except Exception as e:
        return False, time.time() - start_time, str(e)


def run_example_binary(
    example: ExampleInfo,
    verbose: bool = False,
    timeout: int = 60,
) -> Tuple[bool, float, int, str]:
    """
    Run the compiled example binary.

    Returns: (success, duration_seconds, exit_code, output)
    """
    start_time = time.time()

    # Find the executable
    base_name = example.source_file.stem
    executable = example.path / f"{base_name}_arts"

    if not executable.is_file():
        return False, 0.0, -1, f"Executable not found: {executable}"

    # Build command with args
    cmd = [str(executable)] + example.args

    if verbose:
        print_debug(f"Running: {' '.join(cmd)}")

    try:
        result = subprocess.run(
            cmd,
            cwd=example.path,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        duration = time.time() - start_time
        output = result.stdout + result.stderr

        return result.returncode == 0, duration, result.returncode, output

    except subprocess.TimeoutExpired:
        return False, timeout, -1, "Execution timed out"
    except Exception as e:
        return False, time.time() - start_time, -1, str(e)


def parse_output_status(output: str) -> Tuple[str, str]:
    """
    Parse example output to determine pass/fail status.

    Returns: (status, note)
    """
    # Check for CARTS test format
    if "[CARTS]" in output:
        if "FAIL" in output:
            # Find the CARTS line
            for line in output.split("\n"):
                if "[CARTS]" in line:
                    return "FAIL", line.strip()
            return "FAIL", "CARTS test failed"
        elif "PASS" in output:
            for line in output.split("\n"):
                if "[CARTS]" in line:
                    return "PASS", line.strip()
            return "PASS", "CARTS test passed"

    # Check for legacy INCORRECT format
    if "INCORRECT" in output:
        return "FAIL", "reported incorrect result"

    # Default to pass with first line as note
    first_line = output.split("\n")[0].strip() if output else ""
    return "PASS", first_line[:60] if first_line else ""


def run_single_example(
    example: ExampleInfo,
    verbose: bool = False,
    build_timeout: int = 120,
    run_timeout: int = 60,
) -> ExampleResult:
    """Run a single example: build and execute."""
    # Step 1: Build
    build_ok, build_duration, build_output = run_carts_execute(
        example, verbose=verbose, timeout=build_timeout
    )

    if not build_ok:
        return ExampleResult(
            name=example.name,
            build_status="FAIL",
            build_duration=build_duration,
            run_status="SKIP",
            run_duration=0.0,
            exit_code=-1,
            output=build_output,
            note="carts execute failed",
        )

    # Step 2: Run
    run_ok, run_duration, exit_code, run_output = run_example_binary(
        example, verbose=verbose, timeout=run_timeout
    )

    if not run_ok:
        if exit_code == -1 and "timed out" in run_output.lower():
            return ExampleResult(
                name=example.name,
                build_status="PASS",
                build_duration=build_duration,
                run_status="TIMEOUT",
                run_duration=run_duration,
                exit_code=exit_code,
                output=run_output,
                note="execution timed out",
            )
        return ExampleResult(
            name=example.name,
            build_status="PASS",
            build_duration=build_duration,
            run_status="FAIL",
            run_duration=run_duration,
            exit_code=exit_code,
            output=run_output,
            note=f"runtime error (rc={exit_code})",
        )

    # Parse output for status
    status, note = parse_output_status(run_output)

    return ExampleResult(
        name=example.name,
        build_status="PASS",
        build_duration=build_duration,
        run_status=status,
        run_duration=run_duration,
        exit_code=exit_code,
        output=run_output,
        note=note,
    )


# ============================================================================
# Live Display Helpers
# ============================================================================


def create_live_table(
    examples: List[ExampleInfo],
    results: Dict[str, ExampleResult],
    in_progress: Optional[str] = None,
) -> Table:
    """Create a live-updating table showing example progress."""
    table = Table(box=box.ROUNDED, show_header=True, header_style="bold")
    table.add_column("Example", style="cyan", width=28)
    table.add_column("Build", justify="center", width=12)
    table.add_column("Run", justify="center", width=12)
    table.add_column("Note", width=40)

    for ex in examples:
        name = ex.name
        if name in results:
            # Completed - show full results
            r = results[name]
            build_col = f"{status_symbol(r.build_status)} {r.build_duration:.1f}s"
            if r.run_status == "PASS":
                run_col = f"{status_symbol(r.run_status)} {r.run_duration:.2f}s"
            else:
                run_col = f"{status_symbol(r.run_status)} {r.run_status}"
            note_col = r.note[:40] if r.note else ""
            table.add_row(name, build_col, run_col, note_col)
        elif name == in_progress:
            # Currently running
            table.add_row(f"[bold]{name}[/]", "[yellow]\u23f3...[/]", "[dim]-[/]", "[dim]-[/]")
        else:
            # Pending
            table.add_row(f"[dim]{name}[/]", "[dim]-[/]", "[dim]-[/]", "[dim]-[/]")

    return table


def create_live_summary(
    results: Dict[str, ExampleResult],
    total: int,
    elapsed: float,
) -> Text:
    """Create a one-line summary for live display."""
    passed = sum(1 for r in results.values() if r.run_status == "PASS")
    failed = sum(1 for r in results.values() if r.run_status in ("FAIL", "TIMEOUT"))
    pending = total - len(results)

    text = Text()
    text.append(f"\u2713 {passed} passed  ", style="green")
    text.append(f"\u2717 {failed} failed  ", style="red")
    text.append(f"\u25cb {pending} pending  ", style="dim")
    text.append(f"\u23f1 {elapsed:.1f}s", style="dim")
    return text


def create_live_display(
    examples: List[ExampleInfo],
    results: Dict[str, ExampleResult],
    in_progress: Optional[str],
    elapsed: float,
) -> Group:
    """Create the complete live display (table + summary)."""
    table = create_live_table(examples, results, in_progress)
    summary = create_live_summary(results, len(examples), elapsed)
    return Group(table, summary)


def create_summary_panel(results: List[ExampleResult], duration: float) -> Panel:
    """Create a summary panel."""
    passed = sum(1 for r in results if r.run_status == "PASS")
    failed = sum(1 for r in results if r.run_status in ("FAIL", "TIMEOUT", "CRASH"))
    skipped = sum(1 for r in results if r.run_status == "SKIP")

    content = (
        f"[green]\u2713 {passed}[/] passed  "
        f"[red]\u2717 {failed}[/] failed  "
        f"[dim]\u25cb {skipped}[/] skipped  "
        f"[cyan]\u23f1 {duration:.1f}s[/]"
    )
    return Panel(content, title="Summary", border_style="blue")


# ============================================================================
# CLI Application
# ============================================================================

app = typer.Typer(
    name="examples",
    help="CARTS Examples Runner - Run and test CARTS examples",
    add_completion=False,
    no_args_is_help=True,
)


@app.command()
def list_examples(
    verbose: bool = typer.Option(False, "--verbose", "-v", help="Show detailed info"),
):
    """List all available CARTS examples."""
    examples_dir = get_examples_dir()

    if not examples_dir.is_dir():
        print_error(f"Examples directory not found: {examples_dir}")
        raise typer.Exit(1)

    examples = discover_examples(examples_dir)

    if not examples:
        print_warning("No examples found")
        raise typer.Exit(0)

    console.print(f"\n[bold]Available CARTS Examples[/bold] ({len(examples)} total)\n")

    # Group by parent directory
    grouped: Dict[str, List[ExampleInfo]] = {}
    for ex in examples:
        if "/" in ex.name:
            group = ex.name.rsplit("/", 1)[0]
        else:
            group = ""
        grouped.setdefault(group, []).append(ex)

    # Print ungrouped first
    if "" in grouped:
        for ex in grouped[""]:
            if verbose:
                args_str = " ".join(ex.args) if ex.args else "(none)"
                console.print(f"  {ex.name:<30} args: {args_str}")
            else:
                console.print(f"  {ex.name}")
        console.print()

    # Print grouped
    for group in sorted(k for k in grouped.keys() if k):
        console.print(f"[cyan]{group}:[/cyan]")
        for ex in grouped[group]:
            short_name = ex.name.split("/")[-1]
            if verbose:
                args_str = " ".join(ex.args) if ex.args else "(none)"
                console.print(f"  {ex.name:<30} args: {args_str}")
            else:
                console.print(f"  {ex.name}")
        console.print()


@app.command()
def run(
    name: Optional[str] = typer.Argument(None, help="Example name to run"),
    all_examples: bool = typer.Option(False, "--all", "-a", help="Run all examples"),
    verbose: bool = typer.Option(False, "--verbose", "-v", help="Verbose output"),
    json_output: Optional[Path] = typer.Option(None, "--json", "-j", help="Export results to JSON"),
    build_timeout: int = typer.Option(120, "--build-timeout", help="Build timeout in seconds"),
    run_timeout: int = typer.Option(60, "--run-timeout", help="Run timeout in seconds"),
):
    """Run CARTS example(s)."""
    examples_dir = get_examples_dir()

    if not examples_dir.is_dir():
        print_error(f"Examples directory not found: {examples_dir}")
        raise typer.Exit(1)

    if all_examples:
        # Run all examples
        examples = discover_examples(examples_dir)
        # Filter to default list if available
        default_set = set(DEFAULT_EXAMPLES)
        examples = [ex for ex in examples if ex.name in default_set]
        if not examples:
            examples = discover_examples(examples_dir)
    elif name:
        # Run specific example
        example = find_example(examples_dir, name)
        if not example:
            print_error(f"Example not found: {name}")
            print_info("Use 'carts examples --list' to see available examples")
            raise typer.Exit(1)
        examples = [example]
    else:
        print_error("Specify an example name or use --all")
        raise typer.Exit(1)

    if not examples:
        print_warning("No examples to run")
        raise typer.Exit(0)

    # Run examples
    results: List[ExampleResult] = []
    results_dict: Dict[str, ExampleResult] = {}
    passed = 0
    failed = 0

    # Print header (matching benchmark runner style)
    console.print(f"\n[bold]CARTS Examples Runner v{VERSION}[/]")
    console.print("\u2501" * 30)
    console.print(f"Examples: {len(examples)}\n")

    # Run with live-updating table
    start_time = time.time()

    with Live(
        create_live_display(examples, results_dict, None, 0),
        console=console,
        refresh_per_second=4,
    ) as live:
        for example in examples:
            # Update display to show current example as in-progress
            elapsed = time.time() - start_time
            live.update(create_live_display(examples, results_dict, example.name, elapsed))

            # Run example
            result = run_single_example(
                example,
                verbose=verbose,
                build_timeout=build_timeout,
                run_timeout=run_timeout,
            )

            # Update results
            results_dict[example.name] = result
            results.append(result)

            # Count results
            if result.run_status == "PASS":
                passed += 1
            else:
                failed += 1

            # Refresh display with completed result
            elapsed = time.time() - start_time
            live.update(create_live_display(examples, results_dict, None, elapsed))

    # Final summary panel
    total_duration = time.time() - start_time
    console.print()
    console.print(create_summary_panel(results, total_duration))

    # Export JSON if requested
    if json_output:
        total = len(results)
        pass_rate = passed / total if total > 0 else 0.0
        export_data = {
            "metadata": {
                "timestamp": datetime.now().isoformat(),
                "total": total,
                "passed": passed,
                "failed": failed,
                "pass_rate": pass_rate,
            },
            "results": [r.to_dict() for r in results],
        }
        with open(json_output, "w") as f:
            json.dump(export_data, f, indent=2)
        print_success(f"Results exported to {json_output}")

    raise typer.Exit(0 if failed == 0 else 1)


@app.command()
def clean(
    name: Optional[str] = typer.Argument(None, help="Example name to clean"),
    all_examples: bool = typer.Option(False, "--all", "-a", help="Clean all examples"),
    verbose: bool = typer.Option(False, "--verbose", "-v", help="Verbose output"),
):
    """Clean example build artifacts."""
    examples_dir = get_examples_dir()

    if not examples_dir.is_dir():
        print_error(f"Examples directory not found: {examples_dir}")
        raise typer.Exit(1)

    if all_examples:
        examples = discover_examples(examples_dir)
    elif name:
        example = find_example(examples_dir, name)
        if not example:
            print_error(f"Example not found: {name}")
            raise typer.Exit(1)
        examples = [example]
    else:
        print_error("Specify an example name or use --all")
        raise typer.Exit(1)

    # Patterns to clean
    clean_patterns = [
        "*.mlir", "*.ll", "*.o", "*_arts", "*_omp",
        ".carts-metadata.json", ".artsPrintLock",
    ]
    clean_dirs = ["build", "counter", "counters", "logs"]

    cleaned = 0

    for example in examples:
        if verbose:
            print_info(f"Cleaning {example.name}")

        # Clean files
        for pattern in clean_patterns:
            for f in example.path.glob(pattern):
                if f.is_file():
                    if verbose:
                        print_debug(f"Removing {f}")
                    f.unlink()
                    cleaned += 1

        # Clean directories
        for dirname in clean_dirs:
            d = example.path / dirname
            if d.is_dir():
                if verbose:
                    print_debug(f"Removing directory {d}")
                shutil.rmtree(d)
                cleaned += 1

    if cleaned > 0:
        print_success(f"Cleaned {cleaned} files/directories from {len(examples)} example(s)")
    else:
        print_info("Nothing to clean")


# ============================================================================
# Entry Point
# ============================================================================

def main():
    """Main entry point."""
    app()


if __name__ == "__main__":
    main()
