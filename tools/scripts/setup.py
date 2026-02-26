"""Setup command for CARTS CLI."""

import os
import subprocess
import sys
from pathlib import Path
from typing import Optional

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


def _get_poetry_python(directory: Path) -> Optional[Path]:
    """Return the Poetry environment interpreter for a project directory."""
    result = _run_subprocess(
        ["poetry", "env", "info", "-e"],
        cwd=directory,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout:
        return None
    path = Path(result.stdout.strip())
    return path if path.exists() else None


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


def _install_python_envs() -> bool:
    """Install a single shared Python environment for CLI and benchmarks."""
    config = get_config()
    project_root = config.carts_dir
    tools_dir = project_root / "tools"
    benchmarks_scripts_dir = project_root / "external" / "carts-benchmarks" / "scripts"
    shared_python: Optional[Path] = None

    if not _check_command_exists('poetry'):
        print_error("Poetry is required to bootstrap Python environments.")
        print_info("Install Poetry and run `poetry -C tools install --no-root`")
        return False

    if not (tools_dir / "pyproject.toml").exists():
        print_error(f"Missing pyproject.toml in {tools_dir}")
        return False

    print_info("Installing shared Python dependencies...")
    try:
        _run_subprocess(
            ["poetry", "install", "--no-root"],
            cwd=tools_dir,
            env={"POETRY_VIRTUALENVS_IN_PROJECT": "true"},
            realtime=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        print_error("Failed to install dependencies for shared environment")
        return False

    shared_python = _get_poetry_python(tools_dir)
    if shared_python is None:
        print_error("Poetry environment interpreter not found after install")
        return False

    print_info("Verifying shared environment dependencies...")
    try:
        _run_subprocess(
            [str(shared_python), "-c", "import typer, rich, openpyxl"],
            cwd=project_root,
            realtime=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        print_error("Dependency verification failed for shared environment")
        return False

    if benchmarks_scripts_dir.exists():
        print_info("Verifying benchmark runner imports with shared environment...")
        verify_code = (
            f"import sys; "
            f"sys.path.insert(0, {str(project_root / 'tools')!r}); "
            f"sys.path.insert(0, {str(benchmarks_scripts_dir)!r}); "
            "import benchmark_runner"
        )
        try:
            _run_subprocess(
                [str(shared_python), "-c", verify_code],
                cwd=project_root,
                realtime=True,
                check=True,
            )
        except subprocess.CalledProcessError:
            print_error("Failed to import benchmark runner from shared environment")
            return False

    print_success("Shared Python environment is ready")
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
    skip_python_env: bool = typer.Option(
        False,
        "--skip-python-env",
        help="Skip shared Poetry environment setup in tools/",
    ),
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

        if not skip_python_env:
            if not _install_python_envs():
                print_error("Failed to set up Python environments")
                raise typer.Exit(1)

        if not _build_project():
            print_error("Failed to build project")
            raise typer.Exit(1)

    print_header("Setup Complete!")
