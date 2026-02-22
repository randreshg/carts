"""Install command for CARTS CLI — full setup: deps, submodules, and build."""

import os
import subprocess
from pathlib import Path
from typing import Optional

import typer

from carts_styles import (
    print_error,
    print_header,
    print_info,
    print_step,
    print_success,
    print_warning,
)
from scripts.config import get_config
from scripts.deps import (
    check_all_deps,
    install_missing_deps,
    print_dep_report,
)
from scripts.run import run_subprocess as _run_subprocess


# ============================================================================
# Internal helpers
# ============================================================================


def _check_command_exists(cmd: str) -> bool:
    """Check if a command exists in PATH."""
    try:
        subprocess.run([cmd, "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def _add_to_path() -> bool:
    """Add CARTS to the user's PATH by adding the carts script location."""
    config = get_config()
    carts_script_dir = config.carts_dir / "tools"

    if not (carts_script_dir / "carts").exists():
        print_error("CARTS script not found. Please ensure you're in the correct directory.")
        return False

    if _check_command_exists("carts"):
        print_success("carts command is already available in your PATH")
        return True

    shell_profile = None
    if "zsh" in os.environ.get("SHELL", ""):
        shell_profile = Path.home() / ".zshrc"
    elif "bash" in os.environ.get("SHELL", ""):
        shell_profile = Path.home() / ".bashrc"
        if not shell_profile.exists():
            shell_profile = Path.home() / ".bash_profile"
    else:
        print_warning("Unsupported shell. Please manually add to your shell profile:")
        print_info(f'export PATH="{carts_script_dir}:$PATH"')
        return False

    if shell_profile.exists():
        content = shell_profile.read_text()
        if str(carts_script_dir) in content:
            print_info("CARTS is already configured in your shell profile")
            print_info("Try: source ~/.zshrc (or restart your terminal)")
            return True

    with open(shell_profile, "a") as f:
        f.write(f'\n# CARTS\nexport PATH="{carts_script_dir}:$PATH"\n')

    print_success(f"Added CARTS to {shell_profile}")
    print_info("Run 'source ~/.zshrc' or restart your terminal to use 'carts'")
    return True


def _setup_project() -> bool:
    """Set up the CARTS project (submodules, directories)."""
    config = get_config()
    project_root = config.carts_dir

    print_info("Setting up CARTS project...")
    print_info(f"Project root: {project_root}")

    for dir_name in [".install", "build", "external"]:
        (project_root / dir_name).mkdir(exist_ok=True)

    print_info("Initializing and updating git submodules...")
    try:
        _run_subprocess(
            [
                "git", "submodule", "update", "--init", "--recursive",
                "--depth", "1", "--single-branch",
                "--recommend-shallow", "--jobs", "4",
            ],
            cwd=project_root,
            realtime=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        print_error("Failed to initialize/update submodules")
        return False

    return True


def _build_project(gcc_toolchain: Optional[str] = None) -> bool:
    """Build and install the CARTS project using the correct build order."""
    config = get_config()
    project_root = config.carts_dir
    carts_script_dir = project_root / "tools"

    print_info("Building and installing CARTS project...")
    print_info("Following correct build order:")

    # Add carts to PATH for this session
    print_step("Adding CARTS to PATH...", step_num=0, total=5)
    current_path = os.environ.get("PATH", "")
    if str(carts_script_dir) not in current_path:
        os.environ["PATH"] = f"{carts_script_dir}:{current_path}"
        print_success(f"Added {carts_script_dir} to PATH for this session")

    _add_to_path()

    gcc_args = ["--gcc-toolchain", gcc_toolchain] if gcc_toolchain else []

    build_steps = [
        (1, "Building LLVM...", ["carts", "build", "--llvm"] + gcc_args),
        (2, "Building Polygeist...", ["carts", "build", "--polygeist"] + gcc_args),
        (3, "Building ARTS runtime...", ["carts", "build", "--arts"] + gcc_args),
        (4, "Building CARTS...", ["carts", "build"] + gcc_args),
    ]

    for step_num, description, cmd in build_steps:
        print_step(description, step_num=step_num, total=5)
        try:
            _run_subprocess(cmd, cwd=project_root, realtime=True, check=True)
        except subprocess.CalledProcessError:
            print_error(f"Failed: {description}")
            return False

    print_success("All build steps completed successfully!")
    return True


# ============================================================================
# CLI command
# ============================================================================


def install(
    check: bool = typer.Option(
        False, "--check", help="Only check and report dependencies, then exit"),
    skip_deps: bool = typer.Option(
        False, "--skip-deps", help="Skip dependency checking entirely"),
    skip_build: bool = typer.Option(
        False, "--skip-build", help="Skip building (only check/install deps and init submodules)"),
    auto: bool = typer.Option(
        False, "--auto", "-y", help="Auto-install missing deps without prompting"),
    gcc_toolchain: str = typer.Option(
        None, "--gcc-toolchain",
        help="Path to GCC toolchain (e.g. /opt/shared/gcc/12.2.0) for systems where system clang picks up an old libstdc++"),
):
    """Install CARTS: check dependencies, init submodules, and build everything."""
    print_header("CARTS Install")

    # 1. Dependency checking / installation
    if not skip_deps:
        results = check_all_deps()
        all_ok = print_dep_report(results)

        if check:
            raise typer.Exit(0 if all_ok else 1)

        if not all_ok:
            installed = install_missing_deps(results, auto=auto)
            if installed:
                results = check_all_deps()
                all_ok = print_dep_report(results)
            if not all_ok:
                print_error(
                    "Required dependencies are still missing. Cannot proceed."
                )
                raise typer.Exit(1)
    elif check:
        print_warning("--check and --skip-deps are mutually exclusive")
        raise typer.Exit(1)

    # 2. Submodules and directories
    if not _setup_project():
        print_error("Failed to set up project")
        raise typer.Exit(1)

    # 3. Build everything
    if not skip_build:
        if not _build_project(gcc_toolchain=gcc_toolchain):
            print_error("Failed to build project")
            raise typer.Exit(1)

    print_header("Install Complete!")
