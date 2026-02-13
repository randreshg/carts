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
import sys
from pathlib import Path

# Add tools directory to path for imports
tools_dir = Path(__file__).parent.resolve()
if str(tools_dir) not in sys.path:
    sys.path.insert(0, str(tools_dir))


# ============================================================================
# Constants
# ============================================================================

# Directories to skip when discovering examples
SKIP_DIRS = {".git", ".svn", ".hg", "build",
             "logs", "__pycache__", ".cache", ".claude"}

# File extensions for source files
SOURCE_EXTENSIONS = {".c", ".cpp"}

# Default ARTS config (in examples directory)
DEFAULT_ARTS_CONFIG = Path(__file__).parent.parent / "tests" / "examples" / "arts.cfg"


# ============================================================================
# ARTS Config Parsing Functions
# ============================================================================

def parse_arts_cfg(path: Optional[Path]) -> Dict[str, str]:
    """Parse an ARTS config file (arts.cfg) into a key/value dict.

    This parser is intentionally simple: it ignores section headers and
    supports `key=value` lines, stripping whitespace and inline comments.
    """
    if not path or not path.exists():
        return {}

    values: Dict[str, str] = {}
    try:
        for raw in path.read_text().splitlines():
            line = raw.strip()
            if not line:
                continue
            if line.startswith("#") or line.startswith(";"):
                continue
            if line.startswith("[") and line.endswith("]"):
                continue
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            key = key.strip()
            # Strip common inline comments.
            value = value.split("#", 1)[0].split(";", 1)[0].strip()
            if key:
                values[key] = value
    except Exception:
        return {}
    return values


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
                source_files = list(item.glob("*.c")) + \
                    list(item.glob("*.cpp"))
                if source_files:
                    name = f"{prefix}{item.name}" if prefix else item.name
                    source_file = source_files[0]  # Take first source file
                    has_config = (item / "arts.cfg").is_file()
                    # Examples now handle default arguments internally
                    args = []

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

# Patterns to clean before running examples
CLEAN_PATTERNS = [
    "*.mlir", "*.ll", "*.o", "*_arts", "*_omp",
    ".carts-metadata.json", ".artsPrintLock",
]
CLEAN_DIRS = ["build", "counter", "counters", "logs"]


def clean_example(example: ExampleInfo, verbose: bool = False) -> int:
    """Clean build artifacts from an example directory.

    Returns: number of files/directories removed
    """
    cleaned = 0

    # Clean files
    for pattern in CLEAN_PATTERNS:
        for f in example.path.glob(pattern):
            if f.is_file():
                if verbose:
                    print_debug(f"Removing {f}")
                f.unlink()
                cleaned += 1

    # Clean directories
    for dirname in CLEAN_DIRS:
        d = example.path / dirname
        if d.is_dir():
            if verbose:
                print_debug(f"Removing directory {d}")
            shutil.rmtree(d)
            cleaned += 1

    return cleaned


def run_carts_execute(
    example: ExampleInfo,
    verbose: bool = False,
    timeout: int = 120,
    config_file: Optional[Path] = None,
) -> Tuple[bool, float, str]:
    """
    Run carts compile -O3 for an example.

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
    cmd = [str(carts_cli), "compile", source_name, "-O3"]

    # Pass config file to carts-compile for compile-time abstract machine config
    if config_file:
        config_path = config_file.resolve()
        if config_path.is_file():
            cmd.extend(["--arts-config", str(config_path)])
            if verbose:
                print_debug(f"Using config file for build: {config_path}")

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
    config_file: Optional[Path] = None,
    trace: bool = False,
    runtime_args: Optional[List[str]] = None,
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

    # Build command with args - use runtime_args if provided, otherwise empty (defaults in code)
    args = runtime_args if runtime_args is not None else []
    cmd = [str(executable)] + args

    # Set up environment with config file if provided
    env = os.environ.copy()
    if config_file:
        # Resolve to absolute path
        config_path = config_file.resolve()
        if not config_path.is_file():
            return False, 0.0, -1, f"Config file not found: {config_path}"
        env["artsConfig"] = str(config_path)
        if verbose:
            print_debug(f"Using config file: {config_path}")

    if verbose:
        print_debug(f"Running: {' '.join(cmd)}")

    try:
        # Use Popen to capture partial output on timeout
        process = subprocess.Popen(
            cmd,
            cwd=example.path,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
        )

        try:
            stdout, stderr = process.communicate(timeout=timeout)
            duration = time.time() - start_time
            output = stdout + stderr

            # Print output to terminal if trace is enabled
            if trace and output:
                console.print(f"\n[dim]{'─' * 60}[/dim]")
                console.print(f"[bold cyan]Output: {executable.name}[/bold cyan]")
                console.print(f"[dim]{'─' * 60}[/dim]")
                console.print(output.strip())
                console.print(f"[dim]{'─' * 60}[/dim]\n")

            return process.returncode == 0, duration, process.returncode, output

        except subprocess.TimeoutExpired:
            process.kill()
            # Capture any partial output that was produced before timeout
            stdout, stderr = process.communicate()
            duration = time.time() - start_time
            partial_output = stdout + stderr
            timeout_msg = "Execution timed out"
            if partial_output.strip():
                timeout_msg += f"\n\nPartial output:\n{partial_output}"
            return False, duration, -1, timeout_msg

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
    config_file: Optional[Path] = None,
    trace: bool = False,
    runtime_args: Optional[List[str]] = None,
) -> ExampleResult:
    """Run a single example: clean, build and execute."""
    # Step 0: Clean previous build artifacts
    clean_example(example, verbose=verbose)

    # Step 1: Build
    build_ok, build_duration, build_output = run_carts_execute(
        example, verbose=verbose, timeout=build_timeout, config_file=config_file
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
            note="carts compile failed",
        )

    # Step 2: Run
    run_ok, run_duration, exit_code, run_output = run_example_binary(
        example, verbose=verbose, timeout=run_timeout, config_file=config_file, trace=trace,
        runtime_args=runtime_args or []
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
            table.add_row(
                f"[bold]{name}[/]", "[yellow]\u23f3...[/]", "[dim]-[/]", "[dim]-[/]")
        else:
            # Pending
            table.add_row(f"[dim]{name}[/]", "[dim]-[/]",
                          "[dim]-[/]", "[dim]-[/]")

    return table


def create_live_summary(
    results: Dict[str, ExampleResult],
    total: int,
    elapsed: float,
) -> Text:
    """Create a one-line summary for live display."""
    passed = sum(1 for r in results.values() if r.run_status == "PASS")
    failed = sum(1 for r in results.values()
                 if r.run_status in ("FAIL", "TIMEOUT"))
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
    failed = sum(1 for r in results if r.run_status in (
        "FAIL", "TIMEOUT", "CRASH"))
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


@app.command(name="list")
def list_examples(
    verbose: bool = typer.Option(
        False, "--verbose", "-v", help="Show detailed info"),
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

    console.print(
        f"\n[bold]Available CARTS Examples[/bold] ({len(examples)} total)\n")

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


@app.command(
    context_settings={
        "allow_extra_args": True,
        "allow_interspersed_args": False,
    }
)
def run(
    ctx: typer.Context,
    name: Optional[str] = typer.Argument(None, help="Example name to run"),
    all_examples: bool = typer.Option(
        False, "--all", "-a", help="Run all examples"),
    verbose: bool = typer.Option(
        False, "--verbose", "-v", help="Verbose output"),
    json_output: Optional[Path] = typer.Option(
        None, "--json", "-j", help="Export results to JSON"),
    build_timeout: int = typer.Option(
        120, "--build-timeout", help="Build timeout in seconds"),
    run_timeout: int = typer.Option(
        60, "--run-timeout", help="Run timeout in seconds"),
    arts_config: Optional[Path] = typer.Option(
        None, "--arts-config", help="ARTS configuration file to use when running examples"),
    trace: bool = typer.Option(
        False, "--trace", "-t", help="Print example output to terminal"),
):
    """Run CARTS example(s).

    Examples:
        carts examples run array              # Run with default size
        carts examples run array 100          # Run with size 100
        carts examples run matrixmul 64 16     # Run with custom sizes
        carts examples run --all              # Run all examples with defaults
    """
    examples_dir = get_examples_dir()

    if not examples_dir.is_dir():
        print_error(f"Examples directory not found: {examples_dir}")
        raise typer.Exit(1)

    if all_examples:
        # Run all discovered examples (no hardcoded filtering)
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

    # Extract --arts-config from ctx.args if present (due to allow_extra_args=True)
    # and separate runtime arguments for the executable
    runtime_args: List[str] = []
    if ctx.args:
        args_copy = list(ctx.args)
        i = 0
        while i < len(args_copy):
            if args_copy[i] == '--arts-config' and i + 1 < len(args_copy):
                arts_config = Path(args_copy[i + 1])
                args_copy.pop(i)  # remove --arts-config
                args_copy.pop(i)  # remove the path
            else:
                i += 1
        
        # Remaining args are runtime arguments for the executable (only for single examples)
        if not all_examples and name:
            runtime_args = args_copy

    # Run examples
    results: List[ExampleResult] = []
    results_dict: Dict[str, ExampleResult] = {}
    passed = 0
    failed = 0

    # Print header (matching benchmark runner style)
    console.print(f"\n[bold]CARTS Examples Runner v{VERSION}[/]")
    console.print("\u2501" * 30)

    # Show effective ARTS configuration
    if examples:
        # Multiple examples without custom config: each uses its own local config
        if len(examples) > 1 and not arts_config:
            console.print("ARTS Config: using local")
        else:
            # Single example or custom config: show specific values
            sample_example = examples[0]

            # Determine effective config file and source in one pass
            if arts_config:
                effective_config = arts_config
                config_source = "custom"
            elif (sample_example.path / "arts.cfg").exists():
                effective_config = sample_example.path / "arts.cfg"
                config_source = "local"
            else:
                effective_config = DEFAULT_ARTS_CONFIG
                config_source = "default"

            # Parse ARTS config values once
            cfg = parse_arts_cfg(effective_config)
            arts_threads = int(cfg.get("threads", "1"))
            arts_nodes = int(cfg.get("nodeCount", "1"))
            arts_launcher = cfg.get("launcher", "ssh")
            arts_nodes_list = [n.strip() for n in cfg.get("nodes", "localhost").split(",") if n.strip()]

            # Build config display items efficiently
            arts_config_items = [f"arts-threads={arts_threads}",
                                f"arts-nodes={arts_nodes}",
                                f"arts-launcher={arts_launcher}"]

            # Only show node list if it's not just localhost
            if arts_nodes_list != ["localhost"]:
                node_display = ','.join(arts_nodes_list[:3])
                if len(arts_nodes_list) > 3:
                    node_display += "..."
                arts_config_items.append(f"arts-node-list=[{node_display}]")

            console.print(f"ARTS Config ({config_source}): {', '.join(arts_config_items)}")

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
            live.update(create_live_display(
                examples, results_dict, example.name, elapsed))

            # Use runtime args only when running a single example (not --all)
            # When --all is used, runtime_args is empty, so all examples use defaults
            example_runtime_args = runtime_args if not all_examples else []

            # Run example
            result = run_single_example(
                example,
                verbose=verbose,
                build_timeout=build_timeout,
                run_timeout=run_timeout,
                config_file=arts_config,
                trace=trace,
                runtime_args=example_runtime_args,
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
            live.update(create_live_display(
                examples, results_dict, None, elapsed))

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
    all_examples: bool = typer.Option(
        False, "--all", "-a", help="Clean all examples"),
    verbose: bool = typer.Option(
        False, "--verbose", "-v", help="Verbose output"),
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

    cleaned = 0

    for example in examples:
        if verbose:
            print_info(f"Cleaning {example.name}")
        cleaned += clean_example(example, verbose=verbose)

    if cleaned > 0:
        print_success(
            f"Cleaned {cleaned} files/directories from {len(examples)} example(s)")
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
