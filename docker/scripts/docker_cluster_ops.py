"""Cluster lifecycle commands: start/stop/status/exec."""

from __future__ import annotations

import os
import sys
import time
from typing import List

import typer

from carts_styles import print_error, print_header, print_info, print_step, print_success, print_warning
from docker_shared import (
    BASE_IMAGE,
    BUILT_IMAGE,
    NODES,
    NUM_NODES,
    WORKSPACE_VOLUME,
    _docker,
    _get_docker_dir,
    _get_running_containers,
    _image_exists,
    _kill_container_processes,
    _setup_hostname_resolution,
    _start_slurm_cluster,
    _volume_exists,
)


def _docker_stop(quiet: bool) -> None:
    if not quiet:
        print_header("CARTS Docker Stop")

    running = [name for name in _get_running_containers() if name.startswith("arts-node-")]
    if not running:
        if not quiet:
            print_warning("No running ARTS containers found")
        return

    for container in running:
        if not quiet:
            print_info(f"Stopping processes in {container}")
        _kill_container_processes(container)

    _docker("stop", *running, check=False)
    _docker("rm", "-f", *running, check=False)

    if not quiet:
        print_success("All ARTS containers stopped")


def docker_stop():
    """Stop ARTS containers after killing active processes."""
    _docker_stop(quiet=False)


def docker_start():
    """Start Docker containers and initialize Slurm."""
    _get_docker_dir()

    print_header("CARTS Docker Start")

    if not _image_exists(BASE_IMAGE):
        print_error(f"Required image '{BASE_IMAGE}' not found")
        print_warning("Run 'carts docker build' first")
        raise typer.Exit(1)

    print_step("Stopping existing ARTS containers")
    _docker_stop(quiet=True)

    stopped = _docker("ps", "-a", "--format", "{{.Names}}", capture_output=True, check=False)
    if stopped.returncode == 0:
        old_names = [
            name.strip()
            for name in (stopped.stdout or "").splitlines()
            if name.strip().startswith("arts-node-")
        ]
        if old_names:
            _docker("rm", "-f", *old_names, check=False)

    print_step("Preparing shared workspace volume")
    if not _volume_exists(WORKSPACE_VOLUME):
        result = _docker("volume", "create", WORKSPACE_VOLUME, capture_output=True, check=False)
        if result.returncode != 0:
            print_error("Failed to create workspace volume")
            raise typer.Exit(result.returncode)

        if not _image_exists(BUILT_IMAGE):
            print_error(f"Workspace volume is empty and image '{BUILT_IMAGE}' is missing")
            print_warning("Run 'carts docker build' to initialize the workspace volume")
            raise typer.Exit(1)

        result = _docker(
            "run",
            "--rm",
            "-v",
            f"{WORKSPACE_VOLUME}:/dest",
            BUILT_IMAGE,
            "bash",
            "-c",
            "cp -a /opt/carts/. /dest/",
            check=False,
        )
        if result.returncode != 0:
            print_error("Failed to seed workspace volume from arts-node:built")
            raise typer.Exit(result.returncode)
        print_success("Initialized workspace volume")

    print_step(f"Starting {NUM_NODES} node containers")
    for index, node in enumerate(NODES):
        result = _docker(
            "run",
            "-d",
            "--name",
            node,
            "--hostname",
            node,
            "--network",
            "bridge",
            "-e",
            f"ARTS_NODE_ID={index}",
            "-v",
            f"{WORKSPACE_VOLUME}:/opt/carts",
            BASE_IMAGE,
            check=False,
        )
        if result.returncode != 0:
            print_error(f"Failed to start container: {node}")
            raise typer.Exit(result.returncode)

    print_info("Waiting for containers to become ready")
    time.sleep(3)

    running = [node for node in NODES if node in _get_running_containers()]
    if len(running) != NUM_NODES:
        print_warning(f"Expected {NUM_NODES} running nodes, found {len(running)}")

    print_step("Configuring inter-container hostname resolution")
    _setup_hostname_resolution(running)

    _start_slurm_cluster(running)

    print_header("Start Complete")
    print_success("CARTS Docker cluster is running")
    print_info("Use 'carts docker status' to verify container and Slurm health")


def docker_status():
    """Show Docker container and Slurm status."""
    result = _docker(
        "ps",
        "-a",
        "--filter",
        "name=arts-node",
        "--format",
        "table {{.Names}}\\t{{.Status}}\\t{{.Ports}}",
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        print_error("Failed to query Docker containers")
        if result.stderr:
            print_error(result.stderr.strip())
        raise typer.Exit(1)

    output = (result.stdout or "").strip()
    if not output or output.count("\n") == 0:
        print_info("No ARTS containers found. Run 'carts docker start' to begin.")
        raise typer.Exit(0)

    print_header("CARTS Docker Status")
    print(output)

    slurm = _docker("exec", "arts-node-1", "sinfo", capture_output=True, check=False)
    if slurm.returncode == 0 and slurm.stdout and slurm.stdout.strip():
        print_info("Slurm status:")
        print(slurm.stdout.strip())
    else:
        print_warning("Slurm not active or arts-node-1 is not running")


def docker_exec(
    command: List[str] = typer.Argument(..., help="Command to run in container"),
    node: int = typer.Option(1, "--node", "-n", min=1, max=NUM_NODES, help="Node number (1-6)"),
):
    """Execute a command inside a running CARTS container."""
    container = f"arts-node-{node}"

    inspect = _docker("inspect", "--format", "{{.State.Running}}", container, capture_output=True, check=False)
    if inspect.returncode != 0 or (inspect.stdout or "").strip() != "true":
        print_error(f"Container '{container}' is not running")
        print_info("Start containers with 'carts docker start' first")
        raise typer.Exit(1)

    cmd = ["docker", "exec"]
    if sys.stdin.isatty() and sys.stdout.isatty():
        cmd.append("-it")
    cmd.extend([container, *list(command)])

    os.execvp("docker", cmd)
