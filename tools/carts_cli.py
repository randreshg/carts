#!/usr/bin/env python3
"""
CARTS CLI - Compiler for Asynchronous Runtime Systems
A modern Python CLI with rich terminal output.
"""

from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional, Dict, Tuple, Set, Iterable
import platform as sys_platform
import subprocess
import os
import shutil
import sys

import typer
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TaskProgressColumn
from rich.table import Table
import json

# Import shared styles
from carts_styles import (
    Colors,
    console,
    print_header,
    print_step,
    print_success,
    print_error,
    print_warning,
    print_info,
    print_debug,
)

# ============================================================================
# Constants and Enums
# ============================================================================


class Platform(Enum):
    MACOS = "macos"
    LINUX = "linux"
    WINDOWS = "windows"


# CARTS transformation pipeline stages in execution order.
# Used by --all-stages to dump intermediate MLIR after each pass.
PIPELINE_STAGES = [
    "canonicalize-memrefs",  # Normalize memref operations
    "collect-metadata",      # Extract loop/array metadata for optimization
    "initial-cleanup",       # Remove dead code, simplify control flow
    "openmp-to-arts",        # Convert OpenMP parallel regions to ARTS EDTs
    "edt-transforms",        # Optimize EDT (Event Driven Task) structure
    "loop-reordering",       # Cache-optimal loop reorder and tiling setup
    "create-dbs",            # Create DataBlocks for inter-task communication
    "db-opt",                # Optimize DataBlock operations
    "edt-opt",               # Further EDT optimizations
    "concurrency",           # Add concurrency annotations
    "edt-distribution",      # Select distribution strategy, lower arts.for
    "concurrency-opt",       # Optimize concurrent execution
    "epochs",                # Add epoch synchronization
    "pre-lowering",          # Prepare for LLVM lowering
    "arts-to-llvm",          # Final lowering to LLVM IR
]

# Files to clean
CLEAN_FILE_PATTERNS = [
    "*.mlir", "*.ll", "*.o", "*.so", "*.dylib", "*.a", "*.exe",
    "DbGraph_*.dot", "*.log", "*.tmp", ".artsPrintLock",
    ".carts-metadata.json", "*_arts_metadata.mlir", "core",
]

CLEAN_DIR_PATTERNS = [
    "build", "cmake-build-*", ".cache", "introspection", "counter", "counters",
]

# Source files handled by `carts format`.
FORMAT_SUFFIXES: Set[str] = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc", ".td"
}
FORMAT_EXCLUDED_DIRS: Set[str] = {
    ".git", ".install", "build", "external", "third_party"
}

# ============================================================================
# Platform Configuration
# ============================================================================


@dataclass
class PlatformConfig:
    """Platform-specific configuration for CARTS compilation.

    Auto-detects platform (macOS/Linux/Windows) and configures:
    - Include paths for system headers and LLVM/ARTS libraries
    - Sysroot flags for cross-compilation support
    - Linker selection (ld64.lld on macOS, ld.lld on Linux)
    - Library paths and runtime dependencies

    Usage: config = PlatformConfig.detect()
    """

    # Platform identification
    platform: Platform
    arch: str  # e.g., "arm64", "x86_64"

    # Installation directories
    carts_dir: Path              # Root CARTS source directory
    install_dir: Path            # .install directory
    llvm_install_dir: Path       # LLVM/Clang installation
    polygeist_install_dir: Path  # Polygeist (C-to-MLIR) installation
    arts_install_dir: Path       # ARTS runtime installation
    carts_install_dir: Path      # CARTS tools installation

    # Include/library paths (auto-detected)
    llvm_include_path: Path = field(default_factory=Path)
    llvm_lib_path: Path = field(default_factory=Path)
    system_include_path: Optional[Path] = None
    system_cxx_include_path: Optional[Path] = None
    macos_sdk_path: Optional[Path] = None

    # Compiler flags (populated by _setup_*_flags methods)
    include_flags: List[str] = field(default_factory=list)        # -I flags
    cgeist_sysroot_flags: List[str] = field(
        default_factory=list)  # --sysroot for cgeist
    clang_sysroot_flags: List[str] = field(
        default_factory=list)  # -isysroot for clang
    clang_library_flags: List[str] = field(
        default_factory=list)  # -L flags for clang
    compile_library_flags: List[str] = field(
        default_factory=list)  # -L flags for final link
    clang_libraries: List[str] = field(
        default_factory=list)      # -l flags for clang
    compile_libraries: List[str] = field(
        default_factory=list)    # -l flags for final link
    compile_flags: List[str] = field(
        default_factory=list)        # General compile flags
    linker_flags: List[str] = field(
        default_factory=list)         # Linker-specific flags
    linker_path: Optional[Path] = None  # Path to LLD linker
    linker_type: Optional[str] = None   # "lld" or "lld-link"

    @classmethod
    def detect(cls, script_dir: Optional[Path] = None) -> "PlatformConfig":
        """Auto-detect platform and configure paths."""
        if script_dir is None:
            script_dir = Path(__file__).parent.resolve()

        # Determine CARTS_DIR
        if ".install/carts/bin" in str(script_dir):
            # Running from installed location
            carts_dir = script_dir.parent.parent.parent
        else:
            # Running from source location (tools/)
            carts_dir = script_dir.parent

        install_dir = carts_dir / ".install"

        config = cls(
            platform=cls._detect_platform(),
            arch=sys_platform.machine() or "unknown",
            carts_dir=carts_dir,
            install_dir=install_dir,
            llvm_install_dir=install_dir / "llvm",
            polygeist_install_dir=install_dir / "polygeist",
            arts_install_dir=install_dir / "arts",
            carts_install_dir=install_dir / "carts",
        )

        config._setup_paths()
        config._setup_platform_flags()
        config._setup_common_flags()
        config._detect_linker()

        return config

    @staticmethod
    def _detect_platform() -> Platform:
        """Detect the current platform."""
        system = sys_platform.system().lower()
        if system == "darwin":
            return Platform.MACOS
        elif system == "linux":
            return Platform.LINUX
        elif system == "windows" or "msys" in os.environ.get("OSTYPE", "").lower():
            return Platform.WINDOWS
        else:
            raise RuntimeError(f"Unsupported platform: {system}")

    def _setup_paths(self) -> None:
        """Setup include and library paths."""
        self.llvm_include_path = self.llvm_install_dir / \
            "lib" / "clang" / "18" / "include"

        # Detect LLVM library path
        aarch64_path = self.llvm_install_dir / "lib" / "aarch64-unknown-linux-gnu"
        x86_64_path = self.llvm_install_dir / "lib" / "x86_64-unknown-linux-gnu"

        if aarch64_path.is_dir():
            self.llvm_lib_path = aarch64_path
        elif x86_64_path.is_dir():
            self.llvm_lib_path = x86_64_path
        else:
            self.llvm_lib_path = self.llvm_install_dir / "lib"

        # macOS SDK path
        self.macos_sdk_path = Path(
            "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")

    def _setup_platform_flags(self) -> None:
        """Setup platform-specific flags."""
        if self.platform == Platform.MACOS:
            self._setup_macos_flags()
        elif self.platform == Platform.LINUX:
            self._setup_linux_flags()
        elif self.platform == Platform.WINDOWS:
            self._setup_windows_flags()

    def _setup_macos_flags(self) -> None:
        """Setup macOS-specific flags."""
        if self.macos_sdk_path and self.macos_sdk_path.exists():
            self.system_include_path = self.macos_sdk_path / "usr" / "include"
            self.system_cxx_include_path = self.macos_sdk_path / \
                "usr" / "include" / "c++" / "v1"

            self.include_flags.extend([
                f"-I{self.system_include_path}",
                f"-I{self.system_cxx_include_path}",
            ])

            self.cgeist_sysroot_flags = ["--sysroot", str(self.macos_sdk_path)]
            self.clang_sysroot_flags = ["-isysroot", str(self.macos_sdk_path)]

        # No-PIE flags for x86_64
        if self.arch == "x86_64":
            self.compile_flags.extend(["-fno-pie", "-Wl,-no_pie"])

    @staticmethod
    def _is_version_dir(name: str) -> bool:
        """Check if a directory name looks like a version (e.g., '14', '11.4.0')."""
        parts = name.split(".")
        return all(part.isdigit() for part in parts)

    @staticmethod
    def _parse_version(name: str) -> tuple:
        """Parse a version string into a tuple for comparison (e.g., '11.4.0' -> (11, 4, 0))."""
        return tuple(int(part) for part in name.split("."))

    def _setup_linux_flags(self) -> None:
        """Setup Linux-specific flags."""
        self.system_include_path = Path("/usr/include")

        # Detect libstdc++ include path (version directories like /usr/include/c++/14 or /usr/include/c++/11.4.0)
        cxx_base = Path("/usr/include/c++")
        if cxx_base.is_dir():
            libstdcxx_versions = [
                d for d in cxx_base.iterdir()
                if d.is_dir() and self._is_version_dir(d.name)
            ]
            if libstdcxx_versions:
                latest = max(libstdcxx_versions, key=lambda p: self._parse_version(p.name))
                self.system_cxx_include_path = latest
            else:
                self.system_cxx_include_path = None
        else:
            self.system_cxx_include_path = None

        if self.system_cxx_include_path is None:
            print_warning(
                "Could not detect libstdc++ include path. "
                "C++ compilation may fail. Ensure libstdc++ is installed "
                "(e.g., 'apt install libstdc++-dev' or 'dnf install libstdc++-devel')."
            )

        self.include_flags.extend([
            f"-I{self.system_include_path}",
            f"-I{self.system_cxx_include_path}",
        ] if self.system_cxx_include_path else [f"-I{self.system_include_path}"])

        self.clang_library_flags.extend(["-L/usr/lib64", "-L/usr/lib"])
        self.compile_library_flags.extend(["-L/usr/lib64", "-L/usr/lib"])
        self.clang_libraries.extend(["-lpthread", "-lrt", "-lstdc++"])
        self.compile_libraries.extend(["-lpthread", "-lrt", "-lstdc++"])
        self.compile_flags.extend(["-fno-pie", "-no-pie"])

    def _setup_windows_flags(self) -> None:
        """Setup Windows-specific flags."""
        sdk_dir = os.environ.get(
            "WindowsSdkDir", os.environ.get("WINDOWSSDKDIR", ""))
        sdk_version = os.environ.get(
            "WindowsSDKVersion", os.environ.get("WINDOWSSDKVERSION", ""))
        vc_tools_dir = os.environ.get("VCToolsInstallDir", "")
        arch = os.environ.get("CARTS_WINDOWS_ARCH", "x64")

        if not sdk_dir or not sdk_version:
            raise RuntimeError(
                "Windows SDK environment not detected. Run from VS developer prompt.")
        if not vc_tools_dir:
            raise RuntimeError(
                "VCToolsInstallDir not set. Run from VS developer prompt.")

        sdk_dir = sdk_dir.rstrip("/\\")
        sdk_version = sdk_version.rstrip("/\\")
        vc_tools_dir = vc_tools_dir.rstrip("/\\")

        self.include_flags.extend([
            f"-I{sdk_dir}/Include/{sdk_version}/ucrt",
            f"-I{sdk_dir}/Include/{sdk_version}/shared",
            f"-I{sdk_dir}/Include/{sdk_version}/um",
            f"-I{vc_tools_dir}/include",
        ])

        self.clang_library_flags.extend([
            f"-L{sdk_dir}/Lib/{sdk_version}/ucrt/{arch}",
            f"-L{sdk_dir}/Lib/{sdk_version}/um/{arch}",
            f"-L{vc_tools_dir}/lib/{arch}",
        ])
        self.compile_library_flags.extend(self.clang_library_flags)

    def _setup_common_flags(self) -> None:
        """Setup common flags for all platforms."""
        # Include flags
        self.include_flags.insert(0, f"-I{self.llvm_include_path}")
        self.include_flags.extend([
            f"-I{self.carts_install_dir}/include",
            f"-I{self.arts_install_dir}/include",
            f"-I{self.llvm_install_dir}/include",
        ])

        # Library flags
        base_lib_flags = [
            f"-L{self.carts_install_dir}/lib",
            f"-L{self.carts_install_dir}/lib64",
            f"-L{self.arts_install_dir}/lib",
            f"-L{self.arts_install_dir}/lib64",
            f"-L{self.llvm_lib_path}",
            f"-L{self.llvm_install_dir}/lib",
            f"-L{self.llvm_install_dir}/lib64",
        ]
        self.clang_library_flags = base_lib_flags + self.clang_library_flags
        self.compile_library_flags = base_lib_flags + self.compile_library_flags

        # Libraries
        base_libs = ["-lc++", "-lc++abi", "-lunwind"]
        self.clang_libraries = base_libs + self.clang_libraries
        self.compile_libraries = [
            "-lcartstest", "-lcartsbenchmarks", "-larts", "-lomp",
        ] + base_libs + [lib for lib in self.compile_libraries if lib not in base_libs]

        # Compile flags
        self.compile_flags.insert(0, "-O3")

    def _detect_linker(self) -> None:
        """Detect and configure the appropriate linker."""
        linker_candidates = []

        if self.platform == Platform.MACOS:
            linker_candidates = [
                self.llvm_install_dir / "bin" / "ld64.lld",
                self.llvm_install_dir / "bin" / "lld",
            ]
        elif self.platform == Platform.LINUX:
            linker_candidates = [
                self.llvm_install_dir / "bin" / "ld.lld",
                self.llvm_install_dir / "bin" / "lld",
            ]
        elif self.platform == Platform.WINDOWS:
            linker_candidates = [
                self.llvm_install_dir / "bin" / "lld-link.exe",
                self.llvm_install_dir / "bin" / "lld-link",
            ]

        for candidate in linker_candidates:
            if candidate.is_file():
                self.linker_path = candidate
                self.linker_type = "lld-link" if "lld-link" in candidate.name else "lld"
                break

        if self.linker_path:
            if self.platform == Platform.WINDOWS:
                self.linker_flags.extend(
                    ["-fuse-ld=lld", f"--ld-path={self.linker_path}"])
            else:
                self.linker_flags.append(f"--ld-path={self.linker_path}")

        # Add rpaths for ELF/Mach-O targets
        if self.platform in (Platform.LINUX, Platform.MACOS):
            if self.platform == Platform.LINUX:
                self.linker_flags.append("-Wl,--disable-new-dtags")

            rpaths = [
                self.carts_install_dir / "lib",
                self.arts_install_dir / "lib",
                self.llvm_install_dir / "lib",
                self.carts_install_dir / "lib64",
                self.arts_install_dir / "lib64",
                self.llvm_install_dir / "lib64",
                self.llvm_lib_path,
            ]

            existing = set()
            for rp in rpaths:
                rpath_flag = f"-Wl,-rpath,{rp}"
                if rpath_flag not in existing:
                    self.linker_flags.append(rpath_flag)
                    existing.add(rpath_flag)


# ============================================================================
# Global State
# ============================================================================

_config: Optional[PlatformConfig] = None
_verbose: bool = False


def get_config() -> PlatformConfig:
    """Get or create the platform configuration."""
    global _config
    if _config is None:
        _config = PlatformConfig.detect()
    return _config


def set_verbose(verbose: bool) -> None:
    """Set verbose mode."""
    global _verbose
    _verbose = verbose


def is_verbose() -> bool:
    """Check if verbose mode is enabled."""
    return _verbose


# ============================================================================
# Command Execution Helpers
# ============================================================================

def run_subprocess(
    cmd: List[str],
    capture_output: bool = False,
    check: bool = True,
    cwd: Optional[Path] = None,
    env: Optional[Dict[str, str]] = None,
) -> subprocess.CompletedProcess:
    """Run a subprocess with optional output capture."""
    if is_verbose():
        print_debug(f"Running: {' '.join(str(c) for c in cmd)}")

    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)

    try:
        result = subprocess.run(
            cmd,
            capture_output=capture_output,
            check=check,
            cwd=cwd,
            env=merged_env,
            text=True if capture_output else None,
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
    except FileNotFoundError as e:
        print_error(f"Command not found: {cmd[0]}")
        return 1
    except Exception as e:
        print_error(f"Command failed: {e}")
        return 1


# ============================================================================
# CLI Application
# ============================================================================

app = typer.Typer(
    name="carts",
    help="CARTS - Compiler for Asynchronous Runtime Systems",
    add_completion=False,
    no_args_is_help=True,
)

# Docker subcommand group
docker_app = typer.Typer(help="Docker operations for multi-node execution")
app.add_typer(docker_app, name="docker")


# ============================================================================
# Command Callbacks
# ============================================================================

@app.callback(invoke_without_command=True)
def main_callback(
    ctx: typer.Context,
    verbose: bool = typer.Option(
        False, "--verbose", "-v", help="Enable verbose output"),
    help_flag: bool = typer.Option(
        False, "--help", "-h", is_eager=True, help="Show help and exit"),
):
    """CARTS - Compiler for Asynchronous Runtime Systems"""
    # Handle -h/--help globally
    if help_flag:
        if ctx.invoked_subcommand is None:
            typer.echo(ctx.get_help())
            raise typer.Exit()

    set_verbose(verbose)
    if verbose:
        os.environ["CARTS_VERBOSE"] = "1"


# ============================================================================
# Build Command
# ============================================================================

# Counter profile mapping: level -> config file name (in carts/config/)
COUNTER_PROFILES = {
    0: "profile-none.cfg",       # All OFF - baseline performance
    1: "profile-timing.cfg",     # Timing only - minimal overhead (DEFAULT)
    2: "profile-workload.cfg",   # Workload characterization at CLUSTER level
    3: "profile-overhead.cfg",   # Full overhead analysis at CLUSTER level
}


@app.command()
def build(
    clean: bool = typer.Option(False, "--clean", "-c", help="Clean build"),
    arts: bool = typer.Option(False, "--arts", "-a", help="Build only ARTS"),
    polygeist: bool = typer.Option(
        False, "--polygeist", "-p", help="Build only Polygeist"),
    llvm: bool = typer.Option(False, "--llvm", "-l", help="Build only LLVM"),
    debug_level: int = typer.Option(
        0, "--debug",
        help="Debug level: 0=off (default), 1=info, 2=full debug"),
    counters_level: int = typer.Option(
        0, "--counters",
        help="Counter profile: 0=off (default), 1=timing, 2=workload, 3=overhead"),
    profile: Optional[Path] = typer.Option(
        None, "--profile",
        help="Custom counter profile file path (overrides --counters)"),
):
    """Build CARTS project using system clang."""
    config = get_config()

    print_header("CARTS Build")
    console.print(
        f"Platform: [{Colors.INFO}]{config.platform.value}[/{Colors.INFO}] ({config.arch})")

    makefile = config.carts_dir / "Makefile"
    if not makefile.is_file():
        print_error("Makefile not found. Are you in the CARTS project root?")
        raise typer.Exit(1)

    # Determine build target
    if arts:
        target = "arts"
    elif polygeist:
        target = "polygeist"
    elif llvm:
        target = "llvm"
    else:
        target = "build"

    console.print(f"Target: [{Colors.INFO}]{target}[/{Colors.INFO}]")

    # Build make variables
    make_vars = []

    # Debug levels: 0=off, 1=info, 2=full debug
    if debug_level >= 2:
        make_vars.extend([
            "ARTS_DEBUG_ENABLED=ON",
            "ARTS_INFO_ENABLED=ON",
            "ARTS_BUILD_TYPE=Debug"
        ])
    elif debug_level >= 1:
        make_vars.append("ARTS_INFO_ENABLED=ON")

    # Counter levels: 0=off, 1=artsid, 2=deep
    # Levels 1+ require USE_COUNTERS and USE_METRICS
    if counters_level >= 1:
        make_vars.extend(["ARTS_USE_COUNTERS=ON", "ARTS_USE_METRICS=ON"])

    # Always pass counter config path for ARTS builds
    if arts:
        if profile:
            # Use custom profile if provided
            if not profile.exists():
                print_error(f"Profile not found: {profile}")
                raise typer.Exit(1)
            effective_counter_config = profile.resolve()
            console.print(f"Profile: [{Colors.INFO}]{profile}[/{Colors.INFO}]")
        else:
            # Use profile based on counters_level (profiles are in carts/config/)
            profile_name = COUNTER_PROFILES.get(
                counters_level, "profile-timing.cfg")
            effective_counter_config = config.carts_dir / "configs" / profile_name
            console.print(f"Counter profile: [{Colors.INFO}]{profile_name}[/{Colors.INFO}]")
        make_vars.append(f"COUNTER_CONFIG_PATH={effective_counter_config}")

    # Pass platform-specific linker path to make
    if config.linker_path:
        make_vars.append(f'CARTS_LINKER_PATH={config.linker_path}')

    if make_vars:
        console.print(f"Options: [dim]{' '.join(make_vars)}[/dim]")

    console.print()

    # Clean if requested
    if clean:
        print_step("Cleaning previous build...")
        run_subprocess(["make", "clean"], cwd=config.carts_dir, check=False)

    # Run build
    print_step(f"Building {target}...")
    cmd = ["make"] + make_vars + [target]

    result = run_subprocess(cmd, cwd=config.carts_dir, check=False)

    # If polygeist was built, also rebuild carts-compile
    if result.returncode == 0 and target == "polygeist":
        print_step("Rebuilding carts-compile after Polygeist update...")
        cmd = ["make"] + make_vars + ["carts-compile-only"]
        result = run_subprocess(cmd, cwd=config.carts_dir, check=False)

    if result.returncode == 0:
        print_header("Build Complete")
        print_success("CARTS build completed successfully!")
    else:
        print_header("Build Failed")
        print_error("CARTS build failed!")
        raise typer.Exit(1)


# ============================================================================
# Cgeist Command
# ============================================================================

@app.command(context_settings={"allow_extra_args": True, "allow_interspersed_args": False})
def cgeist(
    ctx: typer.Context,
    input_file: Path = typer.Argument(..., help="Input C/C++ file"),
):
    """Run cgeist with automatic include paths."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    cgeist_bin = config.polygeist_install_dir / "bin" / "cgeist"
    if not cgeist_bin.is_file():
        print_error(f"cgeist not found at {cgeist_bin}")
        raise typer.Exit(1)

    # Build command
    cmd = [str(cgeist_bin)]
    cmd.extend(config.include_flags)
    cmd.extend(config.cgeist_sysroot_flags)
    cmd.append("--raise-scf-to-affine")

    # Filter out --arts-config (not supported by cgeist) and pass through rest
    if ctx.args:
        skip_next = False
        for arg in ctx.args:
            if skip_next:
                skip_next = False
                continue
            if arg == "--arts-config":
                skip_next = True
                continue
            cmd.append(arg)

    cmd.append(str(input_file))

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# ============================================================================
# Clang Command
# ============================================================================

@app.command(context_settings={"allow_extra_args": True, "allow_interspersed_args": False})
def clang(
    ctx: typer.Context,
    input_file: Path = typer.Argument(..., help="Input file"),
    output: Optional[Path] = typer.Option(None, "-o", help="Output file"),
):
    """Compile C/C++ with LLVM clang and OpenMP."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    clang_bin = config.llvm_install_dir / "bin" / "clang"
    if not clang_bin.is_file():
        print_error(f"LLVM clang not found at {clang_bin}")
        raise typer.Exit(1)

    # Build command
    cmd = [str(clang_bin)]
    cmd.extend(config.include_flags)
    cmd.extend(config.clang_sysroot_flags)
    cmd.extend(config.clang_library_flags)
    cmd.extend(config.linker_flags)
    cmd.append("-fopenmp")
    cmd.append(str(input_file))
    cmd.extend(config.clang_libraries)

    if output:
        cmd.extend(["-o", str(output)])

    # Pass through extra args (e.g., -O3, -lm, etc.)
    if ctx.args:
        cmd.extend(ctx.args)

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# ============================================================================
# MLIR-Translate Command
# ============================================================================

@app.command(name="mlir-translate")
def mlir_translate(
    args: List[str] = typer.Argument(..., help="Arguments for mlir-translate"),
):
    """Run mlir-translate."""
    config = get_config()

    mlir_translate_bin = config.llvm_install_dir / "bin" / "mlir-translate"
    cmd = [str(mlir_translate_bin)] + args

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# ============================================================================
# Compile Command (unified)
# ============================================================================

@app.command(
    name="compile",
    context_settings={
        "allow_extra_args": True,
        "allow_interspersed_args": False,
        "ignore_unknown_options": True,
    },
)
def compile_cmd(
    ctx: typer.Context,
    input_file: Path = typer.Argument(..., help="Input file (.c/.cpp/.mlir/.ll)"),
    output: Optional[Path] = typer.Option(
        None, "-o", help="Output file or directory"),
    optimize: bool = typer.Option(
        False, "-O3", help="Enable O3 optimizations"),
    debug: bool = typer.Option(False, "-g", help="Generate debug symbols"),
    pipeline: Optional[str] = typer.Option(
        None, "--pipeline",
        help="Stop after pipeline stage (e.g., concurrency, edt-distribution)"),
    all_stages: bool = typer.Option(
        False, "--all-stages", help="Dump all pipeline stages"),
    emit_llvm: bool = typer.Option(
        False, "--emit-llvm", help="Emit LLVM IR output"),
    collect_metadata: bool = typer.Option(
        False, "--collect-metadata", help="Collect and export metadata"),
    partition_fallback: Optional[str] = typer.Option(
        None, "--partition-fallback",
        help="Partition fallback strategy (coarse, fine)"),
    diagnose: bool = typer.Option(
        False, "--diagnose", help="Export diagnostic information"),
    diagnose_output: Optional[Path] = typer.Option(
        None, "--diagnose-output", help="Output file for diagnostics"),
    run_args: Optional[str] = typer.Option(
        None, "--run-args", help="Extra arguments for carts-compile binary"),
    compile_args: Optional[str] = typer.Option(
        None, "--compile-args", help="Extra arguments for link step"),
):
    """Unified compilation command.

    Routes based on input file type:

      .c/.cpp  Full pipeline: C → MLIR → LLVM IR → executable

      .mlir    MLIR transformation pipeline via carts-compile binary

      .ll      Link LLVM IR with ARTS runtime → executable

    Use --pipeline <stage> to stop after a specific pipeline stage.
    """
    ext = input_file.suffix.lower()
    if ext in (".c", ".cpp"):
        _compile_from_c(ctx, input_file, output, optimize, debug, pipeline,
                        all_stages, emit_llvm, collect_metadata,
                        partition_fallback, diagnose, diagnose_output,
                        run_args, compile_args)
    elif ext == ".mlir":
        _compile_from_mlir(ctx, input_file, output, optimize, debug, pipeline,
                           all_stages, emit_llvm, collect_metadata)
    elif ext == ".ll":
        if pipeline:
            print_error("--pipeline is not supported for .ll input (link-only)")
            raise typer.Exit(1)
        _compile_from_ll(ctx, input_file, output, debug)
    else:
        print_error(f"Unsupported file type: {ext}")
        print_info("Supported types: .c, .cpp, .mlir, .ll")
        raise typer.Exit(1)


# -----------------------------------------------------------------------------
# Compile: .c/.cpp input (full pipeline)
# -----------------------------------------------------------------------------

def _compile_from_c(
    ctx: typer.Context,
    input_file: Path,
    output: Optional[Path],
    optimize: bool,
    debug: bool,
    pipeline: Optional[str],
    all_stages: bool,
    emit_llvm: bool,
    collect_metadata: bool,
    partition_fallback: Optional[str],
    diagnose: bool,
    diagnose_output: Optional[Path],
    run_args_str: Optional[str],
    compile_args_str: Optional[str],
) -> None:
    """Full pipeline for C/C++ input."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    base_name = input_file.stem
    output_name = output if output else Path(f"{base_name}_arts")

    # Parse extra args
    extra_pipeline_args = run_args_str.split() if run_args_str else []
    extra_link_args = compile_args_str.split() if compile_args_str else []
    if partition_fallback:
        extra_pipeline_args.append(f"--partition-fallback={partition_fallback}")

    # Separate passthrough args into cgeist vs pipeline categories
    cgeist_args: List[str] = []
    pipeline_passthrough_args: List[str] = []

    # Args that should go to carts-compile binary, not cgeist
    pipeline_only_args = {
        "--arts-config",
        "--metadata-file",
        "--concurrency",
        "--concurrency-opt",
        "--diagnose",
        "--diagnose-output",
        "--debug-only",
        # Loop transforms (carts-compile flags)
        "--loop-transforms-enable-matmul",
        "--loop-transforms-enable-tiling",
        "--loop-transforms-tile-j",
        "--loop-transforms-min-trip-count",
        # Partitioning options
        "--partition-fallback",
        "--distributed-db-ownership",
        "--serial-edtify",
    }

    # Args with a separate value (not =)
    value_args = {"--arts-config", "--metadata-file", "--debug-only"}

    passthrough_optimize = False
    passthrough_diagnose = False
    passthrough_diagnose_output = None

    args_iter = iter(ctx.args) if ctx.args else iter([])
    for arg in args_iter:
        if arg == "-O3":
            passthrough_optimize = True
        elif arg == "--diagnose":
            passthrough_diagnose = True
        elif arg == "--diagnose-output":
            try:
                passthrough_diagnose_output = Path(next(args_iter))
            except StopIteration:
                pass
        else:
            flag_name = arg.split("=", 1)[0]
            if flag_name in pipeline_only_args:
                pipeline_passthrough_args.append(arg)
                if flag_name in value_args and "=" not in arg:
                    try:
                        pipeline_passthrough_args.append(next(args_iter))
                    except StopIteration:
                        pass
            else:
                cgeist_args.append(arg)

    extra_pipeline_args.extend(pipeline_passthrough_args)

    use_dual_mode = optimize or passthrough_optimize
    use_diagnose = diagnose or passthrough_diagnose
    final_diagnose_output = diagnose_output or passthrough_diagnose_output

    if use_dual_mode:
        _compile_c_dual(config, input_file, output_name, debug,
                        extra_pipeline_args, extra_link_args, cgeist_args,
                        use_diagnose, final_diagnose_output, pipeline)
    else:
        _compile_c_simple(config, input_file, output_name, debug,
                          extra_pipeline_args, extra_link_args, cgeist_args,
                          use_diagnose, final_diagnose_output, pipeline)


# -----------------------------------------------------------------------------
# Compile: .mlir input (MLIR pipeline)
# -----------------------------------------------------------------------------

def _compile_from_mlir(
    ctx: typer.Context,
    input_file: Path,
    output: Optional[Path],
    optimize: bool,
    debug: bool,
    pipeline: Optional[str],
    all_stages: bool,
    emit_llvm: bool,
    collect_metadata: bool,
) -> None:
    """MLIR transformation pipeline."""
    config = get_config()
    carts_compile_bin = config.carts_install_dir / "bin" / "carts-compile"

    # Pass --help through to carts-compile binary
    if ctx.args and ("--help" in ctx.args or "-h" in ctx.args):
        result = run_subprocess([str(carts_compile_bin), "--help"], check=False)
        raise typer.Exit(result.returncode)

    if all_stages:
        _compile_all_stages(config, input_file, output, ctx.args or [])
        return

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    cmd = [str(carts_compile_bin), str(input_file)]

    if optimize:
        cmd.append("--O3")
    if emit_llvm:
        cmd.append("--emit-llvm")
    if collect_metadata:
        cmd.append("--collect-metadata")
    if debug:
        cmd.append("-g")
    if pipeline:
        cmd.append(f"--stop-at={pipeline}")
    if output:
        cmd.extend(["-o", str(output)])
    if ctx.args:
        cmd.extend(ctx.args)

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# -----------------------------------------------------------------------------
# Compile: .ll input (link only)
# -----------------------------------------------------------------------------

def _compile_from_ll(
    ctx: typer.Context,
    input_file: Path,
    output: Optional[Path],
    debug: bool,
) -> None:
    """Link LLVM IR with ARTS runtime."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    if output is None:
        print_error("-o <output> is required for .ll input")
        raise typer.Exit(1)

    cmd = _build_link_cmd(config, input_file, output, debug,
                          ctx.args if ctx.args else [])

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# -----------------------------------------------------------------------------
# Compile Pipeline Helpers
# -----------------------------------------------------------------------------

def _build_cgeist_cmd(
    config: PlatformConfig,
    input_file: Path,
    std_flag: str,
    passthrough_args: List[str],
    with_openmp: bool = False,
    with_debug_info: bool = False,
) -> List[str]:
    """Build cgeist (C-to-MLIR) command with standard flags."""
    cmd = [str(config.polygeist_install_dir / "bin" / "cgeist")]
    cmd.extend(config.include_flags)
    cmd.extend(config.cgeist_sysroot_flags)
    cmd.extend(["--raise-scf-to-affine", std_flag, "-O0", "-S",
                 "-D_POSIX_C_SOURCE=199309L"])  # Required for clock_gettime
    if with_openmp:
        cmd.append("-fopenmp")
    if with_debug_info:
        cmd.append("--print-debug-info")
    cmd.extend(passthrough_args)
    cmd.append(str(input_file))
    return cmd


def _build_link_cmd(
    config: PlatformConfig,
    input_file: Path,
    output_file: Path,
    debug: bool,
    extra_args: List[str],
) -> List[str]:
    """Build clang link command for linking with ARTS runtime."""
    cmd = [str(config.llvm_install_dir / "bin" / "clang")]
    cmd.extend(config.compile_flags)
    cmd.extend(config.clang_sysroot_flags)
    cmd.extend(config.compile_library_flags)
    cmd.extend(config.linker_flags)
    cmd.extend(config.compile_libraries)
    cmd.append(str(input_file))
    cmd.extend(["-o", str(output_file)])
    if debug:
        cmd.append("-g")
    cmd.extend(extra_args)
    return cmd


def _compile_c_simple(
    config: PlatformConfig,
    input_file: Path,
    output_name: Path,
    debug: bool,
    pipeline_args: List[str],
    link_args: List[str],
    passthrough_args: Optional[List[str]] = None,
    diagnose: bool = False,
    diagnose_output: Optional[Path] = None,
    pipeline_stop: Optional[str] = None,
) -> None:
    """Simple pipeline: cgeist -> pipeline -> link.

    When pipeline_stop is set, stops after the MLIR pipeline stage (skips link).
    """
    base_name = input_file.stem
    extension = input_file.suffix
    std_flag = "-std=c++17" if extension == ".cpp" else "-std=c17"
    passthrough_args = passthrough_args or []
    skip_link = pipeline_stop is not None

    total_steps = 2 if skip_link else 3
    step_label = f"/{total_steps}]"

    print_header("CARTS Compile Pipeline")
    console.print(f"Input:  [{Colors.INFO}]{input_file}[/{Colors.INFO}]")
    if skip_link:
        console.print(f"Stop:   [{Colors.WARNING}]{pipeline_stop}[/{Colors.WARNING}]")
    else:
        console.print(f"Output: [{Colors.INFO}]{output_name}[/{Colors.INFO}]")
    console.print(f"Mode:   [{Colors.DIM}]Simple ({total_steps}-step)[/{Colors.DIM}]")
    console.print()

    carts_compile_bin = config.carts_install_dir / "bin" / "carts-compile"
    mlir_file = Path(f"{base_name}.mlir")
    ll_file = Path(f"{base_name}-arts.ll")

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        console=console,
    ) as progress:
        # Step 1: Convert C/C++ to MLIR with OpenMP
        task = progress.add_task(f"[1{step_label} Converting C to MLIR...", total=None)
        cmd = _build_cgeist_cmd(
            config, input_file, std_flag, passthrough_args, with_openmp=True)
        if run_command_with_output(cmd, mlir_file) != 0:
            print_error("Failed to convert C to MLIR")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[1{step_label} {mlir_file}")

        # Step 2: Apply ARTS transformations
        task = progress.add_task(
            f"[2{step_label} Applying ARTS transformations...", total=None)
        cmd = [str(carts_compile_bin), str(mlir_file), "--O3"]
        if not skip_link:
            cmd.append("--emit-llvm")
        if debug:
            cmd.append("-g")
        if diagnose:
            cmd.append("--diagnose")
            effective_diagnose_output = diagnose_output or Path(
                f"{base_name}-diagnose.json")
            cmd.extend(["--diagnose-output", str(effective_diagnose_output)])
        if pipeline_stop:
            cmd.append(f"--stop-at={pipeline_stop}")
        cmd.extend(pipeline_args)

        out_file = mlir_file.with_suffix(f".{pipeline_stop}.mlir") if skip_link else ll_file
        if run_command_with_output(cmd, out_file) != 0:
            print_error("Failed to apply ARTS transformations")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[2{step_label} {out_file}")

        if skip_link:
            console.print()
            console.print(Panel(
                f"[{Colors.SUCCESS}]Pipeline output:[/{Colors.SUCCESS}] {out_file}\n"
                f"[{Colors.DIM}]Stopped at: {pipeline_stop}[/{Colors.DIM}]",
                title="Success",
                border_style="green",
            ))
            return

        # Step 3: Link with ARTS runtime
        task = progress.add_task(
            f"[3{step_label} Linking executable...", total=None)
        cmd = _build_link_cmd(
            config, ll_file, output_name, debug, link_args)
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed to link executable")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[3{step_label} {output_name}")

    console.print()
    console.print(Panel(
        f"[{Colors.SUCCESS}]Generated:[/{Colors.SUCCESS}] {output_name}\n"
        f"[{Colors.DIM}]MLIR: {mlir_file}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]LLVM IR: {ll_file}[/{Colors.DIM}]",
        title="Success",
        border_style="green",
    ))


def _compile_c_dual(
    config: PlatformConfig,
    input_file: Path,
    output_name: Path,
    debug: bool,
    pipeline_args: List[str],
    link_args: List[str],
    passthrough_args: Optional[List[str]] = None,
    diagnose: bool = False,
    diagnose_output: Optional[Path] = None,
    pipeline_stop: Optional[str] = None,
) -> None:
    """Dual pipeline with metadata extraction.

    Steps:
    1. Sequential compilation (no OpenMP) - for metadata extraction
    2. Collect metadata from sequential MLIR
    3. Parallel compilation (with OpenMP) - for ARTS transformation
    4. Apply ARTS transformations
    5. Link final executable (skipped if pipeline_stop is set)
    """
    base_name = input_file.stem
    extension = input_file.suffix
    std_flag = "-std=c++17" if extension == ".cpp" else "-std=c17"
    passthrough_args = passthrough_args or []
    skip_link = pipeline_stop is not None

    total_steps = 4 if skip_link else 5
    step_label = f"/{total_steps}]"

    print_header("CARTS Compile Pipeline")
    console.print(f"Input:  [{Colors.INFO}]{input_file}[/{Colors.INFO}]")
    if skip_link:
        console.print(f"Stop:   [{Colors.WARNING}]{pipeline_stop}[/{Colors.WARNING}]")
    else:
        console.print(f"Output: [{Colors.INFO}]{output_name}[/{Colors.INFO}]")
    console.print(
        f"Mode:   [{Colors.WARNING}]Dual compilation (metadata extraction)[/{Colors.WARNING}]")
    console.print()

    carts_compile_bin = config.carts_install_dir / "bin" / "carts-compile"

    metadata_args: List[str] = []
    i = 0
    while i < len(pipeline_args):
        arg = pipeline_args[i]
        if arg == "--arts-config":
            metadata_args.append(arg)
            if i + 1 < len(pipeline_args):
                metadata_args.append(pipeline_args[i + 1])
                i += 1
        elif arg.startswith("--arts-config="):
            metadata_args.append(arg)
        i += 1

    # Output file paths
    seq_mlir = Path(f"{base_name}_seq.mlir")
    metadata_mlir = Path(f"{base_name}_arts_metadata.mlir")
    par_mlir = Path(f"{base_name}.mlir")
    ll_file = Path(f"{base_name}-arts.ll")
    metadata_json = Path(".carts-metadata.json")

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        console=console,
    ) as progress:
        # Step 1: Sequential compilation (no OpenMP, with debug info for metadata)
        task = progress.add_task(f"[1{step_label} Sequential compilation...", total=None)
        cmd = _build_cgeist_cmd(config, input_file, std_flag, passthrough_args,
                                with_openmp=False, with_debug_info=True)
        if run_command_with_output(cmd, seq_mlir) != 0:
            print_error("Failed sequential compilation")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[1{step_label} {seq_mlir}")

        # Step 2: Extract metadata from sequential MLIR
        task = progress.add_task(f"[2{step_label} Collecting metadata...", total=None)
        cmd = [str(carts_compile_bin), str(seq_mlir),
               "--collect-metadata", "-o", str(metadata_mlir)]
        cmd.extend(metadata_args)
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed to collect metadata")
            raise typer.Exit(1)
        progress.remove_task(task)
        if metadata_json.is_file():
            print_success(f"[2{step_label} {metadata_json}")
        else:
            print_warning(f"[2{step_label} Metadata file not created")

        # Step 3: Parallel compilation (with OpenMP for ARTS transformation)
        task = progress.add_task(f"[3{step_label} Parallel compilation...", total=None)
        cmd = _build_cgeist_cmd(config, input_file, std_flag, passthrough_args,
                                with_openmp=True, with_debug_info=True)
        if run_command_with_output(cmd, par_mlir) != 0:
            print_error("Failed parallel compilation")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[3{step_label} {par_mlir}")

        # Step 4: Apply ARTS transformations
        task = progress.add_task(f"[4{step_label} ARTS transformations...", total=None)
        cmd = [str(carts_compile_bin), str(par_mlir), "--O3"]
        if not skip_link:
            cmd.append("--emit-llvm")
        if debug:
            cmd.append("-g")
        if diagnose:
            cmd.append("--diagnose")
            effective_diagnose_output = diagnose_output or Path(
                f"{base_name}-diagnose.json")
            cmd.extend(["--diagnose-output", str(effective_diagnose_output)])
        if pipeline_stop:
            cmd.append(f"--stop-at={pipeline_stop}")
        cmd.extend(pipeline_args)

        out_file = par_mlir.with_suffix(f".{pipeline_stop}.mlir") if skip_link else ll_file
        if run_command_with_output(cmd, out_file) != 0:
            print_error("Failed ARTS transformations")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[4{step_label} {out_file}")

        if skip_link:
            console.print()
            console.print(Panel(
                f"[{Colors.SUCCESS}]Pipeline output:[/{Colors.SUCCESS}] {out_file}\n"
                f"[{Colors.DIM}]Stopped at: {pipeline_stop}[/{Colors.DIM}]",
                title="Success",
                border_style="green",
            ))
            return

        # Step 5: Link with ARTS runtime
        task = progress.add_task(f"[5{step_label} Final linking...", total=None)
        cmd = _build_link_cmd(
            config, ll_file, output_name, debug, link_args)
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed final linking")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[5{step_label} {output_name}")

    console.print()
    files_info = (
        f"[{Colors.SUCCESS}]Generated:[/{Colors.SUCCESS}] {output_name}\n"
        f"[{Colors.DIM}]Sequential: {seq_mlir}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]Metadata: {metadata_json}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]Parallel: {par_mlir}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]LLVM IR: {ll_file}[/{Colors.DIM}]"
    )
    console.print(Panel(files_info, title="Success", border_style="green"))


def _compile_all_stages(
    config: PlatformConfig,
    source_file: Path,
    output_dir: Optional[Path],
    extra_args: List[str],
) -> None:
    """Run pipeline and dump all intermediate stages."""
    if not source_file.is_file():
        print_error(f"Input file '{source_file}' not found")
        raise typer.Exit(1)

    carts_compile_bin = config.carts_install_dir / "bin" / "carts-compile"

    # Determine output directory
    out_dir = output_dir if output_dir else source_file.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    stem = source_file.stem
    total_steps = 2 + len(PIPELINE_STAGES)

    print_header("CARTS Pipeline Dump")

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TaskProgressColumn(),
        console=console,
    ) as progress:
        task = progress.add_task("Processing stages...", total=total_steps)

        # Find arts-config if present
        config_file = source_file.parent / "arts.cfg"
        base_cmd = [str(carts_compile_bin), str(source_file), "--O3"]
        if config_file.is_file():
            base_cmd.extend(["--arts-config", str(config_file)])

        # Dump each stage
        for stage in PIPELINE_STAGES:
            progress.update(task, description=f"Stage: {stage}")
            out_file = out_dir / f"{stem}_{stage}.mlir"
            cmd = base_cmd + [f"--stop-at={stage}",
                              "-o", str(out_file)] + extra_args
            run_subprocess(cmd, check=False)
            progress.advance(task)

        # Final MLIR
        progress.update(task, description="Exporting complete MLIR")
        final_mlir = out_dir / f"{stem}_complete.mlir"
        cmd = base_cmd + ["-o", str(final_mlir)] + extra_args
        run_subprocess(cmd, check=False)
        progress.advance(task)

        # Final LLVM IR
        progress.update(task, description="Exporting LLVM IR")
        final_ll = out_dir / f"{stem}.ll"
        cmd = base_cmd + ["--emit-llvm", "-o", str(final_ll)] + extra_args
        run_subprocess(cmd, check=False)
        progress.advance(task)

    print_success(f"Pipeline dumps written to {out_dir}")


# ============================================================================
# Benchmarks Command
# ============================================================================

@app.command(
    context_settings={
        "allow_extra_args": True,
        "allow_interspersed_args": False,
        "ignore_unknown_options": True,
    }
)
def benchmarks(
    ctx: typer.Context,
    help_flag: bool = typer.Option(
        False, "--help", "-h", is_eager=True, help="Show help"),
):
    """Build and manage CARTS benchmarks.

    Commands: list, run, build, clean, report

    Examples:
      carts benchmarks list
      carts benchmarks run polybench/2mm --size small
      carts benchmarks build --suite polybench
      carts benchmarks clean --all
    """
    config = get_config()
    benchmark_runner = config.carts_dir / "external" / \
        "carts-benchmarks" / "benchmark_runner.py"

    if not benchmark_runner.is_file():
        print_error(f"Benchmark runner not found at {benchmark_runner}")
        raise typer.Exit(1)

    # Use current Python interpreter (already in poetry env) for faster startup
    cmd = [sys.executable, str(benchmark_runner)]

    # Pass --help to the benchmark runner to show its commands
    if help_flag:
        cmd.append("--help")

    # Pass through all extra args (e.g., run, --verbose, --size, etc.)
    if ctx.args:
        cmd.extend(ctx.args)

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# ============================================================================
# Setup Command
# ============================================================================

@app.command()
def setup(
    skip_build: bool = typer.Option(
        False, "--skip-build", help="Skip building"),
    add_to_path: bool = typer.Option(
        False, "--add-to-path", help="Add carts to PATH"),
    deps_only: bool = typer.Option(
        False, "--deps-only", help="Install dependencies only"),
):
    """Set up CARTS environment."""
    config = get_config()
    setup_script = config.carts_dir / "tools" / "setup" / "carts-setup.py"

    if not setup_script.is_file():
        print_error(f"Setup script not found at {setup_script}")
        raise typer.Exit(1)

    cmd = ["python3", str(setup_script)]
    if skip_build:
        cmd.append("--skip-build")
    if add_to_path:
        cmd.append("--add-to-path")
    if deps_only:
        cmd.append("--deps-only")

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# ============================================================================
# Clean Command
# ============================================================================

@app.command()
def clean(
    docker_clean: bool = typer.Option(
        False, "--docker", "-d", help="Clean Docker artifacts"),
):
    """Clean generated files in current directory."""
    if docker_clean:
        _run_docker_clean()
    else:
        _run_local_clean()


def _run_local_clean() -> None:
    """Clean local generated files."""
    print_info("Cleaning generated files in current directory...")

    removed_count = 0
    cwd = Path.cwd()

    # Remove files matching patterns
    for pattern in CLEAN_FILE_PATTERNS:
        for f in cwd.glob(pattern):
            if f.is_file():
                if is_verbose():
                    print_debug(f"Removing file: {f}")
                f.unlink()
                removed_count += 1

    # Remove directories matching patterns
    for pattern in CLEAN_DIR_PATTERNS:
        for d in cwd.glob(pattern):
            if d.is_dir():
                if is_verbose():
                    print_debug(f"Removing directory: {d}")
                shutil.rmtree(d)
                removed_count += 1

    # Remove executables that look like CARTS compilation outputs
    # Only remove files ending with _arts or _omp (our naming convention)
    for pattern in ["*_arts", "*_omp"]:
        for f in cwd.glob(pattern):
            if f.is_file() and os.access(f, os.X_OK):
                if is_verbose():
                    print_debug(f"Removing CARTS executable: {f}")
                f.unlink()
                removed_count += 1

    if removed_count == 0:
        print_info("No generated files found to clean")
    else:
        print_success(f"Cleaned {removed_count} files/directories")


def _run_docker_clean() -> None:
    """Clean Docker containers and images."""
    config = get_config()
    docker_dir = config.carts_dir / "docker"

    if not docker_dir.is_dir():
        print_error(f"Docker directory not found at {docker_dir}")
        raise typer.Exit(1)

    clean_script = docker_dir / "docker-clean.sh"
    if not clean_script.is_file():
        print_error(f"docker-clean.sh not found in {docker_dir}")
        raise typer.Exit(1)

    print_info("Cleaning Docker multi-node setup...")
    run_subprocess(["./docker-clean.sh"], cwd=docker_dir, check=False)


# ============================================================================
# Check Command
# ============================================================================

def _resolve_lit_tool_paths(config: PlatformConfig) -> Tuple[Path, Path, Path]:
    """Resolve required test tools from the install tree."""
    return (
        config.llvm_install_dir / "bin" / "llvm-lit",
        config.llvm_install_dir / "bin" / "FileCheck",
        config.carts_install_dir / "bin" / "carts-compile",
    )


def _setup_lit_pythonpath(config: PlatformConfig) -> None:
    """Configure PYTHONPATH so `llvm-lit` can import `lit` reliably."""
    lit_python_paths: List[str] = []

    # 1) Standard install location for Python modules under LLVM prefix.
    try:
        result = subprocess.run(
            ["python3", "-c",
             "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"],
            capture_output=True,
            text=True,
        )
        python_version = result.stdout.strip()
        llvm_python_path = config.llvm_install_dir / "lib" / \
            f"python{python_version}" / "site-packages"
        if llvm_python_path.is_dir():
            lit_python_paths.append(str(llvm_python_path))
    except Exception:
        pass

    # 2) Runtime location expected by llvm-lit launcher generated by LLVM.
    llvm_lit_runtime = config.llvm_install_dir / "utils" / "lit"
    if (llvm_lit_runtime / "lit").is_dir():
        lit_python_paths.append(str(llvm_lit_runtime))

    # Backward-compatible location used by older bootstrap scripts.
    llvm_lit_runtime_legacy = config.llvm_install_dir / "llvm" / "utils" / "lit"
    if (llvm_lit_runtime_legacy / "lit").is_dir():
        lit_python_paths.append(str(llvm_lit_runtime_legacy))

    # 3) Source fallback if user has not bootstrapped install-side lit yet.
    source_lit_runtime = config.carts_dir / "external" / "Polygeist" / \
        "llvm-project" / "llvm" / "utils" / "lit"
    if (source_lit_runtime / "lit").is_dir():
        lit_python_paths.append(str(source_lit_runtime))

    if not lit_python_paths:
        return

    deduped_paths: List[str] = []
    for path in lit_python_paths:
        if path not in deduped_paths:
            deduped_paths.append(path)

    env_pythonpath = os.environ.get("PYTHONPATH", "")
    if env_pythonpath:
        deduped_paths.append(env_pythonpath)
    os.environ["PYTHONPATH"] = os.pathsep.join(deduped_paths)


def _run_lit_suite(suite: str, verbose_tests: bool) -> None:
    """Run a lit suite and terminate the command with its exit code."""
    config = get_config()
    print_info("Running CARTS test suite...")

    tests_dir = config.carts_dir / "tests"
    if not tests_dir.is_dir():
        print_error(f"Tests directory not found at {tests_dir}")
        raise typer.Exit(1)

    llvm_lit, filecheck, carts_compile = _resolve_lit_tool_paths(config)

    missing = []
    if not llvm_lit.is_file():
        missing.append(f"llvm-lit ({llvm_lit})")
    if not filecheck.is_file():
        missing.append(f"FileCheck ({filecheck})")
    if not carts_compile.is_file():
        missing.append(f"carts-compile ({carts_compile})")

    if missing:
        print_error("Required test tools not found:")
        for tool in missing:
            console.print(f"  - {tool}")
        print_info(
            "Run `carts build` to install llvm-lit/FileCheck/carts-compile under .install/."
        )
        raise typer.Exit(1)

    suite_aliases = {"arts": "examples"}
    effective_suite = suite_aliases.get(suite, suite)
    if suite == "arts":
        print_info("Suite 'arts' is deprecated; using 'examples'.")

    if effective_suite == "all":
        # Keep "all" deterministic and green by running maintained lit suites.
        test_paths = [tests_dir / "contracts"]
    elif effective_suite == "contracts":
        test_paths = [tests_dir / "contracts"]
    elif effective_suite == "examples":
        test_paths = [tests_dir / "examples"]
    else:
        print_error(f"Unknown test suite '{suite}'")
        print_info("Available suites: all, contracts, examples")
        raise typer.Exit(1)

    for test_path in test_paths:
        if not test_path.is_dir():
            print_error(f"Test path not found: {test_path}")
            raise typer.Exit(1)

    _setup_lit_pythonpath(config)

    console.print(
        f"Test suite: [{Colors.INFO}]{effective_suite}[/{Colors.INFO}]")
    for path in test_paths:
        console.print(f"Test path: [dim]{path}[/dim]")
    console.print()

    cmd = [str(llvm_lit)]
    if verbose_tests:
        cmd.append("-v")
    for path in test_paths:
        cmd.append(str(path))

    result = run_subprocess(cmd, check=False)
    console.print()
    if result.returncode == 0:
        print_success("CARTS test suite completed successfully!")
    else:
        print_error(f"CARTS test suite failed with exit code {result.returncode}")
    raise typer.Exit(result.returncode)


def _run_examples() -> None:
    """Run the full example compilation+execution test suite."""
    config = get_config()
    examples_runner = config.carts_dir / "tools" / "examples_runner.py"

    if not examples_runner.is_file():
        print_error(f"Examples runner not found at {examples_runner}")
        raise typer.Exit(1)

    cmd = [sys.executable, str(examples_runner), "run", "--all"]
    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


@app.command(name="test")
def test(
    suite: str = typer.Option("all", "--suite", "-s",
                              help="Test suite: all, contracts, examples"),
    verbose_tests: bool = typer.Option(
        False, "-v", help="Verbose test output"),
    examples: bool = typer.Option(
        False, "--examples",
        help="Run full example compilation+execution test suite"),
):
    """Run CARTS test suite using llvm-lit."""
    if examples:
        _run_examples()
    else:
        _run_lit_suite(suite=suite, verbose_tests=verbose_tests)


@app.command(name="check")
def check(
    suite: str = typer.Option("all", "--suite", "-s",
                              help="Test suite: all, contracts, examples"),
    verbose_tests: bool = typer.Option(
        False, "-v", help="Verbose test output"),
    examples: bool = typer.Option(
        False, "--examples",
        help="Run full example compilation+execution test suite"),
):
    """Alias for `carts test`."""
    if examples:
        _run_examples()
    else:
        _run_lit_suite(suite=suite, verbose_tests=verbose_tests)


def _is_format_candidate(path: Path) -> bool:
    if path.suffix not in FORMAT_SUFFIXES:
        return False
    for part in path.parts:
        if part in FORMAT_EXCLUDED_DIRS:
            return False
    return True


def _collect_tracked_format_files(config: PlatformConfig) -> List[Path]:
    """Collect tracked source files to format."""
    try:
        result = subprocess.run(
            ["git", "-C", str(config.carts_dir), "ls-files"],
            capture_output=True,
            text=True,
            check=True,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return []

    files: List[Path] = []
    for line in result.stdout.splitlines():
        rel = Path(line.strip())
        if not rel:
            continue
        if _is_format_candidate(rel):
            files.append(config.carts_dir / rel)
    return files


def _collect_format_files_from_paths(paths: Iterable[Path]) -> List[Path]:
    """Collect format targets from user-provided files/directories."""
    files: List[Path] = []
    for input_path in paths:
        path = input_path.resolve()
        if not path.exists():
            continue
        if path.is_file():
            if _is_format_candidate(path):
                files.append(path)
            continue
        for file_path in path.rglob("*"):
            if file_path.is_file() and _is_format_candidate(file_path):
                files.append(file_path)
    return files


def _resolve_clang_format(config: PlatformConfig) -> Optional[Path]:
    """Find clang-format in install tree first, then in PATH."""
    install_tool = config.llvm_install_dir / "bin" / "clang-format"
    if install_tool.is_file():
        return install_tool
    path_tool = shutil.which("clang-format")
    if path_tool:
        return Path(path_tool)
    return None


@app.command(name="format")
def format_sources(
    paths: Optional[List[Path]] = typer.Argument(
        None,
        help=(
            "Files or directories to format. If omitted, formats tracked "
            "C/C++/TableGen sources in the repo."
        ),
    ),
    check_only: bool = typer.Option(
        False, "--check", help="Check formatting without modifying files"),
    list_files: bool = typer.Option(
        False, "--list", help="Print the files selected for formatting"),
):
    """Format source files with clang-format."""
    config = get_config()
    clang_format = _resolve_clang_format(config)
    if not clang_format:
        print_error("clang-format not found in .install/llvm/bin or PATH.")
        print_info("Run `carts build` (or install clang-format) and retry.")
        raise typer.Exit(1)

    selected_paths = paths or []
    if selected_paths:
        files = _collect_format_files_from_paths(selected_paths)
    else:
        files = _collect_tracked_format_files(config)

    deduped = sorted({f.resolve() for f in files})
    if not deduped:
        print_warning("No eligible source files found for clang-format.")
        raise typer.Exit(0)

    if list_files:
        for file_path in deduped:
            console.print(str(file_path))

    print_info(
        f"{'Checking' if check_only else 'Formatting'} {len(deduped)} files with {clang_format}"
    )

    failed = 0
    for file_path in deduped:
        cmd = [str(clang_format)]
        if check_only:
            cmd.extend(["--dry-run", "--Werror"])
        else:
            cmd.append("-i")
        cmd.append(str(file_path))
        result = run_subprocess(cmd, check=False)
        if result.returncode != 0:
            failed += 1

    if failed > 0:
        action = "check" if check_only else "format"
        print_error(f"clang-format {action} failed for {failed} file(s).")
        raise typer.Exit(1)

    if check_only:
        print_success("Formatting check passed.")
    else:
        print_success("Formatting completed successfully.")


# ============================================================================
# Docker Commands
# ============================================================================

def _get_docker_script(name: str) -> Tuple[Path, Path]:
    """Get docker script path, raising error if not found.

    Returns (docker_dir, script_path) tuple for use with run_subprocess.
    """
    config = get_config()
    docker_dir = config.carts_dir / "docker"

    if not docker_dir.is_dir():
        print_error(f"Docker directory not found at {docker_dir}")
        raise typer.Exit(1)

    script = docker_dir / f"docker-{name}.sh"
    if not script.is_file():
        print_error(f"docker-{name}.sh not found in {docker_dir}")
        raise typer.Exit(1)

    return docker_dir, script


@docker_app.callback(invoke_without_command=True)
def docker_callback(ctx: typer.Context):
    """Docker operations for multi-node execution."""
    if ctx.invoked_subcommand is None:
        console.print(
            "Docker command requires a subcommand. Use --help for options.")
        raise typer.Exit(1)


@docker_app.command(name="run")
def docker_run(
    file: Optional[Path] = typer.Argument(None, help="File to run in Docker"),
    args: Optional[List[str]] = typer.Argument(None, help="Runtime arguments"),
    slurm: bool = typer.Option(
        False, "--slurm", help="Use Slurm launcher instead of SSH"),
):
    """Run example in Docker containers."""
    docker_dir, _ = _get_docker_script("run")

    cmd = ["./docker-run.sh"]
    if slurm:
        cmd.append("--slurm")
    if file:
        cmd.append(str(file))
    if args:
        cmd.extend(args)

    result = run_subprocess(cmd, cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


@docker_app.command(name="build")
def docker_build(
    force: bool = typer.Option(
        False, "--force", "-f", help="Force rebuild from scratch"),
):
    """Build Docker image and workspace."""
    docker_dir, _ = _get_docker_script("build")

    cmd = ["./docker-build.sh"]
    if force:
        cmd.append("--force")

    result = run_subprocess(cmd, cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


@docker_app.command(name="update")
def docker_update(
    arts: bool = typer.Option(False, "--arts", "-a", help="Rebuild ARTS"),
    polygeist: bool = typer.Option(
        False, "--polygeist", "-p", help="Rebuild Polygeist"),
    llvm: bool = typer.Option(False, "--llvm", "-l", help="Rebuild LLVM"),
    carts_rebuild: bool = typer.Option(
        False, "--carts", "-c", help="Rebuild CARTS"),
    force: bool = typer.Option(False, "--force", "-f", help="Force rebuild"),
    debug_mode: bool = typer.Option(
        False, "--debug", help="Build with debug logging"),
    info_mode: bool = typer.Option(
        False, "--info", help="Build with info logging"),
):
    """Update Docker containers."""
    docker_dir, _ = _get_docker_script("update")

    cmd = ["./docker-update.sh"]
    if force:
        cmd.append("--force")
    if arts:
        if debug_mode:
            cmd.append("--arts=debug")
        elif info_mode:
            cmd.append("--arts=info")
        else:
            cmd.append("--arts")
    if polygeist:
        cmd.append("--polygeist")
    if llvm:
        cmd.append("--llvm")
    if carts_rebuild:
        cmd.append("--carts")

    result = run_subprocess(cmd, cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


@docker_app.command(name="kill")
def docker_kill():
    """Kill all processes in running ARTS containers."""
    docker_dir, _ = _get_docker_script("kill")
    print_info("Killing all processes in ARTS containers...")
    result = run_subprocess(["./docker-kill.sh"], cwd=docker_dir, check=False)
    raise typer.Exit(result.returncode)


@docker_app.command(name="clean")
def docker_clean_cmd():
    """Clean Docker containers and images."""
    _run_docker_clean()


# ============================================================================
# Entry Point
# ============================================================================

def main():
    """Main entry point."""
    # Set LLVM_SYMBOLIZER_PATH for better stack traces
    config = get_config()
    symbolizer = config.llvm_install_dir / "bin" / "llvm-symbolizer"
    if symbolizer.is_file():
        os.environ["LLVM_SYMBOLIZER_PATH"] = str(symbolizer)

    app()


if __name__ == "__main__":
    main()
