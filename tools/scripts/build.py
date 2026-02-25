"""Build command for CARTS CLI."""

from pathlib import Path
from typing import Optional

import typer

from carts_styles import Colors, console, print_header, print_step, print_error, print_success
from scripts.config import get_config
from scripts.deps import check_dependency, Dependency, DepStatus
from scripts.run import run_subprocess


# Counter profile mapping: level -> config file name
COUNTER_PROFILES = {
    0: "profile-none.cfg",       # All OFF - baseline performance
    1: "profile-timing.cfg",     # Timing only - minimal overhead (DEFAULT)
    2: "profile-workload.cfg",   # Workload characterization at CLUSTER level
    3: "profile-overhead.cfg",   # Full overhead analysis at CLUSTER level
}


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
    cc: Optional[str] = typer.Option(
        None, "--cc",
        help="C compiler for LLVM bootstrap (default: clang)"),
    cxx: Optional[str] = typer.Option(
        None, "--cxx",
        help="C++ compiler for LLVM bootstrap (default: clang++)"),
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
            # Use profile based on counters_level
            profile_name = COUNTER_PROFILES.get(
                counters_level, "profile-timing.cfg")
            effective_counter_config = (
                config.carts_dir
                / "external"
                / "carts-benchmarks"
                / "configs"
                / "profiles"
                / profile_name
            )
            console.print(f"Counter profile: [{Colors.INFO}]{profile_name}[/{Colors.INFO}]")
        make_vars.append(f"COUNTER_CONFIG_PATH={effective_counter_config}")

    # Pass platform-specific paths to make
    if config.linker_path:
        make_vars.append(f'CARTS_LINKER_PATH={config.linker_path}')
    if config.gcc_install_prefix:
        make_vars.append(f'LLVM_GCC_INSTALL_PREFIX={config.gcc_install_prefix}')

    # Pass bootstrap compiler overrides to make
    if cc:
        make_vars.append(f'LLVM_C_COMPILER={cc}')
    if cxx:
        make_vars.append(f'LLVM_CXX_COMPILER={cxx}')

    cmake_dep = Dependency(name="cmake", check_cmd="cmake", min_version=(3, 20))
    cmake_result = check_dependency(cmake_dep)
    if cmake_result.status == DepStatus.OK and cmake_result.found_path:
        make_vars.append(f"CMAKE={cmake_result.found_path}")

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

    # If polygeist was built and carts has been built before, rebuild carts-compile
    # to pick up the new polygeist. Skip on first install (build dir won't exist yet).
    carts_build_dir = config.carts_dir / "build"
    if result.returncode == 0 and target == "polygeist" and (carts_build_dir / "build.ninja").is_file():
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
