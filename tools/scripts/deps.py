"""Dependency checking and installation for CARTS CLI."""

import re
import shutil
import subprocess
from dataclasses import dataclass
from enum import Enum
from typing import List, Optional, Tuple

import typer

from carts_styles import (
    Colors,
    Symbols,
    console,
    create_results_table,
    print_error,
    print_info,
    print_warning,
)
from scripts.config import Platform


# ============================================================================
# Data Structures
# ============================================================================


class DepStatus(Enum):
    OK = "ok"
    MISSING = "missing"
    VERSION_TOO_LOW = "version_too_low"


@dataclass
class Dependency:
    """Declaration of a required dependency."""

    name: str
    check_cmd: str
    version_flag: str = "--version"
    version_regex: str = r"(\d+(?:\.\d+)*)"
    min_version: Optional[Tuple[int, ...]] = None
    brew_package: Optional[str] = None
    xcode: bool = False
    apt_package: Optional[str] = None
    dnf_package: Optional[str] = None
    pacman_package: Optional[str] = None


@dataclass
class DepCheckResult:
    """Result of checking a single dependency."""

    dep: Dependency
    status: DepStatus
    found_version: Optional[str] = None
    found_path: Optional[str] = None


# ============================================================================
# Dependency Registry
# ============================================================================


def get_required_deps() -> List[Dependency]:
    """Return the list of all required dependencies."""
    return [
        Dependency(
            name="cmake",
            check_cmd="cmake",
            min_version=(3, 20),
            brew_package="cmake",
            apt_package="cmake",
            dnf_package="cmake",
            pacman_package="cmake",
        ),
        Dependency(
            name="ninja",
            check_cmd="ninja",
            brew_package="ninja",
            apt_package="ninja-build",
            dnf_package="ninja-build",
            pacman_package="ninja",
        ),
        Dependency(
            name="clang",
            check_cmd="clang",
            xcode=True,
            apt_package="clang",
            dnf_package="clang",
            pacman_package="clang",
        ),
        Dependency(
            name="clang++",
            check_cmd="clang++",
            xcode=True,
            apt_package="clang",
            dnf_package="clang",
            pacman_package="clang",
        ),
        Dependency(
            name="git",
            check_cmd="git",
            xcode=True,
            apt_package="git",
            dnf_package="git",
            pacman_package="git",
        ),
        Dependency(
            name="make",
            check_cmd="make",
            xcode=True,
            apt_package="build-essential",
            dnf_package="make",
            pacman_package="make",
        ),
        Dependency(
            name="python3",
            check_cmd="python3",
            brew_package="python3",
            apt_package="python3",
            dnf_package="python3",
            pacman_package="python",
        ),
    ]


# ============================================================================
# Version Parsing & Comparison
# ============================================================================


def _parse_version(version_str: str) -> Tuple[int, ...]:
    """Parse a version string like '3.20.1' into a tuple (3, 20, 1)."""
    return tuple(int(p) for p in version_str.split("."))


def _extract_version(output: str, regex: str) -> Optional[str]:
    """Extract a version string from command output using a regex."""
    match = re.search(regex, output)
    return match.group(1) if match else None


# ============================================================================
# Dependency Checking
# ============================================================================


def check_dependency(dep: Dependency) -> DepCheckResult:
    """Check whether a single dependency is available and meets version requirements."""
    path = shutil.which(dep.check_cmd)
    if path is None:
        return DepCheckResult(dep=dep, status=DepStatus.MISSING)

    found_version = None
    try:
        result = subprocess.run(
            [dep.check_cmd, dep.version_flag],
            capture_output=True,
            text=True,
            timeout=10,
        )
        combined = result.stdout + result.stderr
        found_version = _extract_version(combined, dep.version_regex)
    except (subprocess.SubprocessError, OSError):
        pass

    if dep.min_version and found_version:
        try:
            if _parse_version(found_version) < dep.min_version:
                return DepCheckResult(
                    dep=dep,
                    status=DepStatus.VERSION_TOO_LOW,
                    found_version=found_version,
                    found_path=path,
                )
        except (ValueError, TypeError):
            pass

    return DepCheckResult(
        dep=dep,
        status=DepStatus.OK,
        found_version=found_version,
        found_path=path,
    )


def check_all_deps() -> List[DepCheckResult]:
    """Check all required dependencies."""
    return [check_dependency(dep) for dep in get_required_deps()]


# ============================================================================
# Reporting
# ============================================================================


def print_dep_report(results: List[DepCheckResult]) -> bool:
    """Display a Rich table of dependency check results. Returns True if all pass."""
    table = create_results_table(title="Dependency Check")
    table.add_column("Dependency", style="bold")
    table.add_column("Status")
    table.add_column("Version")
    table.add_column("Path", style="dim")

    all_ok = True
    for r in results:
        if r.status == DepStatus.OK:
            status = f"[{Colors.PASS}]{Symbols.PASS} OK[/{Colors.PASS}]"
        elif r.status == DepStatus.MISSING:
            status = f"[{Colors.FAIL}]{Symbols.FAIL} MISSING[/{Colors.FAIL}]"
            all_ok = False
        else:
            min_ver = ".".join(str(v) for v in r.dep.min_version) if r.dep.min_version else "?"
            status = f"[{Colors.WARNING}]{Symbols.WARNING} <{min_ver}[/{Colors.WARNING}]"
            all_ok = False

        table.add_row(
            r.dep.name,
            status,
            r.found_version or "-",
            r.found_path or "-",
        )

    console.print()
    console.print(table)
    console.print()

    if all_ok:
        from carts_styles import print_success
        print_success("All dependencies satisfied")
    else:
        print_warning("Some dependencies are missing or outdated")

    return all_ok


# ============================================================================
# Platform Detection
# ============================================================================


def _detect_platform() -> Platform:
    """Detect current platform."""
    import platform as sys_platform

    system = sys_platform.system().lower()
    if system == "darwin":
        return Platform.MACOS
    elif system == "linux":
        return Platform.LINUX
    else:
        return Platform.WINDOWS


def detect_linux_package_manager() -> Optional[str]:
    """Detect which package manager is available on Linux."""
    for pm in ("apt-get", "dnf", "pacman"):
        if shutil.which(pm):
            return pm
    return None


# ============================================================================
# Install Command Generation
# ============================================================================


def get_install_commands(results: List[DepCheckResult]) -> List[str]:
    """Generate platform-specific install commands for failing deps."""
    failing = [r for r in results if r.status != DepStatus.OK]
    if not failing:
        return []

    plat = _detect_platform()
    if plat == Platform.MACOS:
        return _macos_install_commands(failing)
    elif plat == Platform.LINUX:
        return _linux_install_commands(failing)
    else:
        print_warning("Automatic installation is not supported on Windows.")
        return []


def _macos_install_commands(failing: List[DepCheckResult]) -> List[str]:
    """Generate macOS install commands."""
    commands: List[str] = []

    xcode_needed = any(r.dep.xcode for r in failing if r.dep.name != "python3")
    brew_packages: set[str] = set()

    for r in failing:
        if r.dep.name == "python3":
            continue
        if r.dep.brew_package:
            brew_packages.add(r.dep.brew_package)

    if xcode_needed:
        commands.append("xcode-select --install")

    if brew_packages:
        if not shutil.which("brew"):
            print_warning(
                "Homebrew not found. Install it first:\n"
                '  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
            )
        else:
            commands.append(f"brew install {' '.join(sorted(brew_packages))}")

    return commands


def _linux_install_commands(failing: List[DepCheckResult]) -> List[str]:
    """Generate Linux install commands."""
    pm = detect_linux_package_manager()
    if pm is None:
        print_warning("No supported package manager found (apt-get, dnf, pacman)")
        return []

    packages: set[str] = set()
    cmake_version_issue = False

    for r in failing:
        if r.dep.name == "python3":
            continue
        if r.status == DepStatus.VERSION_TOO_LOW and r.dep.name == "cmake" and pm == "apt-get":
            cmake_version_issue = True

        pkg = None
        if pm == "apt-get":
            pkg = r.dep.apt_package
        elif pm == "dnf":
            pkg = r.dep.dnf_package
        elif pm == "pacman":
            pkg = r.dep.pacman_package

        if pkg:
            packages.add(pkg)

    commands: List[str] = []

    if cmake_version_issue:
        print_warning(
            "Ubuntu's default cmake may be too old. "
            "Consider adding the Kitware APT repository:\n"
            "  https://apt.kitware.com/"
        )

    if packages:
        if pm == "apt-get":
            commands.append(f"sudo apt-get update && sudo apt-get install -y {' '.join(sorted(packages))}")
        elif pm == "dnf":
            commands.append(f"sudo dnf install -y {' '.join(sorted(packages))}")
        elif pm == "pacman":
            commands.append(f"sudo pacman -S --noconfirm {' '.join(sorted(packages))}")

    return commands


# ============================================================================
# Installation
# ============================================================================


def install_missing_deps(results: List[DepCheckResult], auto: bool = False) -> bool:
    """Attempt to install missing dependencies.

    Shows install commands and asks for confirmation (unless auto=True).
    Returns True if installation succeeded (or nothing to install).
    """
    commands = get_install_commands(results)
    if not commands:
        print_info("No automatic install commands available for missing dependencies")
        return False

    console.print()
    print_info("The following commands will be run to install missing dependencies:")
    for cmd in commands:
        console.print(f"    [bold]{cmd}[/bold]")
    console.print()

    if not auto:
        if not typer.confirm("Proceed with installation?"):
            print_info("Skipping dependency installation")
            return False

    success = True
    for cmd in commands:
        print_info(f"Running: {cmd}")
        result = subprocess.run(cmd, shell=True)
        if result.returncode != 0:
            print_error(f"Command failed: {cmd}")
            success = False

    return success
