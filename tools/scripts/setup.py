"""Setup command for CARTS CLI."""

import os
import subprocess
import sys
from pathlib import Path

import typer

from carts_styles import (
    print_header,
    print_step,
    print_success,
    print_error,
    print_warning,
    print_info,
)
from scripts.config import get_config
from scripts.run import run_subprocess as _run_subprocess


def _check_command_exists(cmd: str) -> bool:
    """Check if a command exists in PATH."""
    try:
        subprocess.run([cmd, '--version'], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def _setup_project() -> bool:
    """Set up the CARTS project (submodules, directories)."""
    config = get_config()
    project_root = config.carts_dir

    print_info("Setting up CARTS project...")
    print_info(f"Project root: {project_root}")

    # Create necessary directories
    for dir_name in ['.install', 'build', 'external']:
        (project_root / dir_name).mkdir(exist_ok=True)

    # Initialize and update submodules
    print_info("Initializing and updating git submodules (shallow clone)...")
    try:
        _run_subprocess(
            [
                "git", "submodule", "update", "--init", "--recursive",
                "--depth", "1", "--jobs", "4",
            ],
            cwd=project_root,
            realtime=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        print_error("Failed to initialize/update submodules")
        return False

    return True


def _build_project() -> bool:
    """Build and install the CARTS project using the correct build order."""
    config = get_config()
    project_root = config.carts_dir
    carts_script_dir = project_root / 'tools'

    print_info("Building and installing CARTS project...")
    print_info("Following correct build order:")

    # Add carts to PATH for this session
    print_step("Adding CARTS to PATH...", step_num=0, total=5)
    current_path = os.environ.get('PATH', '')
    if str(carts_script_dir) not in current_path:
        os.environ['PATH'] = f"{carts_script_dir}:{current_path}"
        print_success(f"Added {carts_script_dir} to PATH for this session")

    # Also add to shell profiles for future sessions
    _add_to_path()

    build_steps = [
        (1, "Building LLVM...", ["carts", "build", "--llvm"]),
        (2, "Building Polygeist...", ["carts", "build", "--polygeist"]),
        (3, "Building ARTS runtime...", ["carts", "build", "--arts"]),
        (4, "Building CARTS...", ["carts", "build"]),
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


def _add_to_path() -> bool:
    """Add CARTS to the user's PATH by adding the carts script location."""
    config = get_config()
    carts_script_dir = config.carts_dir / 'tools'

    if not (carts_script_dir / 'carts').exists():
        print_error("CARTS script not found. Please ensure you're in the correct directory.")
        return False

    # Check if carts is already in current PATH
    if _check_command_exists('carts'):
        print_success("carts command is already available in your PATH")
        return True

    # Detect shell profile
    shell_profile = None
    if 'zsh' in os.environ.get('SHELL', ''):
        shell_profile = Path.home() / '.zshrc'
    elif 'bash' in os.environ.get('SHELL', ''):
        shell_profile = Path.home() / '.bashrc'
        if not shell_profile.exists():
            shell_profile = Path.home() / '.bash_profile'
    else:
        print_warning("Unsupported shell. Please manually add to your shell profile:")
        print_info(f"export PATH=\"{carts_script_dir}:$PATH\"")
        return False

    # Check if already added to shell profile
    if shell_profile.exists():
        content = shell_profile.read_text()
        if str(carts_script_dir) in content:
            print_info("CARTS is already configured in your shell profile")
            print_info("Try: source ~/.zshrc (or restart your terminal)")
            return True

    # Add to shell profile
    with open(shell_profile, 'a') as f:
        f.write(f"\n# CARTS\nexport PATH=\"{carts_script_dir}:$PATH\"\n")

    print_success(f"Added CARTS to {shell_profile}")
    print_info("Run 'source ~/.zshrc' or restart your terminal to use 'carts'")
    return True


def setup(
    skip_build: bool = typer.Option(
        False, "--skip-build", help="Skip building"),
    add_to_path: bool = typer.Option(
        False, "--add-to-path", help="Add carts to PATH"),
):
    """Set up CARTS environment."""
    print_header("CARTS Setup Script")

    # Handle add-to-path first (no dependency installation)
    if add_to_path:
        if _add_to_path():
            print_success("CARTS added to PATH successfully!")
            print_info("You can now use 'carts' from anywhere")
        else:
            raise typer.Exit(1)
        return

    if not skip_build:
        if not _setup_project():
            print_error("Failed to set up project")
            raise typer.Exit(1)

        if not _build_project():
            print_error("Failed to build project")
            raise typer.Exit(1)

    print_header("Setup Complete!")
