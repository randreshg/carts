#!/usr/bin/env python3
"""
CARTS Setup Script
Automatically installs all dependencies and sets up the CARTS environment.
"""

import os
import sys
import subprocess
import platform
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

def install_dependencies():
    """Install system dependencies needed for building."""
    system = platform.system().lower()
    
    if system == "darwin":  # macOS
        print("Detected macOS. Installing build dependencies...")
        
        # Check if Homebrew is installed
        if not check_command_exists('brew'):
            print("Installing Homebrew...")
            install_script = '/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
            if not run_command(install_script):
                print("Failed to install Homebrew. Please install it manually.")
                return False
        
        # Install build tools (LLVM/MLIR will be built locally)
        packages = ['cmake', 'ninja', 'libomp']
        for package in packages:
            print(f"Installing {package}...")
            if not run_command(f"brew install {package}"):
                print(f"Failed to install {package}")
                return False
                
    elif system == "linux":
        print("Detected Linux. Installing build dependencies...")
        
        # Try to detect package manager
        if check_command_exists('apt'):
            packages = ['cmake', 'ninja-build', 'clang', 'libomp-dev', 'build-essential']
            for package in packages:
                if not run_command(f"sudo apt update && sudo apt install -y {package}"):
                    print(f"Failed to install {package}")
                    return False
        elif check_command_exists('yum'):
            packages = ['cmake', 'ninja-build', 'clang', 'libomp-devel', 'gcc-c++']
            for package in packages:
                if not run_command(f"sudo yum install -y {package}"):
                    print(f"Failed to install {package}")
                    return False
        else:
            print("Unsupported Linux distribution. Please install cmake, ninja, clang, and libomp manually.")
            return False
    
    else:
        print(f"Unsupported operating system: {system}")
        return False
    
    return True

def setup_project():
    """Set up the CARTS project."""
    project_root = Path(__file__).parent.parent
    
    print("Setting up CARTS project...")
    
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
    
    if not polygeist_dir.exists():
        print("Downloading Polygeist...")
        if not run_command(f"git clone --branch carts --recursive https://github.com/randreshg/Polygeist.git {polygeist_dir}"):
            print("Failed to download Polygeist")
            return False
    
    return True

def build_project():
    """Build the CARTS project with local dependencies."""
    project_root = Path(__file__).parent.parent
    
    print("Building CARTS project...")
    
    # Build and install dependencies locally (LLVM, Polygeist, ARTS)
    print("Installing LLVM, Polygeist, and ARTS locally...")
    if not run_command("make installdeps", cwd=project_root):
        print("Failed to build dependencies")
        return False
    
    # Build main project
    print("Building CARTS compiler...")
    if not run_command("make build", cwd=project_root):
        print("Failed to build main project")
        return False
    
    return True

def create_environment_script():
    """Create an environment setup script."""
    project_root = Path(__file__).parent.parent
    env_script = project_root / 'setup_env.sh'
    
    install_dir = project_root / '.install'
    
    script_content = f"""#!/bin/bash
# CARTS Environment Setup Script
# Source this script to set up the CARTS environment

export CARTS_ROOT="{project_root}"
export PATH="{install_dir}/carts/bin:{install_dir}/polygeist/bin:{install_dir}/llvm/bin:$PATH"
export LD_LIBRARY_PATH="{install_dir}/carts/lib:{install_dir}/polygeist/lib:{install_dir}/arts/lib:{install_dir}/llvm/lib:$LD_LIBRARY_PATH"

# For macOS, also set DYLD_LIBRARY_PATH
if [[ "$OSTYPE" == "darwin"* ]]; then
    export DYLD_LIBRARY_PATH="{install_dir}/carts/lib:{install_dir}/polygeist/lib:{install_dir}/arts/lib:{install_dir}/llvm/lib:$DYLD_LIBRARY_PATH"
fi

echo "CARTS environment set up successfully!"
echo "You can now use: carts --help"
echo ""
echo "To make carts available permanently, add this line to your shell profile:"
echo "export PATH=\\"{install_dir}/carts/bin:$PATH\\""
"""
    
    with open(env_script, 'w') as f:
        f.write(script_content)
    
    os.chmod(env_script, 0o755)
    print(f"Environment script created: {env_script}")
    print("Source it with: source setup_env.sh")

def add_to_path():
    """Add CARTS to the user's PATH."""
    project_root = Path(__file__).parent.parent.parent
    install_dir = project_root / '.install'
    carts_bin = install_dir / 'carts' / 'bin'
    
    if not carts_bin.exists():
        print("Error: CARTS installation not found. Please run setup first.")
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
        print(f"export PATH=\"{carts_bin}:$PATH\"")
        return False
    
    # Check if already added to shell profile
    if shell_profile.exists():
        with open(shell_profile, 'r') as f:
            content = f.read()
            # Check for exact path match
            if str(carts_bin) in content:
                print("CARTS is already configured in your shell profile")
                print("Try: source ~/.zshrc (or restart your terminal)")
                return True
    
    # Add to shell profile
    with open(shell_profile, 'a') as f:
        f.write(f"\n# CARTS\nexport PATH=\"{carts_bin}:$PATH\"\n")
    
    print(f"Added CARTS to {shell_profile}")
    print("Run 'source ~/.zshrc' or restart your terminal to use 'carts'")
    return True



def main():
    parser = argparse.ArgumentParser(description="CARTS Setup Script")
    parser.add_argument("--skip-deps", action="store_true", help="Skip dependency installation")
    parser.add_argument("--skip-build", action="store_true", help="Skip project building")
    parser.add_argument("--deps-only", action="store_true", help="Only install dependencies")
    parser.add_argument("--add-to-path", action="store_true", help="Add CARTS to PATH (no dependency installation)")
    
    args = parser.parse_args()
    
    print("=== CARTS Setup Script ===")
    
    # Handle add-to-path first (no dependency installation)
    if args.add_to_path:
        if add_to_path():
            print("CARTS added to PATH successfully!")
        else:
            sys.exit(1)
        return
    
    if not args.skip_deps and not args.deps_only:
        print("Installing system build tools...")
        if not install_dependencies():
            print("Failed to install dependencies")
            sys.exit(1)
    
    if args.deps_only:
        print("Dependencies installed successfully!")
        return
    
    if not args.skip_build:
        if not setup_project():
            print("Failed to set up project")
            sys.exit(1)
        
        if not build_project():
            print("Failed to build project")
            sys.exit(1)
    
    create_environment_script()
    
    print("\n=== Setup Complete! ===")
    print("To use CARTS:")
    print("1. Source the environment: source setup_env.sh")
    print("2. Use the carts wrapper: carts --help")
    print("3. Run benchmarks: carts-benchmark --help")
    print("4. Generate reports: carts-report --help")
    print("5. Add to PATH: python3 tools/setup/setup.py --add-to-path")

if __name__ == "__main__":
    main() 