#!/usr/bin/env python3
"""
CARTS Shared Styles - Common Rich console styling for all CARTS CLI tools.

This module provides consistent styling, colors, and output helpers
across the carts CLI, benchmark runner, and examples runner.
"""

from enum import Enum
from typing import Optional

from rich import box
from rich.console import Console
from rich.panel import Panel
from rich.progress import (
    BarColumn,
    Progress,
    SpinnerColumn,
    TaskProgressColumn,
    TextColumn,
    TimeElapsedColumn,
)
from rich.table import Table
from rich.text import Text

# ============================================================================
# Global Console
# ============================================================================

console = Console()

# ============================================================================
# Color Scheme
# ============================================================================

class Colors:
    """CARTS color scheme for consistent styling."""
    # Status colors
    SUCCESS = "bold green"
    ERROR = "bold red"
    WARNING = "bold yellow"
    INFO = "cyan"
    DEBUG = "dim"

    # UI colors
    HEADER = "bold cyan"
    STEP = "bold blue"
    DIM = "dim"
    HIGHLIGHT = "bold white"

    # Status-specific
    PASS = "green"
    FAIL = "red"
    SKIP = "yellow"
    PENDING = "dim"
    RUNNING = "blue"


# ============================================================================
# Status Enum (shared across tools)
# ============================================================================

class Status(str, Enum):
    """Status of a build or run operation."""
    PENDING = "pending"
    BUILDING = "building"
    RUNNING = "running"
    PASS = "pass"
    FAIL = "fail"
    CRASH = "crash"
    TIMEOUT = "timeout"
    SKIP = "skip"

    def style(self) -> str:
        """Get the Rich style for this status."""
        styles = {
            Status.PENDING: Colors.PENDING,
            Status.BUILDING: Colors.RUNNING,
            Status.RUNNING: Colors.RUNNING,
            Status.PASS: Colors.PASS,
            Status.FAIL: Colors.FAIL,
            Status.CRASH: Colors.FAIL,
            Status.TIMEOUT: Colors.WARNING,
            Status.SKIP: Colors.SKIP,
        }
        return styles.get(self, Colors.DIM)

    def symbol(self) -> str:
        """Get a unicode symbol for this status."""
        symbols = {
            Status.PENDING: "...",
            Status.BUILDING: "...",
            Status.RUNNING: "...",
            Status.PASS: "OK",
            Status.FAIL: "FAIL",
            Status.CRASH: "CRASH",
            Status.TIMEOUT: "TIMEOUT",
            Status.SKIP: "SKIP",
        }
        return symbols.get(self, "?")


# ============================================================================
# Output Helpers
# ============================================================================

def print_header(title: str) -> None:
    """Print a styled header panel."""
    console.print()
    console.print(Panel(Text(title, style=Colors.HEADER), box=box.DOUBLE))
    console.print()


def print_step(msg: str, step_num: Optional[int] = None, total: Optional[int] = None) -> None:
    """Print a step indicator."""
    if step_num is not None and total is not None:
        prefix = f"[{step_num}/{total}]"
    else:
        prefix = "->"
    console.print(f"[{Colors.STEP}]{prefix}[/{Colors.STEP}] {msg}")


def print_success(msg: str) -> None:
    """Print a success message."""
    console.print(f"[{Colors.SUCCESS}]OK[/{Colors.SUCCESS}] {msg}")


def print_error(msg: str) -> None:
    """Print an error message."""
    console.print(f"[{Colors.ERROR}]ERROR[/{Colors.ERROR}] {msg}", style="red")


def print_warning(msg: str) -> None:
    """Print a warning message."""
    console.print(f"[{Colors.WARNING}]WARNING[/{Colors.WARNING}] {msg}")


def print_info(msg: str) -> None:
    """Print an info message."""
    console.print(f"[{Colors.INFO}]INFO[/{Colors.INFO}] {msg}")


def print_debug(msg: str) -> None:
    """Print a debug message."""
    console.print(f"[{Colors.DEBUG}]DEBUG[/{Colors.DEBUG}] {msg}", style=Colors.DEBUG)


def print_status(name: str, status: Status, note: str = "") -> None:
    """Print a status line for an item."""
    status_text = f"[{status.style()}]{status.symbol()}[/{status.style()}]"
    if note:
        console.print(f"  {name:.<40} {status_text} {note}")
    else:
        console.print(f"  {name:.<40} {status_text}")


# ============================================================================
# Symbols
# ============================================================================

class Symbols:
    """Unicode symbols for status indicators."""
    PASS = "\u2713"      # ✓
    FAIL = "\u2717"      # ✗
    SKIP = "\u25cb"      # ○
    TIMEOUT = "\u23f1"   # ⏱
    RUNNING = "\u25cf"   # ●


# ============================================================================
# Status Formatting Helpers
# ============================================================================

def format_passed(count: int) -> str:
    """Format passed count with color and symbol."""
    return f"[{Colors.PASS}]{Symbols.PASS} {count}[/{Colors.PASS}] passed"


def format_failed(count: int) -> str:
    """Format failed count with color and symbol."""
    return f"[{Colors.FAIL}]{Symbols.FAIL} {count}[/{Colors.FAIL}] failed"


def format_skipped(count: int) -> str:
    """Format skipped count with color and symbol."""
    return f"[{Colors.DIM}]{Symbols.SKIP} {count}[/{Colors.DIM}] skipped"


def format_summary_line(passed: int, failed: int, skipped: int) -> str:
    """Format a complete summary line with all counts."""
    return f"{format_passed(passed)}  {format_failed(failed)}  {format_skipped(skipped)}"


def status_symbol(status_value: str) -> str:
    """Get a colored symbol for a status string."""
    if status_value == "PASS" or status_value == "pass":
        return f"[{Colors.PASS}]{Symbols.PASS}[/{Colors.PASS}]"
    elif status_value in ("FAIL", "fail", "CRASH", "crash"):
        return f"[{Colors.FAIL}]{Symbols.FAIL}[/{Colors.FAIL}]"
    elif status_value in ("SKIP", "skip"):
        return f"[{Colors.DIM}]{Symbols.SKIP}[/{Colors.DIM}]"
    elif status_value in ("TIMEOUT", "timeout"):
        return f"[{Colors.WARNING}]{Symbols.TIMEOUT}[/{Colors.WARNING}]"
    else:
        return f"[{Colors.DIM}]-[/{Colors.DIM}]"


# ============================================================================
# Progress Helpers
# ============================================================================

def create_progress(description: str = "Processing...") -> Progress:
    """Create a standard progress bar."""
    return Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TaskProgressColumn(),
        TimeElapsedColumn(),
        console=console,
    )


def create_spinner_progress() -> Progress:
    """Create a spinner-only progress (no bar)."""
    return Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        console=console,
    )


# ============================================================================
# Table Helpers
# ============================================================================

def create_results_table(title: str = "Results") -> Table:
    """Create a standard results table."""
    table = Table(
        title=title,
        box=box.ROUNDED,
        show_header=True,
        header_style="bold",
    )
    return table


def create_summary_panel(content: str, title: str = "Summary", style: str = "green") -> Panel:
    """Create a summary panel."""
    return Panel(content, title=title, border_style=style)


# ============================================================================
# Version
# ============================================================================

VERSION = "0.1.0"
