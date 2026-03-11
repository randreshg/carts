"""Update command for Docker CLI."""

from __future__ import annotations

import os
import textwrap
import time

import typer

from carts_styles import print_error, print_header, print_info, print_step, print_success, print_warning
from docker_shared import BASE_IMAGE, WORKSPACE_VOLUME, _detect_docker_cpus, _docker, _docker_exec, _get_docker_dir, _image_exists, _volume_exists


def docker_update(
    arts: bool = typer.Option(False, "--arts", "-a", help="Rebuild ARTS"),
    polygeist: bool = typer.Option(False, "--polygeist", "-p", help="Rebuild Polygeist"),
    llvm: bool = typer.Option(False, "--llvm", "-l", help="Rebuild LLVM"),
    carts_rebuild: bool = typer.Option(False, "--carts", "-c", help="Rebuild CARTS"),
    force: bool = typer.Option(False, "--force", "-f", help="Force rebuild"),
    debug_level: int = typer.Option(
        0, "--debug", min=0, max=3,
        help="ARTS runtime log level: 0=off, 1=warn, 2=info, 3=debug"),
):
    """Update Docker workspace and selectively rebuild components."""
    _get_docker_dir()

    if not _image_exists(BASE_IMAGE):
        print_error(f"Required image '{BASE_IMAGE}' not found")
        print_warning("Run 'carts docker build' first")
        raise typer.Exit(1)

    print_header("CARTS Docker Update")
    print_info("Updating CARTS workspace inside Docker")

    if not _volume_exists(WORKSPACE_VOLUME):
        print_warning(f"Workspace volume '{WORKSPACE_VOLUME}' not found; creating it")
        _docker("volume", "create", WORKSPACE_VOLUME, check=False)

    docker_cpus = _detect_docker_cpus()
    _docker("rm", "-f", "arts-node-update", check=False)

    result = _docker(
        "run",
        "-d",
        "--name",
        "arts-node-update",
        "--hostname",
        "arts-node-update",
        "--network",
        "bridge",
        f"--cpus={docker_cpus}",
        "-v",
        f"{WORKSPACE_VOLUME}:/opt/carts",
        BASE_IMAGE,
        check=False,
    )
    if result.returncode != 0:
        print_error("Failed to start update container")
        raise typer.Exit(result.returncode)

    try:
        print_info("Waiting for update container")
        time.sleep(5)

        git_branch = os.environ.get("GIT_BRANCH", "v2")
        print_step("Updating git state and checking component changes")

        update_flags: list[str] = []
        if force:
            update_flags.append("--force")
        if arts:
            update_flags.append("--arts")
        if polygeist or llvm:
            update_flags.append("--polygeist")
        update_cmd = f"carts update {' '.join(update_flags)}"

        update_script = textwrap.dedent(
            f"""
            set -e
            cd /opt/carts

            git config --global --add safe.directory /opt/carts || true

            target_branch="{git_branch}"
            if [[ -z "$target_branch" ]]; then
                target_branch="v2"
            fi

            echo "CARTS:remote $(git config --get remote.origin.url 2>/dev/null || echo unknown)"
            echo "CARTS:branch $target_branch"

            # Older Docker workspace volumes were cloned as single-branch
            # checkouts and only tracked mlir. Broaden the refspec before
            # fetching so those workspaces can migrate cleanly to v2.
            git config --replace-all remote.origin.fetch '+refs/heads/*:refs/remotes/origin/*'

            if ! git fetch origin --prune; then
                echo "CARTS:fetch_failed"
            fi
            if git show-ref --verify --quiet "refs/heads/$target_branch"; then
                git checkout "$target_branch" || true
            else
                git checkout -B "$target_branch" "origin/$target_branch" || true
            fi

            local_before=$(git rev-parse HEAD 2>/dev/null || echo "")
            remote_sha=$(git rev-parse "origin/$target_branch" 2>/dev/null || echo "")
            if [[ -n "$local_before" && -n "$remote_sha" && "$local_before" != "$remote_sha" ]]; then
                echo "CARTS:pulling $local_before -> $remote_sha"
            else
                [[ -n "$remote_sha" ]] && echo "CARTS:up_to_date $remote_sha"
            fi

            carts_changed=0
            git reset --hard "origin/$target_branch" || true
            local_after_reset=$(git rev-parse HEAD 2>/dev/null || echo "")
            if [[ -n "$local_before" && -n "$local_after_reset" && "$local_before" != "$local_after_reset" ]]; then
                echo "CARTS:changed"
                carts_changed=1
            fi

            head_before_update=$(git rev-parse HEAD 2>/dev/null || echo "")
            {update_cmd}
            head_after_update=$(git rev-parse HEAD 2>/dev/null || echo "")
            if [[ -n "$head_before_update" && -n "$head_after_update" && "$head_before_update" != "$head_after_update" && "$carts_changed" -eq 0 ]]; then
                echo "CARTS:changed"
            fi
            """
        ).strip()

        submodule = _docker_exec("arts-node-update", update_script, capture_output=True, check=False)
        if submodule.returncode != 0:
            print_error("Failed to update repository state inside container")
            if submodule.stderr:
                print_error(submodule.stderr.strip())
            raise typer.Exit(submodule.returncode)

        output_lines = [line.strip() for line in (submodule.stdout or "").splitlines() if line.strip()]
        for line in output_lines:
            if line.startswith("CARTS:"):
                print_info(line)

        carts_changed = any(
            line.startswith("CARTS:changed") or line.startswith("CARTS:pulling")
            for line in output_lines
        )
        build_llvm = llvm
        build_poly = polygeist or llvm
        build_arts = arts
        build_carts = carts_rebuild or carts_changed

        if force:
            build_llvm = True
            build_poly = True
            build_arts = True
            build_carts = True

        if not any([build_llvm, build_poly, build_arts, build_carts]):
            print_success("No rebuild required")
            return

        print_step("Rebuilding changed components")
        arts_mode_arg = ""
        if build_arts:
            arts_mode_arg = f" --debug {debug_level}"

        rebuild_cmd = textwrap.dedent(
            f"""
            set -e
            cd /opt/carts
            export MAKEFLAGS='-j{docker_cpus}'
            if [[ "{str(build_llvm).lower()}" == "true" ]]; then carts build --llvm; fi
            if [[ "{str(build_poly).lower()}" == "true" ]]; then carts build --polygeist; fi
            if [[ "{str(build_arts).lower()}" == "true" ]]; then carts build --arts{arts_mode_arg}; fi
            if [[ "{str(build_llvm or build_poly or build_arts or build_carts).lower()}" == "true" ]]; then carts build; fi
            """
        ).strip()

        rebuild = _docker_exec("arts-node-update", rebuild_cmd, check=False)
        if rebuild.returncode != 0:
            print_error("Docker update build failed")
            raise typer.Exit(rebuild.returncode)

    finally:
        print_step("Cleaning up update container")
        _docker("stop", "arts-node-update", check=False)
        _docker("rm", "arts-node-update", check=False)

    print_header("Update Complete")
    print_success("Docker workspace updated")
