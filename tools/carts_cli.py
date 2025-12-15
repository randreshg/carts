#!/usr/bin/env python3
"""
CARTS CLI - Compiler for Asynchronous Runtime Systems
A modern Python CLI with rich terminal output.
"""

from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional, Dict, Tuple
import platform as sys_platform
import subprocess
import os
import shutil
import sys

import typer
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TaskProgressColumn
from rich.table import Table
from rich.text import Text
from rich import box
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
    "create-dbs",            # Create DataBlocks for inter-task communication
    "db-opt",                # Optimize DataBlock operations
    "edt-opt",               # Further EDT optimizations
    "concurrency",           # Add concurrency annotations
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

    def _setup_linux_flags(self) -> None:
        """Setup Linux-specific flags."""
        self.system_include_path = Path("/usr/include")
        self.system_cxx_include_path = Path("/usr/include/c++/v1")

        self.include_flags.extend([
            f"-I{self.system_include_path}",
            f"-I{self.system_cxx_include_path}",
        ])

        self.clang_library_flags.extend(["-L/usr/lib", "-L/usr/lib64"])
        self.compile_library_flags.extend(["-L/usr/lib", "-L/usr/lib64"])
        self.clang_libraries.extend(["-lpthread", "-lrt"])
        self.compile_libraries.extend(["-lpthread", "-lrt"])
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
            f"-L{self.arts_install_dir}/lib",
            f"-L{self.llvm_lib_path}",
            f"-L{self.llvm_install_dir}/lib",
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
                self.llvm_lib_path,
            ]

            existing = set()
            for rp in rpaths:
                rpath_flag = f"-Wl,-rpath,{rp}"
                if rpath_flag not in existing:
                    self.linker_flags.append(rpath_flag)
                    existing.add(rpath_flag)


# ============================================================================
# Rich Console Setup
# ============================================================================
# Console and print functions are imported from carts_styles


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

# Counter profile mapping: level -> config file name
COUNTER_PROFILES = {
    0: "counter.profile-none.cfg",      # All OFF - baseline performance
    1: "counter.profile-artsid-only.cfg",  # ArtsID metrics for hotspot analysis
    2: "counter.profile-deep.cfg",       # Full captures for deep profiling
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
        help="Counter profile: 0=off (default), 1=artsid, 2=deep"),
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
        profile_name = COUNTER_PROFILES.get(
            counters_level, "counter.profile-none.cfg")
        counter_config = config.carts_dir / "external" / "arts" / profile_name
        make_vars.append(f"COUNTER_CONFIG_PATH={counter_config}")

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

    # If polygeist was built, also rebuild carts-run
    if result.returncode == 0 and target == "polygeist":
        print_step("Rebuilding carts-run after Polygeist update...")
        cmd = ["make"] + make_vars + ["carts-run-only"]
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
# Run Command
# ============================================================================

@app.command(name="run", context_settings={"allow_extra_args": True, "allow_interspersed_args": False})
def run_cmd(
    ctx: typer.Context,
    help_flag: bool = typer.Option(
        False, "--help", "-h", is_eager=True, help="Show help for carts-run"),
    input_file: Optional[Path] = typer.Argument(None, help="Input MLIR file"),
    output: Optional[Path] = typer.Option(None, "-o", help="Output file"),
    stop_at: Optional[str] = typer.Option(
        None, "--stop-at", help="Stop at pipeline stage"),
    all_stages: bool = typer.Option(
        False, "--all-stages", help="Dump all pipeline stages"),
    emit_llvm: bool = typer.Option(
        False, "--emit-llvm", help="Emit LLVM IR output"),
    collect_metadata: bool = typer.Option(
        False, "--collect-metadata", help="Collect and export metadata"),
    optimize: bool = typer.Option(
        False, "--O3", help="Enable O3 optimizations"),
    debug: bool = typer.Option(False, "-g", help="Enable debug info emission"),
):
    """Run CARTS MLIR transformations."""
    config = get_config()
    carts_run_bin = config.carts_install_dir / "bin" / "carts-run"

    # Pass --help through to carts-run binary to show MLIR options
    if help_flag:
        result = run_subprocess([str(carts_run_bin), "--help"], check=False)
        raise typer.Exit(result.returncode)

    # Also check ctx.args in case help comes after the input file
    if ctx.args and ("--help" in ctx.args or "-h" in ctx.args):
        result = run_subprocess([str(carts_run_bin), "--help"], check=False)
        raise typer.Exit(result.returncode)

    # input_file is required if not showing help
    if input_file is None:
        print_error("Input file is required")
        raise typer.Exit(1)

    if all_stages:
        # Handle --all-stages mode
        _run_all_stages(config, input_file, output, ctx.args or [])
        return

    # Normal run
    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    cmd = [str(carts_run_bin), str(input_file)]

    if optimize:
        cmd.append("--O3")
    if emit_llvm:
        cmd.append("--emit-llvm")
    if collect_metadata:
        cmd.append("--collect-metadata")
    if debug:
        cmd.append("-g")
    if stop_at:
        cmd.append(f"--stop-at={stop_at}")
    if output:
        cmd.extend(["-o", str(output)])
    # Pass through extra args (e.g., --metadata-file, --concurrency)
    if ctx.args:
        cmd.extend(ctx.args)

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


def _run_all_stages(
    config: PlatformConfig,
    source_file: Path,
    output_dir: Optional[Path],
    extra_args: List[str],
) -> None:
    """Run pipeline and dump all intermediate stages."""
    if not source_file.is_file():
        print_error(f"Input file '{source_file}' not found")
        raise typer.Exit(1)

    carts_run_bin = config.carts_install_dir / "bin" / "carts-run"

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
        base_cmd = [str(carts_run_bin), str(source_file), "--O3"]
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
# Compile Command
# ============================================================================

@app.command(context_settings={"allow_extra_args": True, "allow_interspersed_args": False})
def compile(
    ctx: typer.Context,
    input_file: Path = typer.Argument(..., help="Input LLVM IR file (.ll)"),
    output: Path = typer.Option(..., "-o", help="Output executable"),
    debug: bool = typer.Option(False, "-g", help="Embed debug symbols"),
):
    """Compile LLVM IR with ARTS runtime."""
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
    cmd.extend(config.compile_flags)
    cmd.extend(config.clang_sysroot_flags)
    cmd.extend(config.compile_library_flags)
    cmd.extend(config.linker_flags)
    cmd.extend(config.compile_libraries)
    cmd.append(str(input_file))
    cmd.extend(["-o", str(output)])

    if debug:
        cmd.append("-g")

    # Pass through extra args (e.g., -lm, -lpthread, etc.)
    if ctx.args:
        cmd.extend(ctx.args)

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# ============================================================================
# Execute Command
# ============================================================================

@app.command(context_settings={"allow_extra_args": True, "allow_interspersed_args": False})
def execute(
    ctx: typer.Context,
    input_file: Path = typer.Argument(..., help="Input C/C++ file"),
    output: Optional[Path] = typer.Option(
        None, "-o", help="Output executable"),
    optimize: bool = typer.Option(
        False, "-O3", help="Enable dual compilation mode"),
    debug: bool = typer.Option(False, "-g", help="Generate debug symbols"),
    diagnose: bool = typer.Option(
        False, "--diagnose", help="Export diagnostic information"),
    diagnose_output: Optional[Path] = typer.Option(
        None, "--diagnose-output", help="Output file for diagnostics"),
    run_args: Optional[str] = typer.Option(
        None, "--run-args", help="Arguments for carts run"),
    compile_args: Optional[str] = typer.Option(
        None, "--compile-args", help="Arguments for carts compile"),
):
    """Complete pipeline: C++ -> executable."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    base_name = input_file.stem
    extension = input_file.suffix

    if extension not in (".cpp", ".c"):
        print_error("Input file must be .cpp or .c")
        raise typer.Exit(1)

    output_name = output if output else Path(f"{base_name}_arts")

    # Parse extra args - combine explicit args with pass-through args
    extra_run_args = run_args.split() if run_args else []
    extra_compile_args = compile_args.split() if compile_args else []

    # Separate passthrough args into two categories:
    # - cgeist_args: Flags for cgeist C-to-MLIR frontend (e.g., -I, -D, -fopenmp)
    # - run_passthrough_args: Flags for carts-run (e.g., --arts-config)
    # This is needed because execute() calls both tools in sequence.
    cgeist_args: List[str] = []
    run_passthrough_args: List[str] = []

    # Args that should go to carts-run, not cgeist
    run_only_args = {"--arts-config", "--metadata-file", "--concurrency",
                     "--concurrency-opt", "--diagnose", "--diagnose-output"}

    # Check if -O3 or --diagnose is in passthrough args
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
        elif arg in run_only_args:
            run_passthrough_args.append(arg)
            if arg in ("--arts-config", "--metadata-file"):
                try:
                    run_passthrough_args.append(next(args_iter))
                except StopIteration:
                    pass
        else:
            cgeist_args.append(arg)

    # Merge run_passthrough_args into extra_run_args
    extra_run_args.extend(run_passthrough_args)

    # Combine typer options with passthrough args
    use_dual_mode = optimize or passthrough_optimize
    use_diagnose = diagnose or passthrough_diagnose
    final_diagnose_output = diagnose_output or passthrough_diagnose_output

    if use_dual_mode:
        _execute_dual(config, input_file, output_name, debug,
                      extra_run_args, extra_compile_args, cgeist_args)
    else:
        _execute_simple(config, input_file, output_name, debug, extra_run_args,
                        extra_compile_args, cgeist_args, use_diagnose, final_diagnose_output)


# -----------------------------------------------------------------------------
# Execute Pipeline Helpers
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
    cmd.extend(["--raise-scf-to-affine", std_flag, "-O0", "-S"])
    if with_openmp:
        cmd.append("-fopenmp")
    if with_debug_info:
        cmd.append("--print-debug-info")
    cmd.extend(passthrough_args)
    cmd.append(str(input_file))
    return cmd


def _build_compile_cmd(
    config: PlatformConfig,
    input_file: Path,
    output_file: Path,
    debug: bool,
    extra_args: List[str],
) -> List[str]:
    """Build clang compile command for linking with ARTS runtime."""
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


def _execute_simple(
    config: PlatformConfig,
    input_file: Path,
    output_name: Path,
    debug: bool,
    run_args: List[str],
    compile_args: List[str],
    passthrough_args: Optional[List[str]] = None,
    diagnose: bool = False,
    diagnose_output: Optional[Path] = None,
) -> None:
    """Simple 3-step pipeline: cgeist -> run -> compile."""
    base_name = input_file.stem
    extension = input_file.suffix
    std_flag = "-std=c++17" if extension == ".cpp" else "-std=c17"
    passthrough_args = passthrough_args or []

    print_header("CARTS Execute Pipeline")
    console.print(f"Input:  [{Colors.INFO}]{input_file}[/{Colors.INFO}]")
    console.print(f"Output: [{Colors.INFO}]{output_name}[/{Colors.INFO}]")
    console.print(f"Mode:   [{Colors.DIM}]Simple (3-step)[/{Colors.DIM}]")
    console.print()

    carts_run_bin = config.carts_install_dir / "bin" / "carts-run"
    mlir_file = Path(f"{base_name}.mlir")
    ll_file = Path(f"{base_name}-arts.ll")

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        console=console,
    ) as progress:
        # Step 1: Convert C/C++ to MLIR with OpenMP
        task = progress.add_task("[1/3] Converting C++ to MLIR...", total=None)
        cmd = _build_cgeist_cmd(
            config, input_file, std_flag, passthrough_args, with_openmp=True)
        if run_command_with_output(cmd, mlir_file) != 0:
            print_error("Failed to convert C++ to MLIR")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[1/3] {mlir_file}")

        # Step 2: Apply ARTS transformations
        task = progress.add_task(
            "[2/3] Applying ARTS transformations...", total=None)
        cmd = [str(carts_run_bin), str(mlir_file), "--O3", "--emit-llvm"]
        if debug:
            cmd.append("-g")
        if diagnose:
            cmd.append("--diagnose")
            # When diagnose is enabled without explicit output, use default file
            effective_diagnose_output = diagnose_output or Path(
                f"{base_name}-diagnose.json")
            cmd.extend(["--diagnose-output", str(effective_diagnose_output)])
        cmd.extend(run_args)
        if run_command_with_output(cmd, ll_file) != 0:
            print_error("Failed to apply ARTS transformations")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[2/3] {ll_file}")

        # Step 3: Link with ARTS runtime
        task = progress.add_task(
            "[3/3] Compiling to executable...", total=None)
        cmd = _build_compile_cmd(
            config, ll_file, output_name, debug, compile_args)
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed to compile to executable")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[3/3] {output_name}")

    console.print()
    console.print(Panel(
        f"[{Colors.SUCCESS}]Generated:[/{Colors.SUCCESS}] {output_name}\n"
        f"[{Colors.DIM}]MLIR: {mlir_file}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]LLVM IR: {ll_file}[/{Colors.DIM}]",
        title="Success",
        border_style="green",
    ))


def _execute_dual(
    config: PlatformConfig,
    input_file: Path,
    output_name: Path,
    debug: bool,
    run_args: List[str],
    compile_args: List[str],
    passthrough_args: Optional[List[str]] = None,
) -> None:
    """Dual 5-step pipeline with metadata extraction.

    Steps:
    1. Sequential compilation (no OpenMP) - for metadata extraction
    2. Collect metadata from sequential MLIR
    3. Parallel compilation (with OpenMP) - for ARTS transformation
    4. Apply ARTS transformations
    5. Link final executable
    """
    base_name = input_file.stem
    extension = input_file.suffix
    std_flag = "-std=c++17" if extension == ".cpp" else "-std=c17"
    passthrough_args = passthrough_args or []

    print_header("CARTS Execute Pipeline")
    console.print(f"Input:  [{Colors.INFO}]{input_file}[/{Colors.INFO}]")
    console.print(f"Output: [{Colors.INFO}]{output_name}[/{Colors.INFO}]")
    console.print(
        f"Mode:   [{Colors.WARNING}]Dual compilation (metadata extraction)[/{Colors.WARNING}]")
    console.print()

    carts_run_bin = config.carts_install_dir / "bin" / "carts-run"

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
        task = progress.add_task("[1/5] Sequential compilation...", total=None)
        cmd = _build_cgeist_cmd(config, input_file, std_flag, passthrough_args,
                                with_openmp=False, with_debug_info=True)
        if run_command_with_output(cmd, seq_mlir) != 0:
            print_error("Failed sequential compilation")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[1/5] {seq_mlir}")

        # Step 2: Extract metadata from sequential MLIR
        task = progress.add_task("[2/5] Collecting metadata...", total=None)
        cmd = [str(carts_run_bin), str(seq_mlir),
               "--collect-metadata", "-o", str(metadata_mlir)]
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed to collect metadata")
            raise typer.Exit(1)
        progress.remove_task(task)
        if metadata_json.is_file():
            print_success(f"[2/5] {metadata_json}")
        else:
            print_warning("[2/5] Metadata file not created")

        # Step 3: Parallel compilation (with OpenMP for ARTS transformation)
        task = progress.add_task("[3/5] Parallel compilation...", total=None)
        cmd = _build_cgeist_cmd(config, input_file, std_flag, passthrough_args,
                                with_openmp=True, with_debug_info=True)
        if run_command_with_output(cmd, par_mlir) != 0:
            print_error("Failed parallel compilation")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[3/5] {par_mlir}")

        # Step 4: Apply ARTS transformations
        task = progress.add_task("[4/5] ARTS transformations...", total=None)
        cmd = [str(carts_run_bin), str(par_mlir), "--O3", "--emit-llvm"]
        if debug:
            cmd.append("-g")
        cmd.extend(run_args)
        if run_command_with_output(cmd, ll_file) != 0:
            print_error("Failed ARTS transformations")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[4/5] {ll_file}")

        # Step 5: Link with ARTS runtime
        task = progress.add_task("[5/5] Final compilation...", total=None)
        cmd = _build_compile_cmd(
            config, ll_file, output_name, debug, compile_args)
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed final compilation")
            raise typer.Exit(1)
        progress.remove_task(task)
        print_success(f"[5/5] {output_name}")

    console.print()
    files_info = (
        f"[{Colors.SUCCESS}]Generated:[/{Colors.SUCCESS}] {output_name}\n"
        f"[{Colors.DIM}]Sequential: {seq_mlir}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]Metadata: {metadata_json}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]Parallel: {par_mlir}[/{Colors.DIM}]\n"
        f"[{Colors.DIM}]LLVM IR: {ll_file}[/{Colors.DIM}]"
    )
    console.print(Panel(files_info, title="Success", border_style="green"))


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
# Examples Command
# ============================================================================

@app.command(
    context_settings={
        "allow_extra_args": True,
        "allow_interspersed_args": False,
        "ignore_unknown_options": True,
    }
)
def examples(
    ctx: typer.Context,
    help_flag: bool = typer.Option(
        False, "--help", "-h", is_eager=True, help="Show help"),
):
    """Run and manage CARTS examples.

    Commands: list-examples, run, clean

    Examples:
      carts examples list-examples           # List all available examples
      carts examples run <name>              # Run a specific example
      carts examples run --all              # Run all examples
      carts examples clean [name]            # Clean example artifacts
      carts examples clean --all            # Clean all examples
    """
    config = get_config()
    examples_runner = config.carts_dir / "tools" / "examples_runner.py"

    if not examples_runner.is_file():
        print_error(f"Examples runner not found at {examples_runner}")
        raise typer.Exit(1)

    cmd = [sys.executable, str(examples_runner)]

    # Pass --help to the examples runner to show its commands
    if help_flag:
        cmd.append("--help")

    # Pass through all extra args (e.g., run, list-examples, --verbose, etc.)
    if ctx.args:
        cmd.extend(ctx.args)

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


def _get_poetry_python(script_dir: Path) -> Optional[str]:
    """Get the poetry Python executable path."""
    # First check if dependencies are available in system Python
    try:
        result = subprocess.run(
            ["python3", "-c", "import typer, rich"],
            capture_output=True,
            check=True,
        )
        return "python3"
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # Try poetry
    try:
        result = subprocess.run(
            ["poetry", "-C", str(script_dir), "env", "info", "-e"],
            capture_output=True,
            text=True,
            check=True,
        )
        poetry_python = result.stdout.strip()
        if Path(poetry_python).is_file():
            return poetry_python
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    return None


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

@app.command()
def check(
    suite: str = typer.Option("all", "--suite", "-s",
                              help="Test suite: all, arts"),
    verbose_tests: bool = typer.Option(
        False, "-v", help="Verbose test output"),
):
    """Run CARTS test suite using llvm-lit."""
    config = get_config()

    print_info("Running CARTS test suite...")

    tests_dir = config.carts_dir / "tests"
    if not tests_dir.is_dir():
        print_error(f"Tests directory not found at {tests_dir}")
        raise typer.Exit(1)

    llvm_lit = config.llvm_install_dir / "bin" / "llvm-lit"
    filecheck = config.llvm_install_dir / "bin" / "FileCheck"
    carts_run = config.carts_install_dir / "bin" / "carts-run"

    # Check for required tools
    missing = []
    if not llvm_lit.is_file():
        missing.append(f"llvm-lit ({llvm_lit})")
    if not filecheck.is_file():
        missing.append(f"FileCheck ({filecheck})")
    if not carts_run.is_file():
        missing.append(f"carts-run ({carts_run})")

    if missing:
        print_error("Required test tools not found:")
        for tool in missing:
            console.print(f"  - {tool}")
        print_info("Run 'carts build' to install the required tools.")
        raise typer.Exit(1)

    # Determine test path
    if suite == "all":
        test_path = tests_dir
    elif suite == "arts":
        test_path = tests_dir / "arts"
    else:
        print_error(f"Unknown test suite '{suite}'")
        print_info("Available suites: all, arts")
        raise typer.Exit(1)

    if not test_path.is_dir():
        print_error(f"Test path not found: {test_path}")
        raise typer.Exit(1)

    console.print(f"Test suite: [{Colors.INFO}]{suite}[/{Colors.INFO}]")
    console.print(f"Test path: [dim]{test_path}[/dim]")
    console.print()

    # Setup PYTHONPATH for lit module
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
            env_pythonpath = os.environ.get("PYTHONPATH", "")
            os.environ["PYTHONPATH"] = f"{llvm_python_path}:{env_pythonpath}" if env_pythonpath else str(
                llvm_python_path)
    except Exception:
        pass

    # Build llvm-lit command
    cmd = [str(llvm_lit)]
    if verbose_tests:
        cmd.append("-v")
    cmd.append(str(test_path))

    result = run_subprocess(cmd, check=False)

    console.print()
    if result.returncode == 0:
        print_success("CARTS test suite completed successfully!")
    else:
        print_error(
            f"CARTS test suite failed with exit code {result.returncode}")

    raise typer.Exit(result.returncode)


# ============================================================================
# Report Command
# ============================================================================

@app.command()
def report(
    # Input sources (at least one required)
    results: Optional[Path] = typer.Option(None, "--results", "-r",
                                           help="JSON from 'carts benchmarks run' (scaling analysis)"),
    counters: Optional[Path] = typer.Option(None, "--counters", "-c",
                                            help="Counter directory with n*_t*.json files (hotspot analysis)"),
    metadata: Optional[Path] = typer.Option(None, "--metadata", "-m",
                                            help="CARTS metadata JSON for source location mapping"),

    # Output control
    format: str = typer.Option("table", "--format", "-f",
                               help="Output format: table, csv, json"),
    output: Optional[Path] = typer.Option(None, "--output", "-o",
                                          help="Write to file instead of stdout"),
    top: int = typer.Option(20, "--top", "-n",
                            help="Limit rows in hotspot output"),

    # Tuning
    stride: int = typer.Option(1000, "--stride",
                               help="EDT arts_id stride for metadata lookup"),
):
    """Analyze benchmark results and/or runtime counters."""
    if not results and not counters:
        print_error("Must specify at least --results or --counters")
        raise typer.Exit(1)

    # Load and analyze results
    if results:
        try:
            with open(results) as f:
                results_data = json.load(f)
        except Exception as e:
            print_error(f"Failed to load results JSON: {e}")
            raise typer.Exit(1)

    if counters:
        if not counters.is_dir():
            print_error(f"Counters directory not found: {counters}")
            raise typer.Exit(1)

        # Load counter data
        edts, dbs = _load_counter_metrics(counters)

        # Load metadata for source mapping if available
        meta = {}
        if metadata:
            if not metadata.is_file():
                print_error(f"Metadata file not found: {metadata}")
                raise typer.Exit(1)
            meta = _load_metadata(metadata)

    # Generate reports (print directly to console)
    if results:
        _print_scaling_table(results_data)

    if counters:
        _print_hotspot_table(edts, dbs, meta, stride, top)

    # Note: --output with --format csv/json would need separate handling
    if output and format != "table":
        print_info(f"Use --format csv or json with --output for file export")


def _load_counter_metrics(counter_dir: Path) -> Tuple[Dict[int, Dict], Dict[int, Dict]]:
    """Load and merge all n*_t*.json counter files."""
    edts, dbs = {}, {}

    for path in sorted(counter_dir.glob("n*_t*.json")):
        try:
            data = json.loads(path.read_text())
        except Exception:
            continue

        arts = data.get("artsIdMetrics", {})

        for edt in arts.get("edts", []):
            if not isinstance(edt, dict):
                continue
            aid = edt.get("arts_id", 0)
            if aid <= 0:
                continue
            rec = edts.setdefault(aid, {"invocations": 0, "total_exec_ns": 0})
            rec["invocations"] += edt.get("invocations", 0)
            rec["total_exec_ns"] += edt.get("total_exec_ns", 0)

        for db in arts.get("dbs", []):
            if not isinstance(db, dict):
                continue
            aid = db.get("arts_id", 0)
            if aid <= 0:
                continue
            rec = dbs.setdefault(
                aid, {"invocations": 0, "bytes_local": 0, "bytes_remote": 0})
            rec["invocations"] += db.get("invocations", 0)
            rec["bytes_local"] += db.get("bytes_local", 0)
            rec["bytes_remote"] += db.get("bytes_remote", 0)

    return edts, dbs


def _load_metadata(metadata_path: Path) -> Dict[int, Dict]:
    """Load CARTS metadata for source location mapping."""
    data = json.loads(metadata_path.read_text())
    meta = {}

    for obj in data.get("loops", {}).values():
        aid = obj.get("arts_id", 0)
        if aid > 0:
            meta[aid] = {"kind": "loop",
                         "location": obj.get("location_key", "")}

    for obj in data.get("memrefs", {}).values():
        aid = obj.get("arts_id", 0)
        if aid > 0:
            meta[aid] = {"kind": "memref",
                         "allocation": obj.get("allocation_id", "")}

    return meta


def _get_kernel_time(run_result: Dict) -> Optional[float]:
    """Extract total kernel time from run result."""
    timings = run_result.get("kernel_timings", {})
    if timings:
        return sum(timings.values())
    return None


def _print_scaling_table(results_data: Dict) -> None:
    """Print scaling metrics table."""
    results = results_data.get("results", [])

    # Group by benchmark
    by_bench = {}
    for r in results:
        if "thread_count" in r:  # Thread sweep results
            by_bench.setdefault(r["name"], []).append(r)

    for bench, runs in by_bench.items():
        runs = sorted(runs, key=lambda r: r["thread_count"])
        if not runs:
            continue

        # Prefer kernel time for speedup (more meaningful), fall back to total time
        use_kernel_time = all(_get_kernel_time(r["run_arts"]) for r in runs)

        if use_kernel_time:
            t1_arts = _get_kernel_time(runs[0]["run_arts"])
            t1_omp = _get_kernel_time(
                runs[0]["run_omp"]) or runs[0]["run_omp"]["duration_sec"]
            time_label = "Kernel"
        else:
            t1_arts = runs[0]["run_arts"]["duration_sec"]
            t1_omp = runs[0]["run_omp"]["duration_sec"]
            time_label = "Total"

        table = Table(title=f"{bench} - Strong Scaling ({time_label} Time)")
        table.add_column("Threads", justify="right")
        table.add_column("ARTS", justify="right")
        table.add_column("OMP", justify="right")
        table.add_column("S(p)", justify="right", style="cyan")
        table.add_column("E(p)", justify="right")
        table.add_column("vs OMP", justify="right")

        for r in runs:
            p = r["thread_count"]

            if use_kernel_time:
                t_arts = _get_kernel_time(r["run_arts"])
                t_omp = _get_kernel_time(
                    r["run_omp"]) or r["run_omp"]["duration_sec"]
            else:
                t_arts = r["run_arts"]["duration_sec"]
                t_omp = r["run_omp"]["duration_sec"]

            speedup = t1_arts / t_arts if t_arts > 0 else 0.0
            eff = speedup / p if p > 0 else 0.0
            diff = ((t_arts - t_omp) / t_omp) * 100 if t_omp > 0 else 0.0

            # Color the vs OMP column based on performance
            if diff <= 5:
                diff_str = f"[{Colors.PASS}]{diff:+.1f}%[/]"
            elif diff <= 20:
                diff_str = f"[{Colors.WARNING}]{diff:+.1f}%[/]"
            else:
                diff_str = f"[{Colors.FAIL}]{diff:+.1f}%[/]"

            table.add_row(
                str(p),
                f"{t_arts:.4f}s" if t_arts < 1 else f"{t_arts:.2f}s",
                f"{t_omp:.4f}s" if t_omp < 1 else f"{t_omp:.2f}s",
                f"{speedup:.2f}x",
                f"{eff:.1%}",
                diff_str
            )

        console.print(table)
        console.print()


def _print_hotspot_table(edts: Dict, dbs: Dict, meta: Dict, stride: int, top: int) -> None:
    """Print hotspot ranking tables."""
    # EDT hotspots by total execution time
    if edts:
        edt_rows = []
        for aid, rec in edts.items():
            base = aid // stride if stride > 0 else aid
            meta_entry = meta.get(base, {})
            loc = meta_entry.get("location", "")
            # Show base arts_id if no location found (helps debugging metadata gaps)
            if not loc:
                loc = f"[dim]base={base}[/]"
            edt_rows.append({
                "arts_id": aid,
                "base": base,
                "invocations": rec["invocations"],
                "total_ns": rec["total_exec_ns"],
                "avg_ns": rec["total_exec_ns"] // max(1, rec["invocations"]),
                "location": loc,
            })
        edt_rows.sort(key=lambda r: r["total_ns"], reverse=True)

        table = Table(title=f"EDT Hotspots (Top {min(top, len(edt_rows))})")
        table.add_column("Rank", justify="right")
        table.add_column("arts_id", justify="right")
        table.add_column("Invocations", justify="right")
        table.add_column("Total Time", justify="right")
        table.add_column("Avg Time", justify="right")
        table.add_column("Location / Source")

        for i, r in enumerate(edt_rows[:top], 1):
            table.add_row(
                str(i),
                str(r["arts_id"]),
                f"{r['invocations']:,}",
                f"{r['total_ns']/1e9:.3f}s",
                f"{r['avg_ns']/1e6:.2f}ms",
                r["location"],
            )
        console.print(table)
        console.print()

    # DB hotspots by remote bytes (prioritized)
    if dbs:
        db_rows = []
        for aid, rec in dbs.items():
            meta_entry = meta.get(aid, {})
            alloc = meta_entry.get("allocation", "")
            # Show arts_id if no allocation found
            if not alloc:
                alloc = f"[dim]id={aid}[/]"
            db_rows.append({
                "arts_id": aid,
                "invocations": rec["invocations"],
                "local_mb": rec["bytes_local"] / (1024**2),
                "remote_mb": rec["bytes_remote"] / (1024**2),
                "allocation": alloc,
            })
        db_rows.sort(key=lambda r: (
            r["remote_mb"], r["local_mb"]), reverse=True)

        table = Table(
            title=f"DB Hotspots (Top {min(top, len(db_rows))} by Remote)")
        table.add_column("Rank", justify="right")
        table.add_column("arts_id", justify="right")
        table.add_column("Invocations", justify="right")
        table.add_column("Local (MB)", justify="right")
        table.add_column("Remote (MB)", justify="right")
        table.add_column("Allocation / Source")

        for i, r in enumerate(db_rows[:top], 1):
            table.add_row(
                str(i),
                str(r["arts_id"]),
                f"{r['invocations']:,}",
                f"{r['local_mb']:.1f}",
                f"{r['remote_mb']:.1f}",
                r["allocation"],
            )
        console.print(table)


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
):
    """Run example in Docker containers."""
    docker_dir, _ = _get_docker_script("run")

    cmd = ["./docker-run.sh"]
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
