"""Build command for CARTS CLI."""

import os
from pathlib import Path
import subprocess
from typing import Optional

from dekk import (
    Colors,
    console,
    print_header,
    print_step,
    print_error,
    print_success,
    Exit,
    Option,
)
from scripts.platform import get_config
from scripts import (
    run_subprocess,
    MAKE_TARGET_ARTS,
    MAKE_TARGET_BUILD,
    MAKE_TARGET_LLVM,
    MAKE_TARGET_POLYGEIST,
    SUBMODULE_BENCHMARKS,
)
from scripts.build_env import (
    build_parallel_env,
    build_process_env,
    configured_make_command,
    configured_make_vars,
    make_command_with_vars,
    ENV_CARTS_BUILD_JOBS,
)

_CLEAN_TARGETS = {
    MAKE_TARGET_BUILD: "clean",
    MAKE_TARGET_ARTS: "arts-clean",
    MAKE_TARGET_POLYGEIST: "polygeist-clean",
    MAKE_TARGET_LLVM: "llvm-clean",
}


# Counter profile mapping: level -> config file name
COUNTER_PROFILES = {
    0: "profile-none.cfg",       # All OFF - baseline performance
    1: "profile-timing.cfg",     # Timing only - minimal overhead (DEFAULT)
    2: "profile-workload.cfg",   # Workload characterization at CLUSTER level
    3: "profile-overhead.cfg",   # Full overhead analysis at CLUSTER level
}


def _check_rdma_provider_deps() -> None:
    """Ensure a production RDMA build has its provider dev packages.

    Delegates to tools/scripts/ensure-rdma-deps.sh (the single source of truth,
    also run by the Makefile `arts:` target), which probes pkg-config and
    auto-installs librdmacm-dev/libucx-dev/libfabric-dev on Debian/Ubuntu. The
    build fails with guidance only when auto-install is unavailable.
    """
    script = Path(__file__).resolve().parent / "ensure-rdma-deps.sh"
    if not script.is_file():
        return
    if subprocess.run(["bash", str(script)], check=False).returncode != 0:
        raise Exit(1)


# ARTS multinode transport kinds. GASNet-EX is the production default on
# Linux; rsocket is an explicit legacy fallback only; TCP is the debug escape.
ARTS_TRANSPORT_GASNET = "gasnet"
ARTS_TRANSPORT_RSOCKET = "rsocket"
ARTS_TRANSPORT_TCP = "tcp"


def _select_arts_transport(config, rdma: Optional[bool], legacy_rsocket: bool) -> str:
    """Resolve the ARTS data-plane transport for an --arts build.

    Decision order (matches AGENTS.md / runtime.md): --no-rdma is the explicit
    TCP/debug escape and wins; --legacy-rsocket selects the legacy rsocket data
    plane; otherwise the production accelerated transport is GASNet-EX, which is
    the Linux default and is also selected by an explicit --rdma. macOS developer
    builds fall back to TCP unless an accelerated transport is requested.
    """
    if rdma is False:
        if legacy_rsocket:
            print_error("--no-rdma and --legacy-rsocket are mutually exclusive.")
            raise Exit(1)
        return ARTS_TRANSPORT_TCP
    if legacy_rsocket:
        return ARTS_TRANSPORT_RSOCKET
    if rdma is True:
        return ARTS_TRANSPORT_GASNET
    return ARTS_TRANSPORT_TCP if config.info.is_macos else ARTS_TRANSPORT_GASNET


def _apply_arts_transport_make_vars(
    transport: str, make_vars: list, no_bootstrap: bool = False
) -> None:
    """Append the make variables that realize the chosen ARTS transport.

    GASNet needs no external configuration by default: when no ARTS_GASNET_PREFIX
    is given, the ARTS build downloads its release tarball, auto-detects the
    conduit, installs its dependencies, and builds it into the build tree. An
    external prefix overrides the bootstrap; --gasnet-no-bootstrap disables it and
    then requires a prefix (fail-closed).
    """
    if transport == ARTS_TRANSPORT_GASNET:
        prefix = os.environ.get("ARTS_GASNET_PREFIX", "").strip()
        conduit = os.environ.get("ARTS_GASNET_CONDUIT", "").strip()
        threadmode = os.environ.get("ARTS_GASNET_THREADMODE", "").strip()
        version = os.environ.get("ARTS_GASNET_VERSION", "").strip()

        if no_bootstrap and not prefix:
            print_error(
                "--gasnet-no-bootstrap requires a prebuilt GASNet: set "
                "ARTS_GASNET_PREFIX=<gasnet-install> (and optionally "
                "ARTS_GASNET_CONDUIT). Drop --gasnet-no-bootstrap to download and "
                "build GASNet automatically, or use --no-rdma for the TCP runtime."
            )
            raise Exit(1)

        if prefix:
            source = f"external prefix={prefix}"
        else:
            source = f"bootstrap (download {version or 'default release'})"
        console.print(
            f"Network: [{Colors.INFO}]GASNet-EX "
            f"(conduit={conduit or 'auto'}, {source})[/{Colors.INFO}]"
        )

        make_vars.extend(["ARTS_USE_GASNET=ON", "ARTS_USE_RDMA=OFF"])
        make_vars.append(f"ARTS_GASNET_BOOTSTRAP={'OFF' if no_bootstrap else 'ON'}")
        # Pass through only what the user set; the ARTS build supplies the rest
        # (auto-detected conduit, pinned version, par threadmode).
        if prefix:
            make_vars.append(f"ARTS_GASNET_PREFIX={prefix}")
        if conduit:
            make_vars.append(f"ARTS_GASNET_CONDUIT={conduit}")
        if threadmode:
            make_vars.append(f"ARTS_GASNET_THREADMODE={threadmode}")
        if version:
            make_vars.append(f"ARTS_GASNET_VERSION={version}")
    elif transport == ARTS_TRANSPORT_RSOCKET:
        console.print(
            f"Network: [{Colors.INFO}]rsocket RDMA (legacy fallback)[/{Colors.INFO}]"
        )
        _check_rdma_provider_deps()
        make_vars.extend(["ARTS_USE_GASNET=OFF", "ARTS_USE_RDMA=ON"])
    else:  # ARTS_TRANSPORT_TCP
        console.print(
            f"Network: [{Colors.INFO}]TCP (debug/escape transport)[/{Colors.INFO}]"
        )
        make_vars.extend(["ARTS_USE_GASNET=OFF", "ARTS_USE_RDMA=OFF"])


def build(
    clean: bool = Option(False, "--clean", "-c", help="Run make clean before building"),
    arts: bool = Option(False, "--arts", "-a", help="Build only ARTS (mutually exclusive target flag)"),
    polygeist: bool = Option(
        False, "--polygeist", "-p", help="Build only Polygeist (mutually exclusive target flag)"),
    llvm: bool = Option(False, "--llvm", "-l", help="Build only LLVM (mutually exclusive target flag)"),
    debug_level: int = Option(
        1, "--debug", min=0, max=3,
        help="ARTS log level (--arts only): 0=error, 1=warn, 2=info, 3=debug"),
    counters_level: int = Option(
        0, "--counters",
        help="Counter profile (--arts only): 0=off, 1=timing, 2=workload, 3=overhead"),
    profile: Optional[Path] = Option(
        None, "--profile",
        help="Custom counter profile file path (overrides --counters)"),
    rdma: Optional[bool] = Option(
        None, "--rdma/--no-rdma",
        help="Production accelerated transport: --rdma selects GASNet-EX "
             "(Linux default ON; macOS default OFF); --no-rdma is the TCP/debug "
             "escape (--arts only)"),
    legacy_rsocket: bool = Option(
        False, "--legacy-rsocket",
        help="Use the legacy rsocket RDMA data plane instead of the default "
             "GASNet-EX production transport (--arts only)"),
    gasnet_no_bootstrap: bool = Option(
        False, "--gasnet-no-bootstrap",
        help="Do not download/build GASNet; require a prebuilt ARTS_GASNET_PREFIX "
             "(--arts only)"),
    cc: Optional[str] = Option(
        None, "--cc",
        help="C compiler for LLVM bootstrap (default: clang; use gcc on systems without clang)"),
    cxx: Optional[str] = Option(
        None, "--cxx",
        help="C++ compiler for LLVM bootstrap (default: clang++; use g++ with --cc gcc)"),
):
    """Build CARTS project using system clang."""
    config = get_config()

    print_header("CARTS Build")
    console.print(
        f"Platform: [{Colors.INFO}]{config.info.os}[/{Colors.INFO}] ({config.info.arch})")

    makefile = config.carts_dir / "Makefile"
    if not makefile.is_file():
        print_error("Makefile not found. Are you in the CARTS project root?")
        raise Exit(1)

    selected_targets = sum((1 if arts else 0, 1 if polygeist else 0, 1 if llvm else 0))
    if selected_targets > 1:
        print_error("Choose only one target flag among --arts, --polygeist, --llvm.")
        raise Exit(1)

    # Determine build target
    if arts:
        target = MAKE_TARGET_ARTS
    elif polygeist:
        target = MAKE_TARGET_POLYGEIST
    elif llvm:
        target = MAKE_TARGET_LLVM
    else:
        target = MAKE_TARGET_BUILD

    console.print(f"Target: [{Colors.INFO}]{target}[/{Colors.INFO}]")

    # Build make variables
    make_vars = configured_make_vars(config)

    if arts:
        transport = _select_arts_transport(config, rdma, legacy_rsocket)
        _apply_arts_transport_make_vars(transport, make_vars, gasnet_no_bootstrap)
        # Expose the raw v2 ARTS runtime levels directly:
        #   0 -> ERROR only
        #   1 -> WARN
        #   2 -> INFO
        #   3 -> DEBUG (+ Debug build)
        make_vars.append(f"ARTS_LOG_LEVEL={debug_level}")
        if debug_level >= 3:
            make_vars.extend([
                "ARTS_BUILD_TYPE=Debug",
            ])

    # Counter levels: 0=off, 1=artsid, 2=deep
    # Levels 1+ require USE_COUNTERS and USE_METRICS
    if counters_level >= 1 or profile is not None:
        make_vars.extend(["ARTS_USE_COUNTERS=ON", "ARTS_USE_METRICS=ON"])

    # Always pass counter config path for ARTS builds
    if arts:
        if profile:
            # Use custom profile if provided
            if not profile.exists():
                print_error(f"Profile not found: {profile}")
                raise Exit(1)
            effective_counter_config = profile.resolve()
            console.print(f"Profile: [{Colors.INFO}]{profile}[/{Colors.INFO}]")
        else:
            # Use profile based on counters_level
            profile_name = COUNTER_PROFILES.get(
                counters_level, "profile-timing.cfg")
            effective_counter_config = (
                config.carts_dir
                / SUBMODULE_BENCHMARKS
                / "configs"
                / "profiles"
                / profile_name
            )
            console.print(f"Counter profile: [{Colors.INFO}]{profile_name}[/{Colors.INFO}]")
        make_vars.append(f"COUNTER_CONFIG_PATH={effective_counter_config}")

    # Pass platform-specific paths to make
    # Pass bootstrap compiler overrides to make
    if cc:
        make_vars.append(f'LLVM_C_COMPILER={cc}')
    if cxx:
        make_vars.append(f'LLVM_CXX_COMPILER={cxx}')

    if make_vars:
        console.print(f"Options: [{Colors.DEBUG}]{' '.join(make_vars)}[/{Colors.DEBUG}]")
    parallel_env = build_process_env(config)
    console.print(
        f"Parallel jobs: [{Colors.INFO}]{parallel_env[ENV_CARTS_BUILD_JOBS]}[/{Colors.INFO}]"
    )

    console.print()

    # Clean if requested
    if clean:
        print_step("Cleaning previous build...")
        clean_target = _CLEAN_TARGETS[target]
        run_subprocess(
            configured_make_command(config, clean_target),
            cwd=config.carts_dir,
            env=parallel_env,
            check=False,
        )

    # Run build
    print_step(f"Building {target}...")
    cmd = make_command_with_vars(make_vars, target)

    result = run_subprocess(
        cmd,
        cwd=config.carts_dir,
        env=parallel_env,
        check=False,
    )

    if result.returncode == 0:
        print_header("Build Complete")
        print_success("CARTS build completed successfully!")
    else:
        print_header("Build Failed")
        print_error("CARTS build failed!")
        raise Exit(1)
