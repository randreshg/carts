"""Shared helpers and constants for Docker CLI commands."""

from __future__ import annotations

import sys
import textwrap
import time
from pathlib import Path
from typing import Dict, List

# Ensure shared CARTS CLI modules are importable when loaded from tools adapters.
TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import typer

from carts_styles import print_error, print_info, print_step, print_warning
from scripts.config import get_config
from scripts.run import run_subprocess


NODES = [f"arts-node-{i}" for i in range(1, 7)]
NUM_NODES = 6
WORKSPACE_VOLUME = "carts-workspace"
BASE_IMAGE = "arts-node-base"
BUILT_IMAGE = "arts-node:built"


def _get_docker_dir() -> Path:
    docker_dir = get_config().carts_dir / "docker"
    if not docker_dir.is_dir():
        print_error(f"Docker directory not found at {docker_dir}")
        raise typer.Exit(1)
    return docker_dir


def _docker(*args: str, **kwargs):
    return run_subprocess(["docker", *args], **kwargs)


def _docker_exec(container: str, cmd: str, **kwargs):
    return run_subprocess(["docker", "exec", container, "bash", "-c", cmd], **kwargs)


def _get_running_containers() -> List[str]:
    result = _docker("ps", "--filter", "name=arts-node", "--format", "{{.Names}}", capture_output=True, check=False)
    if result.returncode != 0:
        return []
    return [line.strip() for line in (result.stdout or "").splitlines() if line.strip()]


def _image_exists(name: str) -> bool:
    result = _docker("images", "-q", name, capture_output=True, check=False)
    return result.returncode == 0 and bool((result.stdout or "").strip())


def _volume_exists(name: str) -> bool:
    result = _docker("volume", "inspect", name, capture_output=True, check=False)
    return result.returncode == 0


def _container_exists(name: str) -> bool:
    result = _docker("ps", "-a", "--format", "{{.Names}}", capture_output=True, check=False)
    if result.returncode != 0:
        return False
    names = {(line or "").strip() for line in (result.stdout or "").splitlines()}
    return name in names


def _detect_docker_cpus(default: str = "2") -> str:
    result = _docker("run", "--rm", "ubuntu:22.04", "nproc", capture_output=True, check=False)
    if result.returncode == 0 and result.stdout and result.stdout.strip().isdigit():
        return result.stdout.strip()
    return default


def _setup_hostname_resolution(nodes: List[str]) -> None:
    if not nodes:
        return

    ips: Dict[str, str] = {}
    for node in nodes:
        result = _docker(
            "inspect",
            "-f",
            "{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}",
            node,
            capture_output=True,
            check=False,
        )
        ip = (result.stdout or "").strip()
        if result.returncode != 0 or not ip:
            print_warning(f"Could not resolve IP for {node}; skipping host entry")
            continue
        ips[node] = ip

    if not ips:
        return

    hosts_entries = "\\n".join(f"{ip} {name}" for name, ip in ips.items())
    for node in nodes:
        _docker_exec(node, f"echo -e '{hosts_entries}' >> /etc/hosts", check=False)


def _start_slurm_cluster(nodes: List[str]) -> None:
    if not nodes:
        return

    print_step("Starting Slurm daemons")

    for node in nodes:
        _docker_exec(node, "cp /opt/carts/docker/slurm.conf /etc/slurm/slurm.conf 2>/dev/null || true", check=False)
        _docker_exec(
            node,
            (
                "mkdir -p /run/munge /var/log/munge && "
                "chown -R munge:munge /etc/munge /var/log/munge /run/munge && "
                "chmod 755 /run/munge /var/log/munge && "
                "chmod 400 /etc/munge/munge.key && "
                "chmod 755 /var/spool/slurmctld /var/spool/slurmd /var/log/slurm"
            ),
            check=False,
        )

    print_info("Starting munge on all nodes")
    for node in nodes:
        _docker_exec(node, "munged --force", check=False)

    if "arts-node-1" in nodes:
        print_info("Starting slurmctld on arts-node-1")
        _docker_exec("arts-node-1", "slurmctld", check=False)

    time.sleep(1)
    print_info("Starting slurmd on all nodes")
    for node in nodes:
        _docker_exec(node, "slurmd", check=False)

    time.sleep(2)
    sinfo = _docker("exec", "arts-node-1", "sinfo", capture_output=True, check=False)
    if sinfo.returncode == 0 and sinfo.stdout and sinfo.stdout.strip():
        print_info("Slurm cluster status:")
        print(sinfo.stdout.strip())
    else:
        print_warning("sinfo failed; Slurm may still be initializing")


def _kill_container_processes(container: str) -> None:
    _docker_exec(
        container,
        textwrap.dedent(
            """
            ps aux | grep -v -E '(ps|grep|sleep|bash|root|PID)' | awk '{print $2}' | xargs -r kill -9 2>/dev/null || true
            pkill -9 -f 'arts' 2>/dev/null || true
            pkill -9 -f 'carts' 2>/dev/null || true
            pkill -9 slurmd 2>/dev/null || true
            pkill -9 slurmctld 2>/dev/null || true
            pkill -9 munged 2>/dev/null || true
            ps aux | grep -E '<defunct>' | awk '{print $2}' | xargs -r kill -9 2>/dev/null || true
            """
        ).strip(),
        check=False,
    )
