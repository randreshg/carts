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

def run_command(cmd, cwd=None, check=True):
    """Run a command and return the result."""
    print(f"Running: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    try:
        result = subprocess.run(cmd, shell=isinstance(cmd, str), cwd=cwd, 
                              check=check, capture_output=True, text=True)
        if result.stdout:
            print(result.stdout)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"Error: {e}")
        if e.stderr:
            print(f"stderr: {e.stderr}")
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
    
    print("Setting up CARTS project...")
    print(f"Project root: {project_root}")
    
    # Create necessary directories
    dirs_to_create = ['.install', 'build', 'external']
    for dir_name in dirs_to_create:
        (project_root / dir_name).mkdir(exist_ok=True)
    
    # Download dependencies if they don't exist
    arts_dir = project_root / 'external' / 'arts'
    polygeist_dir = project_root / 'external' / 'Polygeist'
    
    if not arts_dir.exists():
        print("Downloading ARTS runtime...")
        if not run_command(f"git clone --recursive https://github.com/randreshg/ARTS.git {arts_dir}"):
            print("Failed to download ARTS runtime")
            return False
    else:
        print("ARTS runtime already exists. Skipping download.")
    
    if not polygeist_dir.exists():
        print("Downloading Polygeist...")
        if not run_command(f"git clone --branch carts --recursive https://github.com/randreshg/Polygeist.git {polygeist_dir}"):
            print("Failed to download Polygeist")
            return False
    else:
        print("Polygeist already exists. Skipping download.")
    
    return True

def build_project():
    """Build and install the CARTS project using the Makefile."""
    project_root = Path(__file__).resolve().parent.parent.parent
    
    print("Building and installing CARTS project...")
    
    # This assumes the main Makefile has an 'install' target that handles
    # all necessary dependencies and build steps.
    if not run_command("make all", cwd=project_root):
        print("Failed to build and install project")
        return False
    
    return True

def add_to_path():
    """Add CARTS to the user's PATH by adding the carts script location."""
    project_root = Path(__file__).resolve().parent.parent.parent
    carts_script_dir = project_root / 'tools'
    
    if not (carts_script_dir / 'carts').exists():
        print("Error: CARTS script not found. Please ensure you're in the correct directory.")
        return False
    
    # Check if carts is already in current PATH
    if check_command_exists('carts'):
        print("carts command is already available in your PATH")
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
        print("Unsupported shell. Please manually add to your shell profile:")
        print(f"export PATH=\"{carts_script_dir}:$PATH\"")
        return False
    
    # Check if already added to shell profile
    if shell_profile.exists():
        with open(shell_profile, 'r') as f:
            content = f.read()
            # Check for exact path match
            if str(carts_script_dir) in content:
                print("CARTS is already configured in your shell profile")
                print("Try: source ~/.zshrc (or restart your terminal)")
                return True
    
    # Add to shell profile
    with open(shell_profile, 'a') as f:
        f.write(f"\n# CARTS\nexport PATH=\"{carts_script_dir}:$PATH\"\n")
    
    print(f"Added CARTS to {shell_profile}")
    print("Run 'source ~/.zshrc' or restart your terminal to use 'carts'")
    return True



def main():
    parser = argparse.ArgumentParser(description="CARTS Setup Script")
    parser.add_argument("--skip-build", action="store_true", help="Skip project building")
    parser.add_argument("--add-to-path", action="store_true", help="Add CARTS to PATH (no dependency installation)")
    
    args = parser.parse_args()
    
    print("=== CARTS Setup Script ===")
    
    # Handle add-to-path first (no dependency installation)
    if args.add_to_path:
        if add_to_path():
            print("CARTS added to PATH successfully!")
            print("You can now use 'carts' from anywhere")
        else:
            sys.exit(1)
        return
    
    if not args.skip_build:
        if not setup_project():
            print("Failed to set up project")
            sys.exit(1)
        
        if not build_project():
            print("Failed to build project")
            sys.exit(1)
    
    # Add to PATH automatically after successful build
    if add_to_path():
        print("CARTS added to PATH automatically!")
    
    print("\n=== Setup Complete! ===")
    print("To use CARTS:")
    print("1. The 'carts' command is now available in your PATH")
    print("2. Use the carts wrapper: carts --help")
    print("3. Run benchmarks: carts benchmark --help")
    print("4. Launch ArtsUI: carts ui")
    print("")
    print("Note: If you restart your terminal, run: python3 tools/setup/carts-setup.py --add-to-path")

if __name__ == "__main__":
    main() 