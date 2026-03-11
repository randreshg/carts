"""Compile pipeline commands for CARTS CLI."""

from dataclasses import dataclass, field
from contextlib import contextmanager
import os
from pathlib import Path
from typing import List, Optional

import typer
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TaskProgressColumn

from carts_styles import Colors, console, print_header, print_error, print_info, print_success
from scripts.config import PlatformConfig, get_config
from scripts.run import run_subprocess, run_command_with_output

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


@contextmanager
def _pushd(path: Path):
    """Temporarily change working directory."""
    original = Path.cwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(original)


def _get_compile_workdir_override() -> Optional[Path]:
    """Return compile workdir override from environment, if set."""
    raw = os.environ.get("CARTS_COMPILE_WORKDIR")
    if not raw:
        return None
    workdir = Path(raw).expanduser().resolve()
    workdir.mkdir(parents=True, exist_ok=True)
    return workdir


_PATH_VALUE_FLAGS = {
    "-I",
    "-isystem",
    "-iquote",
    "-idirafter",
    "-include",
    "-L",
    "-F",
    "-isysroot",
    "--sysroot",
}
_PATH_VALUE_PREFIXES = (
    "-I",
    "-L",
    "-F",
    "-isystem",
    "-iquote",
    "-idirafter",
    "-include",
)
_PATH_EQUALS_PREFIXES = ("--sysroot=",)


def _resolve_relative_cli_path(token: str, base_dir: Path) -> str:
    """Resolve a path-valued CLI token relative to the original invocation cwd."""
    path = Path(token)
    if path.is_absolute():
        return str(path)
    return str((base_dir / path).resolve())


def _normalize_path_flags(args: List[str], base_dir: Path) -> List[str]:
    """Stabilize path-valued compiler/linker flags before changing cwd."""
    normalized: List[str] = []
    i = 0
    while i < len(args):
        arg = args[i]
        if arg in _PATH_VALUE_FLAGS and i + 1 < len(args):
            normalized.append(arg)
            i += 1
            normalized.append(_resolve_relative_cli_path(args[i], base_dir))
        elif any(arg.startswith(prefix) for prefix in _PATH_EQUALS_PREFIXES):
            prefix, value = arg.split("=", 1)
            normalized.append(f"{prefix}={_resolve_relative_cli_path(value, base_dir)}")
        else:
            matched_prefix = None
            for prefix in _PATH_VALUE_PREFIXES:
                if arg.startswith(prefix) and len(arg) > len(prefix):
                    matched_prefix = prefix
                    break
            if matched_prefix is not None:
                normalized.append(
                    matched_prefix
                    + _resolve_relative_cli_path(arg[len(matched_prefix):], base_dir)
                )
            else:
                normalized.append(arg)
        i += 1
    return normalized


def _is_numeric(s: str) -> bool:
    """Check if a string is a numeric value (including negative)."""
    try:
        float(s)
        return True
    except ValueError:
        return False


@dataclass
class CompileArgs:
    """Parsed passthrough arguments, classified by pipeline stage.

    Classification is pattern-based so that new ``cl::opt`` flags added to
    ``carts-compile.cpp`` are automatically forwarded without any Python
    changes.  The rules are:

    1. A handful of *special* flags are consumed by the Python CLI itself
       (``-O3``, ``--diagnose``, ``--diagnose-output``).
    2. Linker flags are recognised by well-known prefixes/names
       (``-l``, ``-L``, ``-Wl,``, ``-Xlinker``, ``-framework``).
    3. Short single-dash flags (``-I``, ``-D``, ``-std``, ``-f``, …) go to
       cgeist.
    4. Everything else — i.e. any ``--`` long flag — is forwarded to the
       carts-compile pipeline binary.
    """

    cgeist: List[str] = field(default_factory=list)    # -I, -D, -std, etc.
    pipeline: List[str] = field(default_factory=list)   # --arts-config, --partition-*, etc.
    link: List[str] = field(default_factory=list)        # -l, -L, -Wl, -framework, etc.
    optimize: bool = False                               # -O3 passthrough
    diagnose: bool = False                               # --diagnose passthrough
    diagnose_output: Optional[Path] = None               # --diagnose-output passthrough

    # Linker flag prefixes (single-dash)
    _LINKER_PREFIXES = ("-l", "-L", "-Wl,")
    # Linker flags that consume the next argument
    _LINKER_VALUE_FLAGS = {"-Xlinker", "-framework"}

    @classmethod
    def parse(cls, args: List[str]) -> "CompileArgs":
        result = cls()
        i = 0
        while i < len(args):
            arg = args[i]
            # --- Special flags consumed by the Python CLI ---
            if arg == "-O3":
                result.optimize = True
            elif arg == "--diagnose":
                result.diagnose = True
            elif arg == "--diagnose-output":
                if i + 1 < len(args):
                    i += 1
                    result.diagnose_output = Path(args[i])
            # --- Linker flags ---
            elif arg.startswith(cls._LINKER_PREFIXES):
                result.link.append(arg)
            elif arg in cls._LINKER_VALUE_FLAGS:
                result.link.append(arg)
                if i + 1 < len(args):
                    i += 1
                    result.link.append(args[i])
            # --- Long flags (--*) -> pipeline (carts-compile binary) ---
            elif arg.startswith("--"):
                result.pipeline.append(arg)
                # If no "=", the next non-flag token is the value.
                if "=" not in arg and i + 1 < len(args):
                    nxt = args[i + 1]
                    if not nxt.startswith("-") or _is_numeric(nxt):
                        i += 1
                        result.pipeline.append(nxt)
            # --- Everything else -> cgeist (-I, -D, -std, -f, …) ---
            else:
                result.cgeist.append(arg)
            i += 1
        return result


# ============================================================================
# Cgeist Command
# ============================================================================

def cgeist(
    ctx: typer.Context,
    input_file: Path = typer.Argument(..., help="Input C/C++ file"),
):
    """Run cgeist with automatic include paths."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    cgeist_bin = config.get_polygeist_tool("cgeist")
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

    clang_bin = config.get_llvm_tool("clang")
    if not clang_bin.is_file():
        print_error(f"LLVM clang not found at {clang_bin}")
        raise typer.Exit(1)

    # Build command
    cmd = [str(clang_bin)]
    cmd.extend(config.runtime_flags)
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

def mlir_translate(
    args: List[str] = typer.Argument(..., help="Arguments for mlir-translate"),
):
    """Run mlir-translate."""
    config = get_config()

    mlir_translate_bin = config.get_llvm_tool("mlir-translate")
    cmd = [str(mlir_translate_bin)] + args

    result = run_subprocess(cmd, check=False)
    raise typer.Exit(result.returncode)


# ============================================================================
# Compile Command (unified)
# ============================================================================

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
):
    """Unified compilation command.

    Routes based on input file type:

      .c/.cpp  Full pipeline: C -> MLIR -> LLVM IR -> executable

      .mlir    MLIR transformation pipeline via carts-compile binary

      .ll      Link LLVM IR with ARTS runtime -> executable

    Use --pipeline <stage> to stop after a specific pipeline stage.
    """
    ext = input_file.suffix.lower()
    if ext in (".c", ".cpp"):
        _compile_from_c(ctx, input_file, output, optimize, debug, pipeline,
                        all_stages, emit_llvm, collect_metadata,
                        partition_fallback, diagnose, diagnose_output)
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
) -> None:
    """Full pipeline for C/C++ input."""
    config = get_config()
    # Resolve early so changing cwd for artifact isolation does not break input lookup.
    input_file = input_file.resolve()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    base_name = input_file.stem
    output_name = output if output else Path(f"{base_name}_arts")

    # Parse passthrough args
    parsed = CompileArgs.parse(ctx.args or [])
    invocation_cwd = Path.cwd().resolve()

    extra_pipeline_args = parsed.pipeline[:]
    extra_link_args = _normalize_path_flags(parsed.link[:], invocation_cwd)
    cgeist_args = _normalize_path_flags(parsed.cgeist, invocation_cwd)

    if partition_fallback:
        extra_pipeline_args.append(f"--partition-fallback={partition_fallback}")

    use_dual_mode = optimize or parsed.optimize
    use_diagnose = diagnose or parsed.diagnose
    final_diagnose_output = diagnose_output or parsed.diagnose_output

    workdir_override = _get_compile_workdir_override()
    if workdir_override is None:
        if use_dual_mode:
            _compile_c_dual(config, input_file, output_name, debug,
                            extra_pipeline_args, extra_link_args, cgeist_args,
                            use_diagnose, final_diagnose_output, pipeline)
        else:
            _compile_c_simple(config, input_file, output_name, debug,
                              extra_pipeline_args, extra_link_args, cgeist_args,
                              use_diagnose, final_diagnose_output, pipeline)
        return

    with _pushd(workdir_override):
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
    carts_compile_bin = config.get_carts_tool("carts-compile")

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
    cmd = [str(config.get_polygeist_tool("cgeist"))]
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
    cmd = [str(config.get_llvm_tool("clang"))]
    cmd.extend(config.compile_flags)  # includes runtime_flags
    cmd.extend(config.clang_sysroot_flags)
    cmd.extend(config.compile_library_flags)
    cmd.extend(config.linker_flags)
    cmd.append(str(input_file))
    cmd.extend(["-o", str(output_file)])
    if debug:
        cmd.append("-g")
    cmd.extend(config.compile_libraries)
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

    carts_compile_bin = config.get_carts_tool("carts-compile")
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
        f"Mode:   [{Colors.WARNING}]Dual compilation[/{Colors.WARNING}]")
    console.print()

    carts_compile_bin = config.get_carts_tool("carts-compile")

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
            from carts_styles import print_warning
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

    carts_compile_bin = config.get_carts_tool("carts-compile")

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
