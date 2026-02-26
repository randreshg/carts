"""Build command for Docker CLI."""

from __future__ import annotations

import os
import tempfile
import textwrap
import time

import typer

from carts_styles import print_error, print_header, print_info, print_step, print_success
from docker_shared import (
    BASE_IMAGE,
    WORKSPACE_VOLUME,
    _detect_docker_cpus,
    _docker,
    _docker_exec,
    _get_docker_dir,
    _image_exists,
    _volume_exists,
)


def docker_build(
    force: bool = typer.Option(False, "--force", "-f", help="Force rebuild from scratch"),
):
    """Build Docker base image and workspace volume."""
    docker_dir = _get_docker_dir()

    print_header("CARTS Docker Build")
    print_info("Building CARTS Docker components")

    base_exists = _image_exists(BASE_IMAGE)
    volume_exists = _volume_exists(WORKSPACE_VOLUME)

    if force:
        print_info("Force mode enabled")
    elif base_exists and volume_exists:
        print_info("Base image and workspace volume already exist")
        print_info("Use 'carts docker update' for incremental rebuilds")
        print_info("Use 'carts docker build --force' for full rebuild")
        raise typer.Exit(0)

    if not base_exists or force:
        print_step(f"Building Docker image: {BASE_IMAGE}")
        result = _docker("build", "-t", BASE_IMAGE, ".", cwd=docker_dir, check=False)
        if result.returncode != 0:
            print_error("Failed to build base image")
            raise typer.Exit(result.returncode)
    else:
        print_info(f"Using existing image: {BASE_IMAGE}")

    print_step("Starting builder container")
    _docker("rm", "-f", "arts-node-builder", check=False)
    docker_cpus = _detect_docker_cpus()
    result = _docker(
        "run",
        "-d",
        "--name",
        "arts-node-builder",
        "--hostname",
        "arts-node-builder",
        "--network",
        "bridge",
        f"--cpus={docker_cpus}",
        BASE_IMAGE,
        check=False,
    )
    if result.returncode != 0:
        print_error("Failed to start builder container")
        raise typer.Exit(result.returncode)

    try:
        print_info("Waiting for builder to initialize")
        time.sleep(5)

        if not volume_exists or force:
            git_url = os.environ.get("GIT_URL", "https://github.com/randreshg/carts.git")
            git_branch = os.environ.get("GIT_BRANCH", "mlir")
            print_step(f"Cloning and building CARTS (branch: {git_branch})")
            build_cmd = textwrap.dedent(
                f"""
                set -e
                rm -rf /opt/carts && mkdir -p /opt/carts
                git config --global init.defaultBranch main
                git config --global pull.rebase false
                git config --global core.preloadindex true
                git config --global core.fscache true
                git config --global gc.auto 256
                git clone --branch {git_branch} --depth 1 --single-branch --no-tags --progress {git_url} /opt/carts
                cd /opt/carts
                cd tools
                POETRY_VIRTUALENVS_IN_PROJECT=true poetry install --no-root
                cd /opt/carts
                export MAKEFLAGS='-j{docker_cpus}'
                export CMAKE_BUILD_PARALLEL_LEVEL={docker_cpus}
                python3 -u tools/setup/carts-setup.py
                """
            ).strip()
            result = _docker_exec("arts-node-builder", build_cmd, check=False)
            if result.returncode != 0:
                print_error("Build inside builder container failed")
                raise typer.Exit(result.returncode)
        else:
            print_info("Using existing workspace volume")

        print_step("Ensuring workspace volume exists")
        if not _volume_exists(WORKSPACE_VOLUME):
            result = _docker("volume", "create", WORKSPACE_VOLUME, capture_output=True, check=False)
            if result.returncode != 0:
                print_error("Failed to create workspace volume")
                raise typer.Exit(result.returncode)
            print_success(f"Created volume: {WORKSPACE_VOLUME}")

        if not volume_exists or force:
            print_step("Initializing workspace volume from builder")
            with tempfile.TemporaryDirectory(prefix="carts-docker-build-") as tmp_dir:
                result = _docker("cp", "arts-node-builder:/opt/carts/.", tmp_dir, check=False)
                if result.returncode != 0:
                    print_error("Failed to copy workspace from builder container")
                    raise typer.Exit(result.returncode)

                result = _docker(
                    "run",
                    "--rm",
                    "-v",
                    f"{WORKSPACE_VOLUME}:/dest",
                    "-v",
                    f"{tmp_dir}:/src",
                    BASE_IMAGE,
                    "bash",
                    "-c",
                    "cp -a /src/. /dest/",
                    check=False,
                )
                if result.returncode != 0:
                    print_error("Failed to initialize workspace volume")
                    raise typer.Exit(result.returncode)

            print_success("Workspace volume initialized")

    finally:
        print_step("Cleaning up builder container")
        _docker("stop", "arts-node-builder", check=False)
        _docker("rm", "arts-node-builder", check=False)

    print_header("Build Complete")
    print_success("Docker build completed")
    print_info("Next: run 'carts docker start'")
