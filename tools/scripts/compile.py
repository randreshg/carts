"""Compile pipeline commands for CARTS CLI."""

from dataclasses import dataclass, field
from contextlib import contextmanager
import json
import os
from pathlib import Path
from typing import Dict, List, Optional

from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TaskProgressColumn
from rich.table import Table
from dekk import Argument, Colors, Context, Exit, Option, console, print_header, print_error, print_info, print_success, print_warning
from scripts.platform import CartsConfig, get_config, is_verbose
from scripts import (
    run_subprocess,
    run_command_with_output,
    ARTS_CONFIG_FILENAME,
    TOOL_CARTS_COMPILE,
    TOOL_CGEIST,
    TOOL_CLANG,
)

# Compiler flags used across multiple functions
FLAG_ARTS_CONFIG = "--arts-config"
FLAG_DIAGNOSE = "--diagnose"
FLAG_DIAGNOSE_OUTPUT = "--diagnose-output"
FLAG_ARTS_DEBUG = "--arts-debug"
FLAG_DEBUG_ONLY = "--debug-only"
FLAG_PIPELINE = "--pipeline"
FLAG_OUTPUT = "-o"
FLAG_METADATA_FILE = "--metadata-file"
DEFAULT_METADATA_FILENAME = ".carts-metadata.json"


@dataclass(frozen=True)
class PipelineTokens:
    pipeline: List[str]
    start_from: List[str]
    pipeline_sequence: List[str]


@dataclass(frozen=True)
class PipelineManifestStep:
    name: str
    passes: List[str]


@dataclass(frozen=True)
class PipelineManifest:
    tokens: PipelineTokens
    steps: List[PipelineManifestStep]
    epilogue_steps: List[PipelineManifestStep]


def _parse_manifest_steps_field(
    data: Dict[str, object], key: str
) -> List[PipelineManifestStep]:
    """Parse a manifest step-list field."""
    steps_raw = data.get(key)
    if steps_raw is None:
        raise ValueError(f"field '{key}' must be a list")
    if not isinstance(steps_raw, list):
        raise ValueError(f"field '{key}' must be a list")

    steps: List[PipelineManifestStep] = []
    for step_raw in steps_raw:
        if not isinstance(step_raw, dict):
            raise ValueError(f"each '{key}' entry must be an object")
        name = step_raw.get("name")
        passes = step_raw.get("passes", [])
        if not isinstance(name, str):
            raise ValueError(f"manifest step field '{key}.name' must be a string")
        if not isinstance(passes, list) or not all(isinstance(p, str) for p in passes):
            raise ValueError(
                f"manifest step field '{key}.passes' must be a list of strings"
            )
        steps.append(PipelineManifestStep(name=name, passes=list(passes)))
    return steps


_PIPELINE_MANIFEST_CACHE: Optional[PipelineManifest] = None
_DISALLOWED_PIPELINE_FLAGS = {
    FLAG_DEBUG_ONLY: FLAG_ARTS_DEBUG,
}



def _normalize_arts_debug(spec: str) -> str:
    """Normalize `--arts-debug` channels and reject empty specs."""
    channels = [channel.strip() for channel in spec.split(",") if channel.strip()]
    if not channels:
        raise ValueError(
            "Invalid --arts-debug value. Use comma-separated channels, "
            "for example --arts-debug=info,debug."
        )
    return ",".join(channels)


def _has_flag(args: List[str], flag: str) -> bool:
    """Return True when args contain a `--flag` token in either form."""
    for arg in args:
        if arg == flag or arg.startswith(f"{flag}="):
            return True
    return False


def _parse_pipeline_tokens_payload(payload: str) -> PipelineTokens:
    """Parse pipeline token arrays from the compiler manifest JSON."""
    data = json.loads(payload)
    if not isinstance(data, dict):
        raise ValueError("root JSON object must be a map")

    def require_string_list(key: str) -> List[str]:
        value = data.get(key)
        if not isinstance(value, list) or not all(isinstance(x, str) for x in value):
            raise ValueError(f"field '{key}' must be a list of strings")
        return list(dict.fromkeys(value))

    pipeline = require_string_list("pipeline")
    start_from = require_string_list("start_from")
    pipeline_sequence = require_string_list("pipeline_sequence")

    if not pipeline_sequence:
        raise ValueError("field 'pipeline_sequence' cannot be empty")
    if not start_from:
        raise ValueError("field 'start_from' cannot be empty")
    if not pipeline:
        raise ValueError("field 'pipeline' cannot be empty")

    return PipelineTokens(
        pipeline=pipeline,
        start_from=start_from,
        pipeline_sequence=pipeline_sequence,
    )


def _parse_pipeline_manifest_payload(payload: str) -> PipelineManifest:
    """Parse `carts-compile --print-pipeline-manifest-json` output."""
    tokens = _parse_pipeline_tokens_payload(payload)
    data = json.loads(payload)
    steps = _parse_manifest_steps_field(data, "pipeline_steps")
    epilogue_steps = _parse_manifest_steps_field(data, "epilogue_steps")

    return PipelineManifest(
        tokens=tokens,
        steps=steps,
        epilogue_steps=epilogue_steps,
    )


def _pipeline_manifest_to_dict(manifest: PipelineManifest) -> Dict[str, object]:
    """Convert pipeline manifest dataclass into a JSON-serializable dict."""
    payload: Dict[str, object] = {
        "pipeline": manifest.tokens.pipeline,
        "start_from": manifest.tokens.start_from,
        "pipeline_sequence": manifest.tokens.pipeline_sequence,
        "pipeline_steps": [
            {"name": step.name, "passes": step.passes}
            for step in manifest.steps
        ],
        "epilogue_steps": [
            {"name": step.name, "passes": step.passes}
            for step in manifest.epilogue_steps
        ],
    }
    return payload


def _query_compiler_json(
    config: CartsConfig, flag: str, error_message: str
) -> str:
    """Run carts-compile JSON endpoint and return stdout payload."""
    carts_compile_bin = config.get_carts_tool(TOOL_CARTS_COMPILE)
    if not carts_compile_bin.is_file():
        print_error(f"carts-compile not found at {carts_compile_bin}")
        raise Exit(1)

    result = run_subprocess(
        [str(carts_compile_bin), flag],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        print_error(error_message)
        raise Exit(result.returncode)
    return result.stdout or ""


def _get_pipeline_manifest(config: CartsConfig) -> PipelineManifest:
    """Load pipeline manifest from carts-compile once per process."""
    global _PIPELINE_MANIFEST_CACHE
    if _PIPELINE_MANIFEST_CACHE is not None:
        return _PIPELINE_MANIFEST_CACHE

    try:
        payload = _query_compiler_json(
            config,
            "--print-pipeline-manifest-json",
            "Failed to query pipeline manifest from carts-compile. "
            "Run `dekk carts build` and retry.",
        )
        _PIPELINE_MANIFEST_CACHE = _parse_pipeline_manifest_payload(payload)
    except (json.JSONDecodeError, ValueError) as exc:
        print_error(f"Invalid pipeline manifest payload from carts-compile: {exc}")
        raise Exit(1)

    return _PIPELINE_MANIFEST_CACHE


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


def _drop_flag_and_value(args: List[str], flag: str) -> List[str]:
    """Remove `--flag` tokens from passthrough args, including optional value."""
    filtered: List[str] = []
    i = 0
    while i < len(args):
        arg = args[i]
        if arg == flag:
            i += 1
            if i < len(args):
                next_arg = args[i]
                if not next_arg.startswith("-") or _is_numeric(next_arg):
                    i += 1
            continue
        if arg.startswith(f"{flag}="):
            i += 1
            continue
        filtered.append(arg)
        i += 1
    return filtered


def _reject_disallowed_pipeline_flags(args: List[str]) -> None:
    """Reject non-canonical flags that bypass ARTS compile conventions."""
    for disallowed, replacement in _DISALLOWED_PIPELINE_FLAGS.items():
        if _has_flag(args, disallowed):
            print_error(f"Unsupported option: {disallowed}. Use {replacement}.")
            raise Exit(1)


@dataclass
class CompileArgs:
    """Parsed passthrough arguments, classified by pipeline step.

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
            if arg in ("-O3", "--O3"):
                result.optimize = True
            elif arg == FLAG_DIAGNOSE:
                result.diagnose = True
            elif arg == FLAG_DIAGNOSE_OUTPUT:
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
    ctx: Context,
    input_file: Path = Argument(..., help="Input C/C++ file"),
):
    """Run cgeist with automatic include paths."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise Exit(1)

    cgeist_bin = config.get_polygeist_tool(TOOL_CGEIST)
    if not cgeist_bin.is_file():
        print_error(f"cgeist not found at {cgeist_bin}")
        raise Exit(1)

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
            if arg == FLAG_ARTS_CONFIG:
                skip_next = True
                continue
            cmd.append(arg)

    cmd.append(str(input_file))

    result = run_subprocess(cmd, check=False)
    raise Exit(result.returncode)


# ============================================================================
# Clang Command
# ============================================================================

def clang(
    ctx: Context,
    input_file: Path = Argument(..., help="Input file"),
    output: Optional[Path] = Option(None, "-o", help="Output file"),
):
    """Compile C/C++ with LLVM clang and OpenMP."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise Exit(1)

    clang_bin = config.get_llvm_tool(TOOL_CLANG)
    if not clang_bin.is_file():
        print_error(f"LLVM clang not found at {clang_bin}")
        raise Exit(1)

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
    raise Exit(result.returncode)


# ============================================================================
# MLIR-Translate Command
# ============================================================================

def mlir_translate(
    ctx: Context,
    args: Optional[List[str]] = Argument(
        None, help="Arguments for mlir-translate"),
):
    """Run mlir-translate."""
    config = get_config()

    mlir_translate_bin = config.get_llvm_tool("mlir-translate")
    translated_args = list(args or [])
    if ctx.args:
        translated_args.extend(ctx.args)
    if not translated_args:
        print_error("Provide mlir-translate arguments, for example --help.")
        raise Exit(1)

    cmd = [str(mlir_translate_bin)] + translated_args

    result = run_subprocess(cmd, check=False)
    raise Exit(result.returncode)


# ============================================================================
# Pipeline Command
# ============================================================================

def pipeline(
    pipeline: Optional[str] = Option(
        None, FLAG_PIPELINE, help="Show passes for one pipeline step"),
    json_output: bool = Option(
        False, "--json", help="Print pipeline manifest JSON"),
):
    """Show pipeline steps and passes from carts-compile."""
    config = get_config()
    manifest = _get_pipeline_manifest(config)

    if pipeline and pipeline not in manifest.tokens.pipeline_sequence:
        print_error(f"Unknown pipeline step: '{pipeline}'")
        print_info("Available pipeline steps:")
        for token in manifest.tokens.pipeline_sequence:
            console.print(f"  - {token}")
        raise Exit(1)

    if json_output:
        console.print(json.dumps(_pipeline_manifest_to_dict(manifest), indent=2))
        return

    print_header("CARTS Pipeline")
    table = Table(title="Pipeline Order")
    table.add_column("#", style=Colors.INFO, justify="right")
    table.add_column("Pipeline", style=Colors.SUCCESS)
    table.add_column("Passes", style=Colors.STEP, justify="right")
    selected_pipelines = manifest.steps
    if pipeline:
        selected_pipelines = [entry for entry in manifest.steps if entry.name == pipeline]
    for index, entry in enumerate(manifest.steps, start=1):
        if pipeline and entry.name != pipeline:
            continue
        table.add_row(str(index), entry.name, str(len(entry.passes)))
    console.print(table)

    for entry in selected_pipelines:
        console.print()
        console.print(f"[{Colors.INFO}]{entry.name}[/{Colors.INFO}]")
        for pass_name in entry.passes:
            console.print(f"  - {pass_name}")


# ============================================================================
# Compile Command (unified)
# ============================================================================

def compile_cmd(
    ctx: Context,
    input_file: Path = Argument(..., help="Input file (.c/.cpp/.mlir/.ll)"),
    output: Optional[Path] = Option(
        None, "-o",
        help="Output path (file for normal compile; directory with --all-pipelines)"),
    optimize: bool = Option(
        False, "-O3", help="Enable O3 optimizations"),
    debug: bool = Option(False, "-g", help="Generate debug symbols"),
    pipeline: Optional[str] = Option(
        None, FLAG_PIPELINE,
        help="Stop after pipeline step (e.g., concurrency, edt-distribution)"),
    all_pipelines: bool = Option(
        False, "--all-pipelines",
        help="(.mlir input) write one output per pipeline step"),
    emit_llvm: bool = Option(
        False, "--emit-llvm", help="Emit LLVM IR output"),
    collect_metadata: bool = Option(
        False, "--collect-metadata", help="Collect and export metadata"),
    partition_fallback: Optional[str] = Option(
        None, "--partition-fallback",
        help="Partition fallback strategy (coarse, fine)"),
    diagnose: bool = Option(
        False, FLAG_DIAGNOSE, help="Export diagnostic information"),
    diagnose_output: Optional[Path] = Option(
        None, FLAG_DIAGNOSE_OUTPUT, help="Output file for diagnostics"),
    arts_debug: Optional[str] = Option(
        None, FLAG_ARTS_DEBUG,
        help="Comma-separated ARTS debug channels (example: info,debug)"),
    start_from: Optional[str] = Option(
        None, "--start-from",
        help="Start pipeline at specified step (input must already match that step)"),
):
    """Unified compilation command.

    Routes based on input file type:

      .c/.cpp  Full pipeline: C -> MLIR -> LLVM IR -> executable

      .mlir    MLIR transformation pipeline via carts-compile binary

      .ll      Link LLVM IR with ARTS runtime -> executable

    Use --pipeline <step> to stop after a specific pipeline step.
    """
    passthrough_args = list(ctx.args or [])
    _reject_disallowed_pipeline_flags(passthrough_args)
    if arts_debug is not None:
        passthrough_args = _drop_flag_and_value(passthrough_args, FLAG_ARTS_DEBUG)

    normalized_arts_debug: Optional[str] = None
    if arts_debug:
        try:
            normalized_arts_debug = _normalize_arts_debug(arts_debug)
        except ValueError as exc:
            print_error(str(exc))
            raise Exit(1)

    ext = input_file.suffix.lower()
    if ext == ".ll":
        if pipeline:
            print_error("--pipeline is not supported for .ll input (link-only)")
            raise Exit(1)
        if start_from:
            print_error("--start-from is not supported for .ll input (link-only)")
            raise Exit(1)
        if all_pipelines:
            print_error("--all-pipelines is only supported for .mlir input")
            raise Exit(1)
        if emit_llvm or collect_metadata or partition_fallback or diagnose or diagnose_output:
            print_error(
                "--emit-llvm, --collect-metadata, --partition-fallback, and diagnose "
                "options are not supported for .ll input (link-only)"
            )
            raise Exit(1)
        if normalized_arts_debug or _has_flag(passthrough_args, FLAG_ARTS_DEBUG):
            print_error("--arts-debug is only supported for carts-compile pipeline runs")
            raise Exit(1)
        _compile_from_ll(input_file, output, debug, passthrough_args)
        return

    if ext not in (".c", ".cpp", ".mlir"):
        print_error(
            f"Unsupported input '{input_file}'. Supported extensions: "
            ".c, .cpp, .mlir, .ll."
        )
        raise Exit(1)

    config = get_config()
    manifest = _get_pipeline_manifest(config)
    if pipeline and pipeline not in manifest.tokens.pipeline:
        print_error(f"Unknown pipeline step: '{pipeline}'")
        print_info("Available pipeline steps:")
        for pipeline_token in manifest.tokens.pipeline:
            console.print(f"  - {pipeline_token}")
        raise Exit(1)
    if start_from and start_from not in manifest.tokens.start_from:
        print_error(f"Unknown start-from pipeline step: '{start_from}'")
        print_info("Available start-from pipeline steps:")
        for pipeline_token in manifest.tokens.start_from:
            console.print(f"  - {pipeline_token}")
        raise Exit(1)

    if ext in (".c", ".cpp"):
        if all_pipelines:
            print_error("--all-pipelines is only supported for .mlir input")
            raise Exit(1)
        if emit_llvm:
            print_error("--emit-llvm is only supported for .mlir input")
            raise Exit(1)
        if collect_metadata:
            print_error("--collect-metadata is only supported for .mlir input")
            raise Exit(1)
        if start_from:
            print_error("--start-from is only supported for .mlir input")
            raise Exit(1)
        _compile_from_c(input_file, output, optimize, debug, pipeline,
                        partition_fallback, diagnose, diagnose_output,
                        normalized_arts_debug, passthrough_args)
    else:
        if all_pipelines and (pipeline or start_from):
            print_error("--all-pipelines cannot be combined with --pipeline or --start-from")
            raise Exit(1)
        _compile_from_mlir(input_file, output, optimize, debug, pipeline,
                           all_pipelines, emit_llvm, collect_metadata,
                           partition_fallback, diagnose, diagnose_output,
                           normalized_arts_debug,
                           start_from, passthrough_args)


# -----------------------------------------------------------------------------
# Compile: .c/.cpp input (full pipeline)
# -----------------------------------------------------------------------------

def _compile_from_c(
    input_file: Path,
    output: Optional[Path],
    optimize: bool,
    debug: bool,
    pipeline: Optional[str],
    partition_fallback: Optional[str],
    diagnose: bool,
    diagnose_output: Optional[Path],
    arts_debug: Optional[str],
    passthrough_args: List[str],
) -> None:
    """Full pipeline for C/C++ input."""
    config = get_config()
    # Resolve early so changing cwd for artifact isolation does not break input lookup.
    input_file = input_file.resolve()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise Exit(1)

    base_name = input_file.stem
    output_name = output if output else Path(f"{base_name}_arts")

    # Parse passthrough args
    parsed = CompileArgs.parse(passthrough_args)
    invocation_cwd = Path.cwd().resolve()

    extra_pipeline_args = parsed.pipeline[:]
    extra_link_args = _normalize_path_flags(parsed.link[:], invocation_cwd)
    cgeist_args = _normalize_path_flags(parsed.cgeist, invocation_cwd)

    if partition_fallback:
        extra_pipeline_args.append(f"{"--partition-fallback"}={partition_fallback}")
    if arts_debug:
        extra_pipeline_args.append(f"{FLAG_ARTS_DEBUG}={arts_debug}")

    enable_pipeline_o3 = optimize or parsed.optimize
    use_diagnose = diagnose or parsed.diagnose
    final_diagnose_output = diagnose_output or parsed.diagnose_output

    workdir_override = _get_compile_workdir_override()
    if workdir_override is None:
        _compile_c_pipeline(config, input_file, output_name, debug,
                            enable_pipeline_o3,
                            extra_pipeline_args, extra_link_args, cgeist_args,
                            use_diagnose, final_diagnose_output, pipeline)
        return

    with _pushd(workdir_override):
        _compile_c_pipeline(config, input_file, output_name, debug,
                            enable_pipeline_o3,
                            extra_pipeline_args, extra_link_args, cgeist_args,
                            use_diagnose, final_diagnose_output, pipeline)


# -----------------------------------------------------------------------------
# Compile: .mlir input (MLIR pipeline)
# -----------------------------------------------------------------------------

def _compile_from_mlir(
    input_file: Path,
    output: Optional[Path],
    optimize: bool,
    debug: bool,
    pipeline: Optional[str],
    all_pipelines: bool,
    emit_llvm: bool,
    collect_metadata: bool,
    partition_fallback: Optional[str] = None,
    diagnose: bool = False,
    diagnose_output: Optional[Path] = None,
    arts_debug: Optional[str] = None,
    start_from: Optional[str] = None,
    passthrough_args: Optional[List[str]] = None,
) -> None:
    """MLIR transformation pipeline."""
    config = get_config()
    carts_compile_bin = config.get_carts_tool(TOOL_CARTS_COMPILE)
    passthrough_args = passthrough_args or []
    metadata_file = _find_metadata_file(input_file, passthrough_args)

    # Pass --help through to carts-compile binary
    if "--help" in passthrough_args or "-h" in passthrough_args:
        result = run_subprocess([str(carts_compile_bin), "--help"], check=False)
        raise Exit(result.returncode)

    if all_pipelines:
        extra_args = list(passthrough_args)
        if metadata_file:
            extra_args.extend([FLAG_METADATA_FILE, str(metadata_file)])
        if arts_debug:
            extra_args.append(f"{FLAG_ARTS_DEBUG}={arts_debug}")
        _compile_all_pipelines(config, input_file, output, extra_args, optimize)
        return

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise Exit(1)

    cmd = [str(carts_compile_bin), str(input_file)]

    # Auto-discover arts.cfg if not explicitly provided
    arts_cfg = _find_arts_config(input_file, passthrough_args, config)
    if arts_cfg:
        cmd.extend([FLAG_ARTS_CONFIG, str(arts_cfg)])
    if metadata_file:
        cmd.extend([FLAG_METADATA_FILE, str(metadata_file)])

    if optimize:
        cmd.append("--O3")
    if emit_llvm:
        cmd.append("--emit-llvm")
    if collect_metadata:
        cmd.append("--collect-metadata")
    if debug:
        cmd.append("-g")
    if pipeline:
        cmd.append(f"{FLAG_PIPELINE}={pipeline}")
    if partition_fallback:
        cmd.append(f"{"--partition-fallback"}={partition_fallback}")
    if diagnose:
        cmd.append(FLAG_DIAGNOSE)
    if diagnose_output:
        cmd.extend([FLAG_DIAGNOSE_OUTPUT, str(diagnose_output)])
    if arts_debug:
        cmd.append(f"{FLAG_ARTS_DEBUG}={arts_debug}")
    if start_from:
        cmd.append(f"{"--start-from"}={start_from}")
    if output:
        cmd.extend([FLAG_OUTPUT, str(output)])
    if passthrough_args:
        cmd.extend(passthrough_args)

    result = run_subprocess(cmd, check=False)
    raise Exit(result.returncode)


# -----------------------------------------------------------------------------
# Compile: .ll input (link only)
# -----------------------------------------------------------------------------

def _compile_from_ll(
    input_file: Path,
    output: Optional[Path],
    debug: bool,
    passthrough_args: List[str],
) -> None:
    """Link LLVM IR with ARTS runtime."""
    config = get_config()

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise Exit(1)

    if output is None:
        print_error("-o <output> is required for .ll input")
        raise Exit(1)

    cmd = _build_link_cmd(
        config,
        input_file,
        output,
        debug,
        passthrough_args,
    )

    result = run_subprocess(cmd, check=False)
    raise Exit(result.returncode)


# -----------------------------------------------------------------------------
# Compile Pipeline Helpers
# -----------------------------------------------------------------------------

def _find_arts_config(
    input_file: Path, pipeline_args: List[str], config: Optional["CartsConfig"] = None
) -> Optional[Path]:
    """Find arts.cfg for the compilation.

    Returns None if the user already supplied --arts-config.  Otherwise
    checks for arts.cfg next to the input file, then falls back to the
    default examples config.  When no config is found carts-compile
    will fail immediately — that is intentional.
    """
    for arg in pipeline_args:
        if arg == FLAG_ARTS_CONFIG or arg.startswith(f"{FLAG_ARTS_CONFIG}="):
            return None

    # Check next to the input file
    candidate = input_file.parent.resolve() / ARTS_CONFIG_FILENAME
    if candidate.is_file():
        return candidate

    # Fall back to the default examples config
    if config:
        default = config.carts_dir / "tests" / "examples" / ARTS_CONFIG_FILENAME
        if default.is_file():
            return default

    return None


def _find_metadata_file(input_file: Path, pipeline_args: List[str]) -> Optional[Path]:
    """Find sibling metadata JSON for .mlir compilation when not overridden."""
    for arg in pipeline_args:
        if arg == FLAG_METADATA_FILE or arg.startswith(f"{FLAG_METADATA_FILE}="):
            return None

    candidate = input_file.parent.resolve() / DEFAULT_METADATA_FILENAME
    if candidate.is_file():
        return candidate
    return None


def _collect_flag_args(args: List[str], allowed_flags: List[str]) -> List[str]:
    """Collect selected long flags in either `--flag value` or `--flag=value` form."""
    collected: List[str] = []
    i = 0
    while i < len(args):
        arg = args[i]
        matched = next((flag for flag in allowed_flags if arg == flag), None)
        if matched is not None:
            collected.append(arg)
            if i + 1 < len(args):
                next_arg = args[i + 1]
                if not next_arg.startswith("--") or _is_numeric(next_arg):
                    collected.append(next_arg)
                    i += 1
            i += 1
            continue
        if any(arg.startswith(f"{flag}=") for flag in allowed_flags):
            collected.append(arg)
        i += 1
    return collected


def _metadata_json_path_from_args(args: List[str]) -> Path:
    """Return metadata JSON path from passthrough args or default."""
    i = 0
    while i < len(args):
        arg = args[i]
        if arg == FLAG_METADATA_FILE and i + 1 < len(args):
            return Path(args[i + 1])
        if arg.startswith(f"{FLAG_METADATA_FILE}="):
            return Path(arg.split("=", 1)[1])
        i += 1
    return Path(DEFAULT_METADATA_FILENAME)


def _build_cgeist_cmd(
    config: CartsConfig,
    input_file: Path,
    std_flag: str,
    passthrough_args: List[str],
    with_openmp: bool = False,
    with_debug_info: bool = False,
) -> List[str]:
    """Build cgeist (C-to-MLIR) command with standard flags."""
    cmd = [str(config.get_polygeist_tool(TOOL_CGEIST))]
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
    config: CartsConfig,
    input_file: Path,
    output_file: Path,
    debug: bool,
    extra_args: List[str],
) -> List[str]:
    """Build clang link command for linking with ARTS runtime."""
    cmd = [str(config.get_llvm_tool(TOOL_CLANG))]
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


def _compile_c_pipeline(
    config: CartsConfig,
    input_file: Path,
    output_name: Path,
    debug: bool,
    enable_pipeline_o3: bool,
    pipeline_args: List[str],
    link_args: List[str],
    passthrough_args: Optional[List[str]] = None,
    diagnose: bool = False,
    diagnose_output: Optional[Path] = None,
    pipeline_stop: Optional[str] = None,
) -> None:
    """C/C++ compilation pipeline with metadata extraction.

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
        f"Mode:   [{Colors.WARNING}]Compilation pipeline[/{Colors.WARNING}]")
    console.print()

    carts_compile_bin = config.get_carts_tool(TOOL_CARTS_COMPILE)

    metadata_args = _collect_flag_args(
        pipeline_args, [FLAG_ARTS_CONFIG, FLAG_METADATA_FILE]
    )

    # Output file paths
    seq_mlir = Path(f"{base_name}_seq.mlir")
    metadata_mlir = Path(f"{base_name}_arts_metadata.mlir")
    par_mlir = Path(f"{base_name}.mlir")
    ll_file = Path(f"{base_name}-arts.ll")
    metadata_json = _metadata_json_path_from_args(metadata_args)

    # Step 2 rewrites metadata from the current sequential MLIR. A stale file
    # from an unrelated prior compile can poison JSON import and crash later
    # pipeline stages, so start from a clean metadata cache path.
    try:
        metadata_json.unlink(missing_ok=True)
    except OSError as exc:
        print_warning(f"Could not remove stale metadata file {metadata_json}: {exc}")

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
            raise Exit(1)
        progress.remove_task(task)
        print_success(f"[1{step_label} {seq_mlir}")

        # Step 2: Extract metadata from sequential MLIR
        task = progress.add_task(f"[2{step_label} Collecting metadata...", total=None)
        cmd = [str(carts_compile_bin), str(seq_mlir),
               "--collect-metadata", FLAG_OUTPUT, str(metadata_mlir)]
        arts_cfg = _find_arts_config(input_file, pipeline_args, config)
        if arts_cfg and not _has_flag(metadata_args, FLAG_ARTS_CONFIG):
            cmd.extend([FLAG_ARTS_CONFIG, str(arts_cfg)])
        cmd.extend(metadata_args)
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed to collect metadata")
            raise Exit(1)
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
            raise Exit(1)
        progress.remove_task(task)
        print_success(f"[3{step_label} {par_mlir}")

        # Step 4: Apply ARTS transformations
        task = progress.add_task(f"[4{step_label} ARTS transformations...", total=None)
        cmd = [str(carts_compile_bin), str(par_mlir)]
        if enable_pipeline_o3:
            cmd.append("--O3")
        arts_cfg = _find_arts_config(input_file, pipeline_args, config)
        if arts_cfg:
            cmd.extend([FLAG_ARTS_CONFIG, str(arts_cfg)])
        if not skip_link:
            cmd.append("--emit-llvm")
        if debug:
            cmd.append("-g")
        if diagnose:
            cmd.append(FLAG_DIAGNOSE)
            effective_diagnose_output = diagnose_output or Path(
                f"{base_name}-diagnose.json")
            cmd.extend([FLAG_DIAGNOSE_OUTPUT, str(effective_diagnose_output)])
        if pipeline_stop:
            cmd.append(f"{FLAG_PIPELINE}={pipeline_stop}")
        cmd.extend(pipeline_args)

        out_file = par_mlir.with_suffix(f".{pipeline_stop}.mlir") if skip_link else ll_file
        if run_command_with_output(cmd, out_file) != 0:
            print_error("Failed ARTS transformations")
            raise Exit(1)
        progress.remove_task(task)
        print_success(f"[4{step_label} {out_file}")

        if skip_link:
            console.print()
            console.print(Panel(
                f"[{Colors.SUCCESS}]Pipeline output:[/{Colors.SUCCESS}] {out_file}\n"
                f"[{Colors.DIM}]Stopped at: {pipeline_stop}[/{Colors.DIM}]",
                title="Success",
                border_style=Colors.SUCCESS,
            ))
            return

        # Step 5: Link with ARTS runtime
        task = progress.add_task(f"[5{step_label} Final linking...", total=None)
        cmd = _build_link_cmd(
            config, ll_file, output_name, debug, link_args)
        if run_subprocess(cmd, check=False).returncode != 0:
            print_error("Failed final linking")
            raise Exit(1)
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
    console.print(Panel(files_info, title="Success", border_style=Colors.SUCCESS))


def _compile_all_pipelines(
    config: CartsConfig,
    source_file: Path,
    output_dir: Optional[Path],
    extra_args: List[str],
    optimize: bool,
) -> None:
    """Run pipeline and dump all intermediate pipeline steps."""
    if not source_file.is_file():
        print_error(f"Input file '{source_file}' not found")
        raise Exit(1)

    carts_compile_bin = config.get_carts_tool(TOOL_CARTS_COMPILE)

    # Determine output directory
    out_dir = output_dir if output_dir else source_file.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    manifest = _get_pipeline_manifest(config)
    pipeline_steps = manifest.tokens.pipeline_sequence

    stem = source_file.stem
    total_steps = 2 + len(pipeline_steps)

    print_header("CARTS Pipeline Dump")

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TaskProgressColumn(),
        console=console,
    ) as progress:
        task = progress.add_task("Processing pipeline steps...", total=total_steps)

        base_cmd = [str(carts_compile_bin), str(source_file)]
        if optimize:
            base_cmd.append("--O3")
        arts_cfg = _find_arts_config(source_file, extra_args, config)
        if arts_cfg:
            base_cmd.extend([FLAG_ARTS_CONFIG, str(arts_cfg)])

        results = []  # List of (pipeline_step, success_bool)

        # Dump each pipeline step
        for pipeline_step in pipeline_steps:
            progress.update(task, description=f"Pipeline: {pipeline_step}")
            out_file = out_dir / f"{stem}_{pipeline_step}.mlir"
            cmd = base_cmd + [f"{FLAG_PIPELINE}={pipeline_step}",
                              FLAG_OUTPUT, str(out_file)] + extra_args
            rc = run_subprocess(cmd, check=False).returncode
            results.append((pipeline_step, rc == 0))
            progress.advance(task)

        # Final MLIR
        progress.update(task, description="Exporting complete MLIR")
        final_mlir = out_dir / f"{stem}_complete.mlir"
        cmd = base_cmd + [FLAG_OUTPUT, str(final_mlir)] + extra_args
        rc = run_subprocess(cmd, check=False).returncode
        results.append(("complete-mlir", rc == 0))
        progress.advance(task)

        # Final LLVM IR
        progress.update(task, description="Exporting LLVM IR")
        final_ll = out_dir / f"{stem}.ll"
        cmd = base_cmd + ["--emit-llvm", FLAG_OUTPUT, str(final_ll)] + extra_args
        rc = run_subprocess(cmd, check=False).returncode
        results.append(("emit-llvm", rc == 0))
        progress.advance(task)

    # Print summary table
    passed = sum(1 for _, ok in results if ok)
    failed = sum(1 for _, ok in results if not ok)

    table = Table(title="Pipeline Results")
    table.add_column("Pipeline", style=Colors.INFO)
    table.add_column("Status", justify="center")
    for pipeline_name, ok in results:
        status = f"[{Colors.SUCCESS}]PASS[/{Colors.SUCCESS}]" if ok else f"[{Colors.ERROR}]FAIL[/{Colors.ERROR}]"
        table.add_row(pipeline_name, status)
    console.print(table)

    console.print(
        f"\n[{Colors.SUCCESS}]{passed} passed[/{Colors.SUCCESS}], [{Colors.ERROR}]{failed} failed[/{Colors.ERROR}] "
        f"out of {len(results)} pipeline outputs"
    )
    print_success(f"Pipeline dumps written to {out_dir}")
