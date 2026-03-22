"""Install command for CARTS CLI — full setup: deps, submodules, and build."""

import os
import subprocess
from pathlib import Path
from typing import Optional

from dekk import BinaryInstaller, EnvironmentActivator
from dekk import Colors, Exit, Option, console, print_error, print_header, print_info, print_step, print_success, print_warning
from dekk.envspec import EnvironmentSpec
from scripts.platform import get_config
from scripts import (
    run_subprocess as _run_subprocess,
    ARTS_NESTED_SUBMODULES,
    POLYGEIST_NESTED_SUBMODULES,
    SUBMODULE_ARTS,
    SUBMODULE_BENCHMARKS,
    SUBMODULE_POLYGEIST,
)


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


def _install_wrapper() -> bool:
    """Generate a self-contained wrapper via dekk's BinaryInstaller.

    Creates ``.install/bin/carts`` — a shell script that bakes in all
    environment variables from ``.dekk.toml`` and execs the Python CLI
    directly, just like apxm does.  Also updates the shell profile to add
    ``.install/bin/`` to PATH.

    The wrapper goes into ``.install/bin/`` (not ``.install/``) because
    ``.install/carts/`` is already a directory holding build artifacts
    (carts-compile, etc.).
    """
    config = get_config()
    project_root = config.carts_dir
    cli_script = project_root / "tools" / "carts_cli.py"
    spec_file = project_root / ".dekk.toml"

    if not cli_script.exists():
        print_error("carts_cli.py not found. Please ensure you're in the correct directory.")
        return False

    if not spec_file.exists():
        print_error(".dekk.toml not found at project root.")
        return False

    # Use the bootstrap venv Python (has dekk installed) so the wrapper
    # is fully self-contained — no runtime bootstrap needed.
    bootstrap_python = project_root / ".carts" / "bootstrap-venv" / "bin" / "python3"
    if not bootstrap_python.exists():
        bootstrap_python = None  # fall back to no python (will use system)

    try:
        spec = EnvironmentSpec.from_file(spec_file)
        install_dir = project_root / ".install" / "bin"
        res = BinaryInstaller(project_root).install_wrapper(
            target=cli_script,
            spec=spec,
            python=bootstrap_python,
            name="carts",
            install_dir=install_dir,
        )
        print_success(res.message)
        return True
    except Exception as e:
        print_warning(f"Wrapper install failed: {e}")
        return False


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
            ["git", "submodule", "sync", "--recursive"],
            cwd=project_root,
            realtime=True,
            check=True,
        )
        _run_subprocess(
            [
                "git", "submodule", "update", "--init",
                "--depth", "1", "--single-branch",
                "--recommend-shallow", "--jobs", "4",
                SUBMODULE_ARTS, SUBMODULE_POLYGEIST, SUBMODULE_BENCHMARKS,
            ],
            cwd=project_root,
            realtime=True,
            check=True,
        )
        _run_subprocess(
            [
                "git", "submodule", "sync", "--recursive",
            ],
            cwd=project_root / SUBMODULE_ARTS,
            realtime=True,
            check=True,
        )
        _run_subprocess(
            [
                "git", "submodule", "update", "--init",
                "--depth", "1", "--single-branch",
                "--recommend-shallow", "--jobs", "4",
                *ARTS_NESTED_SUBMODULES,
            ],
            cwd=project_root / SUBMODULE_ARTS,
            realtime=True,
            check=True,
        )
        _run_subprocess(
            [
                "git", "submodule", "sync", "--recursive",
            ],
            cwd=project_root / SUBMODULE_POLYGEIST,
            realtime=True,
            check=True,
        )
        _run_subprocess(
            [
                "git", "submodule", "update", "--init",
                "--depth", "1", "--single-branch",
                "--recommend-shallow", "--jobs", "4",
                *POLYGEIST_NESTED_SUBMODULES,
            ],
            cwd=project_root / SUBMODULE_POLYGEIST,
            realtime=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        print_error("Failed to initialize/update submodules")
        return False

    return True


def _build_project(cc: Optional[str] = None, cxx: Optional[str] = None) -> bool:
    """Build and install the CARTS project using the correct build order."""
    config = get_config()
    project_root = config.carts_dir
    carts_script_dir = project_root / "tools"

    print_info("Building and installing CARTS project...")
    print_info("Following correct build order:")

    # Add carts to PATH for this session (both tools/ and .install/)
    print_step("Adding CARTS to PATH...", step_num=0, total=4)
    current_path = os.environ.get("PATH", "")
    for path_dir in [carts_script_dir, project_root / ".install"]:
        if str(path_dir) not in current_path:
            os.environ["PATH"] = f"{path_dir}:{os.environ['PATH']}"
            print_success(f"Added {path_dir} to PATH for this session")

    cc_args = []
    if cc:
        cc_args += ["--cc", cc]
    if cxx:
        cc_args += ["--cxx", cxx]

    build_steps = [
        (1, "Building LLVM...", ["carts", "build", "--llvm"] + cc_args),
        (2, "Building Polygeist...", ["carts", "build", "--polygeist"]),
        (3, "Building ARTS runtime...", ["carts", "build", "--arts"]),
        (4, "Building CARTS...", ["carts", "build"]),
    ]

    for step_num, description, cmd in build_steps:
        print_step(description, step_num=step_num, total=4)
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
    benchmarks_scripts_dir = project_root / SUBMODULE_BENCHMARKS / "scripts"
    shared_python: Optional[Path] = None

    venv_python = tools_dir / ".venv" / "bin" / "python"

    if _check_command_exists("poetry") and (tools_dir / "pyproject.toml").exists():
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
    elif venv_python.exists():
        # Venv already bootstrapped by the wrapper script (pip fallback)
        shared_python = venv_python
        print_info("Using existing Python environment (bootstrapped by wrapper)")
    else:
        print_error("No Python environment found. Run 'dekk tools/carts_cli.py install' or './tools/carts install' to bootstrap it.")
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
            "import runner"
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


# ============================================================================
# CLI command
# ============================================================================


def install(
    check: bool = Option(
        False, "--check", help="Only check and report dependencies, then exit"),
    skip_deps: bool = Option(
        False, "--skip-deps", help="Skip dependency checking entirely"),
    skip_build: bool = Option(
        False, "--skip-build", help="Skip building (only check/install deps and init submodules)"),
    skip_python_env: bool = Option(
        False,
        "--skip-python-env",
        help="Skip shared Poetry environment setup in tools/",
    ),
    auto: bool = Option(
        False, "--auto", "-y", help="Auto-install missing deps without prompting"),
    cc: Optional[str] = Option(
        None, "--cc",
        help="C compiler for LLVM bootstrap (default: clang)"),
    cxx: Optional[str] = Option(
        None, "--cxx",
        help="C++ compiler for LLVM bootstrap (default: clang++)"),
):
    """Install CARTS: check dependencies, init submodules, and build everything."""
    print_header("CARTS Install")

    # 1. Dependency checking via dekk EnvironmentActivator
    if not skip_deps:
        try:
            activator = EnvironmentActivator.from_cwd()
            result = activator.activate()
            if result.missing_tools:
                missing = ", ".join(result.missing_tools)
                console.print(f"\n[{Colors.ERROR}]Missing tools: {missing}[/{Colors.ERROR}]")
                console.print(f"Run [{Colors.HIGHLIGHT}]carts doctor[/{Colors.HIGHLIGHT}] for detailed diagnostics.\n")
                if check or not auto:
                    raise Exit(1)
            elif check:
                print_success("All dependencies satisfied")
                raise Exit(0)
        except FileNotFoundError:
            print_warning("No .dekk.toml found — skipping dependency validation")
            if check:
                raise Exit(0)
    elif check:
        print_warning("--check and --skip-deps are mutually exclusive")
        raise Exit(1)

    # 2. Submodules and directories
    if not _setup_project():
        print_error("Failed to set up project")
        raise Exit(1)

    # 3. Python environments
    if not skip_python_env:
        if not _install_python_envs():
            print_error("Failed to set up Python environments")
            raise Exit(1)

    # 4. Build everything
    if not skip_build:
        if not _build_project(cc=cc, cxx=cxx):
            print_error("Failed to build project")
            raise Exit(1)

    # 5. Generate self-contained wrapper at .install/carts and update PATH
    _install_wrapper()

    print_header("Install Complete!")
