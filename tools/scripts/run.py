"""Command execution helpers for CARTS CLI."""

from pathlib import Path
from typing import Dict, List, Optional
import os
import subprocess
import sys

from carts_styles import console, print_debug, print_error
from scripts.config import is_verbose


def run_subprocess(
    cmd: List[str],
    capture_output: bool = False,
    check: bool = True,
    cwd: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
    timeout: Optional[int] = None,
    realtime: bool = False,
) -> subprocess.CompletedProcess:
    """Run a subprocess with optional output capture.

    Args:
        timeout: Timeout in seconds (passed to subprocess.run).
        realtime: Stream stdout line-by-line instead of capturing.
    """
    if is_verbose():
        print_debug(f"Running: {' '.join(str(c) for c in cmd)}")

    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)

    if realtime:
        try:
            process = subprocess.Popen(
                cmd,
                cwd=cwd,
                env=merged_env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
            )
            for line in iter(process.stdout.readline, ''):
                print(line.rstrip())
                sys.stdout.flush()
            process.wait()
            result = subprocess.CompletedProcess(
                cmd, process.returncode, stdout="", stderr="")
            if check and result.returncode != 0:
                raise subprocess.CalledProcessError(result.returncode, cmd)
            return result
        except subprocess.CalledProcessError:
            raise
        except Exception as e:
            print_error(f"Command failed: {e}")
            raise

    try:
        result = subprocess.run(
            cmd,
            capture_output=capture_output,
            check=check,
            cwd=cwd,
            env=merged_env,
            text=True if capture_output else None,
            timeout=timeout,
        )
        return result
    except subprocess.CalledProcessError as e:
        if capture_output:
            if e.stdout:
                console.print(e.stdout)
            if e.stderr:
                console.print(e.stderr, style="red")
        raise


def run_command_with_output(
    cmd: List[str],
    output_file: Optional[Path] = None,
    cwd: Optional[Path] = None,
) -> int:
    """Run a command, optionally redirecting stdout to a file."""
    if is_verbose():
        if output_file:
            print_debug(
                f"Running: {' '.join(str(c) for c in cmd)} > {output_file}")
        else:
            print_debug(f"Running: {' '.join(str(c) for c in cmd)}")

    try:
        if output_file:
            with open(output_file, 'w') as f:
                result = subprocess.run(
                    cmd, stdout=f, stderr=subprocess.PIPE, cwd=cwd, text=True)
                if result.returncode != 0 and result.stderr:
                    console.print(result.stderr, style="red")
                return result.returncode
        else:
            result = subprocess.run(cmd, cwd=cwd)
            return result.returncode
    except FileNotFoundError:
        print_error(f"Command not found: {cmd[0]}")
        return 1
    except Exception as e:
        print_error(f"Command failed: {e}")
        return 1
