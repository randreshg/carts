"""Test and lit commands for CARTS CLI."""

from pathlib import Path
from typing import List, Optional, Sequence, Tuple
import subprocess
import os

import typer

from sniff import Colors, console, print_error, print_info, print_success
from scripts.platform import CartsConfig, get_config
from scripts import (
    run_subprocess,
    TOOL_CARTS_COMPILE,
    TOOL_FILECHECK,
    TOOL_LLVM_LIT,
    SUBMODULE_POLYGEIST,
    POLYGEIST_NESTED_SUBMODULES,
)


def _resolve_lit_tool_paths(config: CartsConfig) -> Tuple[Path, Path, Path]:
    """Resolve required test tools from the install tree."""
    return (
        config.get_llvm_tool(TOOL_LLVM_LIT),
        config.get_llvm_tool(TOOL_FILECHECK),
        config.get_carts_tool(TOOL_CARTS_COMPILE),
    )


def _setup_lit_pythonpath(config: CartsConfig) -> None:
    """Configure PYTHONPATH so `llvm-lit` can import `lit` reliably."""
    lit_python_paths: List[str] = []

    try:
        result = subprocess.run(
            ["python3", "-c",
             "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"],
            capture_output=True, text=True,
        )
        python_version = result.stdout.strip()
        llvm_python_path = config.llvm_install_dir / "lib" / \
            f"python{python_version}" / "site-packages"
        if llvm_python_path.is_dir():
            lit_python_paths.append(str(llvm_python_path))
    except Exception:
        pass

    llvm_lit_runtime = config.llvm_install_dir / "utils" / "lit"
    if (llvm_lit_runtime / "lit").is_dir():
        lit_python_paths.append(str(llvm_lit_runtime))

    source_lit_runtime = (
        config.carts_dir / SUBMODULE_POLYGEIST
        / POLYGEIST_NESTED_SUBMODULES[0] / "llvm" / "utils" / "lit"
    )
    if (source_lit_runtime / "lit").is_dir():
        lit_python_paths.append(str(source_lit_runtime))

    if not lit_python_paths:
        return

    deduped_paths = list(dict.fromkeys(lit_python_paths))
    env_pythonpath = os.environ.get("PYTHONPATH", "")
    if env_pythonpath:
        deduped_paths.append(env_pythonpath)
    os.environ["PYTHONPATH"] = os.pathsep.join(deduped_paths)


def _resolve_lit_targets(config: CartsConfig, suite: str) -> List[Path]:
    """Resolve built-in test paths for a named suite."""
    tests_dir = config.carts_dir / "tests"
    if not tests_dir.is_dir():
        print_error(f"Tests directory not found at {tests_dir}")
        raise typer.Exit(1)

    if suite in ("all", "contracts"):
        test_paths = [tests_dir / "contracts"]
    else:
        print_error(f"Unknown test suite '{suite}'")
        print_info("Available suites: all, contracts")
        raise typer.Exit(1)

    for test_path in test_paths:
        if not test_path.is_dir():
            print_error(f"Test path not found: {test_path}")
            raise typer.Exit(1)

    return test_paths


def _ensure_lit_tools(config: CartsConfig) -> Tuple[Path, Path, Path]:
    """Fail fast if the bundled lit toolchain is not available."""
    llvm_lit, filecheck, carts_compile = _resolve_lit_tool_paths(config)

    missing = []
    if not llvm_lit.is_file():
        missing.append(f"{TOOL_LLVM_LIT} ({llvm_lit})")
    if not filecheck.is_file():
        missing.append(f"{TOOL_FILECHECK} ({filecheck})")
    if not carts_compile.is_file():
        missing.append(f"{TOOL_CARTS_COMPILE} ({carts_compile})")

    if missing:
        print_error("Required test tools not found:")
        for tool in missing:
            console.print(f"  - {tool}")
        print_info(
            "Run `carts build` to install llvm-lit/FileCheck/carts-compile under .install/."
        )
        raise typer.Exit(1)

    return llvm_lit, filecheck, carts_compile


def _run_lit(
    test_paths: Sequence[Path],
    verbose_tests: bool,
    suite_name: str = "custom",
    extra_args: Optional[Sequence[str]] = None,
) -> None:
    """Run llvm-lit on the given paths and exit with its return code."""
    config = get_config()
    print_info("Running CARTS test suite...")
    llvm_lit, _, _ = _ensure_lit_tools(config)
    _setup_lit_pythonpath(config)

    console.print(f"Test suite: [{Colors.INFO}]{suite_name}[/{Colors.INFO}]")
    for path in test_paths:
        console.print(f"Test path: [{Colors.DEBUG}]{path}[/{Colors.DEBUG}]")
    console.print()

    cmd = [str(llvm_lit)]
    if verbose_tests:
        cmd.append("-v")
    if extra_args:
        cmd.extend(extra_args)
    for path in test_paths:
        cmd.append(str(path))

    result = run_subprocess(cmd, check=False)
    console.print()
    if result.returncode == 0:
        print_success("CARTS test suite completed successfully!")
    else:
        print_error(f"CARTS test suite failed with exit code {result.returncode}")
    raise typer.Exit(result.returncode)


def test(
    suite: str = typer.Option("all", "--suite", "-s",
                              help="Test suite: all, contracts"),
    verbose_tests: bool = typer.Option(
        False, "-v", help="Verbose test output"),
):
    """Run CARTS test suite using llvm-lit."""
    config = get_config()
    test_paths = _resolve_lit_targets(config, suite)
    _run_lit(test_paths, verbose_tests=verbose_tests, suite_name=suite)


def lit(
    ctx: typer.Context,
    suite: str = typer.Option(
        "contracts",
        "--suite",
        "-s",
        help="Default suite when no explicit paths are passed: contracts, all",
    ),
    verbose_tests: bool = typer.Option(
        False, "--verbose", "-v", help="Verbose lit output"
    ),
):
    """Run the bundled llvm-lit directly.

    Examples:
      carts lit
      carts lit tests/contracts/for_dispatch_clamp.mlir
      carts lit -v tests/contracts
      carts lit -- --filter=for_lowering tests/contracts
    """
    config = get_config()
    raw_args = list(ctx.args)

    explicit_targets: List[Path] = []
    passthrough_args: List[str] = []

    if "--" in raw_args:
        idx = raw_args.index("--")
        before = raw_args[:idx]
        passthrough_args = raw_args[idx + 1:]
    else:
        before = raw_args

    for arg in before:
        if arg.startswith("-"):
            passthrough_args.append(arg)
        else:
            explicit_targets.append(
                (config.carts_dir / arg).resolve()
                if not Path(arg).is_absolute()
                else Path(arg)
            )

    if explicit_targets:
        _run_lit(explicit_targets, verbose_tests=verbose_tests,
                 extra_args=passthrough_args)
        return

    default_paths = _resolve_lit_targets(config, suite)
    _run_lit(default_paths, verbose_tests=verbose_tests,
             suite_name=suite, extra_args=passthrough_args)
