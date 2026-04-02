#!/usr/bin/env python3
"""Project install driver for `dekk carts install`.

This script is intentionally small and imperative. Dekk owns the outer install
flow; this driver owns the CARTS-specific work that dekk cannot infer:

1. create local build/install directories
2. initialize the required submodules
3. build LLVM, Polygeist, ARTS, and CARTS in order
4. install the `carts` wrapper into `.install/bin`
"""

from __future__ import annotations
from pathlib import Path

from dekk import BinaryInstaller, print_error, print_header, print_info, print_step, print_success
from dekk.environment.spec import EnvironmentSpec

from scripts import (
    ARTS_NESTED_SUBMODULES,
    POLYGEIST_NESTED_SUBMODULES,
    SUBMODULE_ARTS,
    SUBMODULE_BENCHMARKS,
    SUBMODULE_POLYGEIST,
    run_subprocess,
)
from scripts.build import _export_pipeline_manifest
from scripts.platform import get_config


def _run(cmd: list[str], *, cwd: Path, label: str) -> None:
    result = run_subprocess(cmd, cwd=cwd, realtime=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"{label} failed")


def _setup_project() -> None:
    config = get_config()
    project_root = config.carts_dir

    print_step("Preparing build directories...")
    for dir_name in [".install", ".install/bin", "build", "external"]:
        (project_root / dir_name).mkdir(parents=True, exist_ok=True)

    print_step("Synchronizing submodule metadata...")
    _run(["git", "submodule", "sync", "--recursive"], cwd=project_root, label="submodule sync")

    print_step("Initializing top-level submodules...")
    _run(
        [
            "git", "submodule", "update", "--init",
            "--depth", "1", "--single-branch", "--recommend-shallow", "--jobs", "4",
            SUBMODULE_ARTS, SUBMODULE_POLYGEIST, SUBMODULE_BENCHMARKS,
        ],
        cwd=project_root,
        label="top-level submodule update",
    )

    arts_dir = project_root / SUBMODULE_ARTS
    print_step("Initializing ARTS nested submodules...")
    _run(["git", "submodule", "sync", "--recursive"], cwd=arts_dir, label="ARTS submodule sync")
    _run(
        [
            "git", "submodule", "update", "--init",
            "--depth", "1", "--single-branch", "--recommend-shallow", "--jobs", "4",
            *ARTS_NESTED_SUBMODULES,
        ],
        cwd=arts_dir,
        label="ARTS nested submodule update",
    )

    polygeist_dir = project_root / SUBMODULE_POLYGEIST
    print_step("Initializing Polygeist nested submodules...")
    _run(
        ["git", "submodule", "sync", "--recursive"],
        cwd=polygeist_dir,
        label="Polygeist submodule sync",
    )
    _run(
        [
            "git", "submodule", "update", "--init",
            "--depth", "1", "--single-branch", "--recommend-shallow", "--jobs", "4",
            *POLYGEIST_NESTED_SUBMODULES,
        ],
        cwd=polygeist_dir,
        label="Polygeist nested submodule update",
    )


def _build_project() -> None:
    config = get_config()
    project_root = config.carts_dir

    build_steps = [
        ("Building LLVM...", ["make", "llvm"]),
        ("Building Polygeist...", ["make", "polygeist"]),
        ("Building ARTS runtime...", ["make", "arts"]),
        ("Building CARTS...", ["make", "build"]),
    ]

    for label, cmd in build_steps:
        print_step(label)
        _run(cmd, cwd=project_root, label=label)

    _export_pipeline_manifest(config)


def _install_wrapper() -> None:
    config = get_config()
    project_root = config.carts_dir
    cli_script = project_root / "tools" / "carts_cli.py"
    spec = EnvironmentSpec.from_file(project_root / ".dekk.toml")

    print_step("Installing carts wrapper...")
    result = BinaryInstaller(project_root).install_wrapper(
        target=cli_script,
        spec=spec,
        name="carts",
        install_dir=project_root / ".install" / "bin",
        update_shell=True,
    )
    print_success(result.message)


def main() -> int:
    print_header("CARTS Install")
    print_info("Running CARTS project install through dekk-managed environment setup.")

    try:
        _setup_project()
        _build_project()
        _install_wrapper()
    except RuntimeError as exc:
        print_error(str(exc))
        return 1

    print_header("Install Complete")
    print_success("CARTS is installed. Use `carts --help` for daily commands.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
