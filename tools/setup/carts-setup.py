#!/usr/bin/env python3
"""
CARTS Setup Script
Automatically installs all dependencies and sets up the CARTS environment.
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

# Import shared styles
sys.path.insert(0, str(Path(__file__).parent.parent.resolve()))
from carts_styles import (
    print_header,
    print_step,
    print_success,
    print_error,
    print_warning,
    print_info,
)


def run_command(cmd, cwd=None, check=True, realtime=False):
    """Run a command and return the result."""
    print_info(f"Running: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    sys.stdout.flush()

    if realtime:
        # For long-running commands like git clone, show output in real-time
        try:
            process = subprocess.Popen(cmd, shell=isinstance(cmd, str), cwd=cwd,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, bufsize=1, universal_newlines=True)

            # Stream output line by line
            for line in iter(process.stdout.readline, ''):
                print(line.rstrip())
                sys.stdout.flush()

            process.wait()
            return process.returncode == 0
        except Exception as e:
            print_error(f"Error: {e}")
            sys.stdout.flush()
            return False
    else:
        # For short commands, capture output as before
        try:
            result = subprocess.run(cmd, shell=isinstance(cmd, str), cwd=cwd,
                                    check=check, capture_output=True, text=True)
            if result.stdout:
                print(result.stdout)
            sys.stdout.flush()
            return result.returncode == 0
        except subprocess.CalledProcessError as e:
            print_error(f"Error: {e}")
            if e.stderr:
                print_error(f"stderr: {e.stderr}")
            sys.stdout.flush()
            return False


def check_command_exists(cmd):
    """Check if a command exists in PATH."""
    try:
        subprocess.run([cmd, '--version'], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def setup_project():
    """Set up the CARTS project."""
    project_root = Path(__file__).resolve().parent.parent.parent

    print_info("Setting up CARTS project...")
    print_info(f"Project root: {project_root}")

    # Create necessary directories
    dirs_to_create = ['.install', 'build', 'external']
    for dir_name in dirs_to_create:
        (project_root / dir_name).mkdir(exist_ok=True)

    # Initialize and update submodules to latest commits
    print_info("Initializing and updating git submodules to latest commits...")
    # Configure git to use HTTPS instead of SSH for GitHub
    if not run_command("git submodule update --init --recursive", cwd=project_root, realtime=True):
        print_error("Failed to initialize/update submodules")
        return False

    return True


def build_project():
    """Build and install the CARTS project using the correct build order."""
    project_root = Path(__file__).resolve().parent.parent.parent

    print_info("Building and installing CARTS project...")
    print_info("Following correct build order:")

    # First, add carts to PATH (handle Docker environment)
    print_step("Adding CARTS to PATH...", step_num=0, total=5)
    carts_script_dir = project_root / 'tools'

    # In Docker, we can directly export PATH for this session
    import os
    current_path = os.environ.get('PATH', '')
    if str(carts_script_dir) not in current_path:
        os.environ['PATH'] = f"{carts_script_dir}:{current_path}"
        print_success(f"Added {carts_script_dir} to PATH for this session")

    # Also add to shell profiles for future sessions
    add_to_path()

    # Step 1: Build LLVM first
    print_step("Building LLVM...", step_num=1, total=5)
    if not run_command("carts build --llvm", cwd=project_root, realtime=True):
        print_error("Failed to build LLVM")
        return False

    # Step 2: Build Polygeist (depends on LLVM)
    print_step("Building Polygeist...", step_num=2, total=5)
    if not run_command("carts build --polygeist", cwd=project_root, realtime=True):
        print_error("Failed to build Polygeist")
        return False

    # Step 3: Build ARTS runtime
    print_step("Building ARTS runtime...", step_num=3, total=5)
    if not run_command("carts build --arts", cwd=project_root, realtime=True):
        print_error("Failed to build ARTS runtime")
        return False

    # Step 4: Build CARTS (depends on everything above)
    print_step("Building CARTS...", step_num=4, total=5)
    if not run_command("carts build", cwd=project_root, realtime=True):
        print_error("Failed to build CARTS")
        return False

    print_success("All build steps completed successfully!")
    return True


def add_to_path():
    """Add CARTS to the user's PATH by adding the carts script location."""
    project_root = Path(__file__).resolve().parent.parent.parent
    carts_script_dir = project_root / 'tools'

    if not (carts_script_dir / 'carts').exists():
        print_error("CARTS script not found. Please ensure you're in the correct directory.")
        return False

    # Check if carts is already in current PATH
    if check_command_exists('carts'):
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
        with open(shell_profile, 'r') as f:
            content = f.read()
            # Check for exact path match
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


def main():
    parser = argparse.ArgumentParser(description="CARTS Setup Script")
    parser.add_argument("--skip-build", action="store_true",
                        help="Skip project building")
    parser.add_argument("--add-to-path", action="store_true",
                        help="Add CARTS to PATH (no dependency installation)")

    args = parser.parse_args()

    print_header("CARTS Setup Script")
    sys.stdout.flush()

    # Handle add-to-path first (no dependency installation)
    if args.add_to_path:
        if add_to_path():
            print_success("CARTS added to PATH successfully!")
            print_info("You can now use 'carts' from anywhere")
        else:
            sys.exit(1)
        return

    if not args.skip_build:
        if not setup_project():
            print_error("Failed to set up project")
            sys.exit(1)

        if not build_project():
            print_error("Failed to build project")
            sys.exit(1)

    print_header("Setup Complete!")

if __name__ == "__main__":
    main()
