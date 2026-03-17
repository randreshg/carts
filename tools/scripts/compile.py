"""Compile pipeline commands for CARTS CLI."""

from dataclasses import dataclass, field
from contextlib import contextmanager
import json
import os
from pathlib import Path
from typing import Dict, List, Optional

import typer
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TaskProgressColumn
from rich.table import Table

from carts_styles import (
    Colors,
    console,
    print_header,
    print_error,
    print_info,
    print_success,
    print_warning,
)
from scripts.config import PlatformConfig, get_config
from scripts.run import run_subprocess, run_command_with_output

FLAG_ARTS_CONFIG = "--arts-config"
FLAG_OPTIMIZE_O3 = "--O3"
FLAG_EMIT_LLVM = "--emit-llvm"
FLAG_COLLECT_METADATA = "--collect-metadata"
FLAG_DIAGNOSE = "--diagnose"
FLAG_DIAGNOSE_OUTPUT = "--diagnose-output"
FLAG_START_FROM = "--start-from"
FLAG_PARTITION_FALLBACK = "--partition-fallback"
FLAG_PIPELINE = "--pipeline"
FLAG_ALL_PIPELINES = "--all-pipelines"
FLAG_HELP = "--help"
FLAG_PRINT_PIPELINE_TOKENS_JSON = "--print-pipeline-tokens-json"
FLAG_PRINT_PIPELINE_MANIFEST_JSON = "--print-pipeline-manifest-json"
FLAG_DEBUG_ONLY = "--debug-only"
FLAG_ARTS_ONLY = "--arts-only"
CLI_OPTIMIZE_O3 = "-O3"
FLAG_OUTPUT = "-o"
DEFAULT_METADATA_FILENAME = ".carts-metadata.json"
PIPELINE_MANIFEST_FILENAME = "pipeline-manifest.json"
CGEIST_FLAG_RAISE_AFFINE = "--raise-scf-to-affine"
CGEIST_FLAG_PRINT_DEBUG_INFO = "--print-debug-info"


@dataclass(frozen=True)
class PipelineTokens:
    pipeline: List[str]
    start_from: List[str]
    pipeline_sequence: List[str]
    aliases: Dict[str, str]


@dataclass(frozen=True)
class PipelineManifestStep:
    name: str
    passes: List[str]


@dataclass(frozen=True)
class PipelineManifest:
    tokens: PipelineTokens
    steps: List[PipelineManifestStep]


_PIPELINE_TOKENS_CACHE: Optional[PipelineTokens] = None
_PIPELINE_MANIFEST_CACHE: Optional[PipelineManifest] = None


def _dedupe_tokens(tokens: List[str]) -> List[str]:
    """Keep token order stable while removing duplicates."""
    seen = set()
    deduped: List[str] = []
    for token in tokens:
        if token not in seen:
            seen.add(token)
            deduped.append(token)
    return deduped


def _has_debug_only_flag(args: List[str]) -> bool:
    """Return True when passthrough args include legacy --debug-only."""
    i = 0
    while i < len(args):
        arg = args[i]
        if arg == FLAG_DEBUG_ONLY or arg.startswith(f"{FLAG_DEBUG_ONLY}="):
            return True
        i += 1
    return False


def _normalize_arts_only(spec: str) -> str:
    """Normalize `--arts-only` channels and reject empty specs."""
    channels = [channel.strip() for channel in spec.split(",") if channel.strip()]
    if not channels:
        raise ValueError("Invalid --arts-only value.")
    return ",".join(channels)


def _parse_pipeline_tokens_payload(payload: str) -> PipelineTokens:
    """Parse `carts-compile --print-pipeline-tokens-json` output."""
    data = json.loads(payload)
    if not isinstance(data, dict):
        raise ValueError("root JSON object must be a map")

    def require_string_list(key: str) -> List[str]:
        value = data.get(key)
        if not isinstance(value, list) or not all(isinstance(x, str) for x in value):
            raise ValueError(f"field '{key}' must be a list of strings")
        return _dedupe_tokens(list(value))

    pipeline = require_string_list("pipeline")
    start_from = require_string_list("start_from")
    pipeline_sequence = require_string_list("pipeline_sequence")

    aliases_raw = data.get("aliases", {})
    if not isinstance(aliases_raw, dict):
        raise ValueError("field 'aliases' must be an object")
    aliases: Dict[str, str] = {}
    for alias, target in aliases_raw.items():
        if not isinstance(alias, str) or not isinstance(target, str):
            raise ValueError("alias keys and values must be strings")
        aliases[alias] = target

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
        aliases=aliases,
    )


def _parse_pipeline_manifest_payload(payload: str) -> PipelineManifest:
    """Parse `carts-compile --print-pipeline-manifest-json` output."""
    tokens = _parse_pipeline_tokens_payload(payload)
    data = json.loads(payload)
    steps_raw = data.get("pipeline_steps")
    if not isinstance(steps_raw, list):
        raise ValueError("field 'pipeline_steps' must be a list")

    steps: List[PipelineManifestStep] = []
    for step_raw in steps_raw:
        if not isinstance(step_raw, dict):
            raise ValueError("each pipeline step entry must be an object")
        name = step_raw.get("name")
        passes = step_raw.get("passes", [])
        if not isinstance(name, str):
            raise ValueError("pipeline step field 'name' must be a string")
        if not isinstance(passes, list) or not all(isinstance(p, str) for p in passes):
            raise ValueError("pipeline step field 'passes' must be a list of strings")
        steps.append(PipelineManifestStep(name=name, passes=list(passes)))

    return PipelineManifest(tokens=tokens, steps=steps)


def _pipeline_manifest_to_dict(manifest: PipelineManifest) -> Dict[str, object]:
    """Convert pipeline manifest dataclass into a JSON-serializable dict."""
    return {
        "pipeline": manifest.tokens.pipeline,
        "start_from": manifest.tokens.start_from,
        "pipeline_sequence": manifest.tokens.pipeline_sequence,
        "aliases": manifest.tokens.aliases,
        "pipeline_steps": [
            {"name": step.name, "passes": step.passes}
            for step in manifest.steps
        ],
    }


def _query_compiler_json(
    config: PlatformConfig, flag: str, error_message: str
) -> str:
    """Run carts-compile JSON endpoint and return stdout payload."""
    carts_compile_bin = config.get_carts_tool("carts-compile")
    if not carts_compile_bin.is_file():
        print_error(f"carts-compile not found at {carts_compile_bin}")
        raise typer.Exit(1)

    result = run_subprocess(
        [str(carts_compile_bin), flag],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        print_error(error_message)
        raise typer.Exit(result.returncode)
    return result.stdout or ""


def _pipeline_manifest_path(config: PlatformConfig) -> Path:
    """Return installed pipeline manifest path."""
    return config.carts_install_dir / "share" / PIPELINE_MANIFEST_FILENAME


def _load_pipeline_manifest_from_file(config: PlatformConfig) -> Optional[PipelineManifest]:
    """Load installed pipeline manifest when available."""
    manifest_path = _pipeline_manifest_path(config)
    if not manifest_path.is_file():
        return None
    try:
        payload = manifest_path.read_text(encoding="utf-8")
        return _parse_pipeline_manifest_payload(payload)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print_warning(f"Ignoring invalid pipeline manifest at {manifest_path}: {exc}")
        return None


def _load_pipeline_tokens_from_manifest(config: PlatformConfig) -> Optional[PipelineTokens]:
    """Load pipeline tokens from installed manifest file when available."""
    manifest = _load_pipeline_manifest_from_file(config)
    if manifest is None:
        return None
    return manifest.tokens


def _get_pipeline_tokens(config: PlatformConfig) -> PipelineTokens:
    """Load pipeline tokens from carts-compile once per process."""
    global _PIPELINE_TOKENS_CACHE
    if _PIPELINE_TOKENS_CACHE is not None:
        return _PIPELINE_TOKENS_CACHE

    from_manifest = _load_pipeline_tokens_from_manifest(config)
    if from_manifest is not None:
        _PIPELINE_TOKENS_CACHE = from_manifest
        return _PIPELINE_TOKENS_CACHE

    try:
        payload = _query_compiler_json(
            config,
            FLAG_PRINT_PIPELINE_TOKENS_JSON,
            "Failed to query pipeline tokens from carts-compile. "
            "Run `tools/carts build` and retry.",
        )
        _PIPELINE_TOKENS_CACHE = _parse_pipeline_tokens_payload(payload)
    except (json.JSONDecodeError, ValueError) as exc:
        print_error(f"Invalid pipeline token payload from carts-compile: {exc}")
        raise typer.Exit(1)

    return _PIPELINE_TOKENS_CACHE


def _get_pipeline_manifest(config: PlatformConfig) -> PipelineManifest:
    """Load pipeline manifest from install cache or compiler JSON endpoint."""
    global _PIPELINE_MANIFEST_CACHE
    if _PIPELINE_MANIFEST_CACHE is not None:
        return _PIPELINE_MANIFEST_CACHE

    from_file = _load_pipeline_manifest_from_file(config)
    if from_file is not None:
        _PIPELINE_MANIFEST_CACHE = from_file
        return _PIPELINE_MANIFEST_CACHE

    try:
        payload = _query_compiler_json(
            config,
            FLAG_PRINT_PIPELINE_MANIFEST_JSON,
            "Failed to query pipeline manifest from carts-compile. "
            "Run `tools/carts build` and retry.",
        )
        _PIPELINE_MANIFEST_CACHE = _parse_pipeline_manifest_payload(payload)
    except (json.JSONDecodeError, ValueError) as exc:
        print_error(f"Invalid pipeline manifest payload from carts-compile: {exc}")
        raise typer.Exit(1)

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
            if arg in (CLI_OPTIMIZE_O3, FLAG_OPTIMIZE_O3):
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
    cmd.append(CGEIST_FLAG_RAISE_AFFINE)

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
# Pipeline Command
# ============================================================================

def pipeline(
    pipeline: Optional[str] = typer.Option(
        None, FLAG_PIPELINE, help="Show passes for one pipeline step"),
    json_output: bool = typer.Option(
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
        raise typer.Exit(1)

    if json_output:
        console.print(json.dumps(_pipeline_manifest_to_dict(manifest), indent=2))
        return

    print_header("CARTS Pipeline")
    table = Table(title="Pipeline Order")
    table.add_column("#", style="cyan", justify="right")
    table.add_column("Pipeline", style="green")
    table.add_column("Passes", style="magenta", justify="right")
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
    ctx: typer.Context,
    input_file: Path = typer.Argument(..., help="Input file (.c/.cpp/.mlir/.ll)"),
    output: Optional[Path] = typer.Option(
        None, "-o", help="Output file or directory"),
    optimize: bool = typer.Option(
        False, CLI_OPTIMIZE_O3, help="Enable O3 optimizations"),
    debug: bool = typer.Option(False, "-g", help="Generate debug symbols"),
    pipeline: Optional[str] = typer.Option(
        None, FLAG_PIPELINE,
        help="Stop after pipeline step (e.g., concurrency, edt-distribution)"),
    all_pipelines: bool = typer.Option(
        False, FLAG_ALL_PIPELINES, help="Dump all pipeline steps"),
    emit_llvm: bool = typer.Option(
        False, FLAG_EMIT_LLVM, help="Emit LLVM IR output"),
    collect_metadata: bool = typer.Option(
        False, FLAG_COLLECT_METADATA, help="Collect and export metadata"),
    partition_fallback: Optional[str] = typer.Option(
        None, FLAG_PARTITION_FALLBACK,
        help="Partition fallback strategy (coarse, fine)"),
    diagnose: bool = typer.Option(
        False, FLAG_DIAGNOSE, help="Export diagnostic information"),
    diagnose_output: Optional[Path] = typer.Option(
        None, FLAG_DIAGNOSE_OUTPUT, help="Output file for diagnostics"),
    arts_only: Optional[str] = typer.Option(
        None, FLAG_ARTS_ONLY,
        help="Enable ARTS_INFO/ARTS_DEBUG channels in carts-compile"),
    start_from: Optional[str] = typer.Option(
        None, FLAG_START_FROM,
        help="Resume pipeline from specified step (requires saved MLIR)"),
):
    """Unified compilation command.

    Routes based on input file type:

      .c/.cpp  Full pipeline: C -> MLIR -> LLVM IR -> executable

      .mlir    MLIR transformation pipeline via carts-compile binary

      .ll      Link LLVM IR with ARTS runtime -> executable

    Use --pipeline <step> to stop after a specific pipeline step.
    """
    config = get_config()
    pipeline_tokens = _get_pipeline_tokens(config)

    if pipeline and pipeline not in pipeline_tokens.pipeline:
        print_error(f"Unknown pipeline step: '{pipeline}'")
        print_info("Available pipeline steps:")
        for pipeline_token in pipeline_tokens.pipeline:
            console.print(f"  - {pipeline_token}")
        raise typer.Exit(1)
    if start_from and start_from not in pipeline_tokens.start_from:
        print_error(f"Unknown start-from pipeline step: '{start_from}'")
        print_info("Available start-from pipeline steps:")
        for pipeline_token in pipeline_tokens.start_from:
            console.print(f"  - {pipeline_token}")
        raise typer.Exit(1)
    if _has_debug_only_flag(ctx.args or []):
        print_error("Use --arts-only instead of legacy --debug-only.")
        raise typer.Exit(1)
    normalized_arts_only: Optional[str] = None
    if arts_only:
        try:
            normalized_arts_only = _normalize_arts_only(arts_only)
        except ValueError as exc:
            print_error(str(exc))
            raise typer.Exit(1)

    ext = input_file.suffix.lower()
    if ext in (".c", ".cpp"):
        _compile_from_c(ctx, input_file, output, optimize, debug, pipeline,
                        all_pipelines, emit_llvm, collect_metadata,
                        partition_fallback, diagnose, diagnose_output,
                        normalized_arts_only,
                        start_from)
    elif ext == ".mlir":
        _compile_from_mlir(ctx, input_file, output, optimize, debug, pipeline,
                           all_pipelines, emit_llvm, collect_metadata,
                           partition_fallback, diagnose, diagnose_output,
                           normalized_arts_only,
                           start_from)
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
    all_pipelines: bool,
    emit_llvm: bool,
    collect_metadata: bool,
    partition_fallback: Optional[str],
    diagnose: bool,
    diagnose_output: Optional[Path],
    arts_only: Optional[str],
    start_from: Optional[str] = None,
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
        extra_pipeline_args.append(f"{FLAG_PARTITION_FALLBACK}={partition_fallback}")
    if start_from:
        extra_pipeline_args.append(f"{FLAG_START_FROM}={start_from}")
    if arts_only:
        extra_pipeline_args.append(f"{FLAG_ARTS_ONLY}={arts_only}")

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
    ctx: typer.Context,
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
    arts_only: Optional[str] = None,
    start_from: Optional[str] = None,
) -> None:
    """MLIR transformation pipeline."""
    config = get_config()
    carts_compile_bin = config.get_carts_tool("carts-compile")

    # Pass --help through to carts-compile binary
    if ctx.args and (FLAG_HELP in ctx.args or "-h" in ctx.args):
        result = run_subprocess([str(carts_compile_bin), FLAG_HELP], check=False)
        raise typer.Exit(result.returncode)

    if all_pipelines:
        extra_args = list(ctx.args or [])
        if arts_only:
            extra_args.append(f"{FLAG_ARTS_ONLY}={arts_only}")
        _compile_all_pipelines(config, input_file, output, extra_args)
        return

    if not input_file.is_file():
        print_error(f"Input file '{input_file}' not found")
        raise typer.Exit(1)

    cmd = [str(carts_compile_bin), str(input_file)]

    # Auto-discover arts.cfg if not explicitly provided
    arts_cfg = _find_arts_config(input_file, ctx.args or [], config)
    if arts_cfg:
        cmd.extend([FLAG_ARTS_CONFIG, str(arts_cfg)])

    if optimize:
        cmd.append(FLAG_OPTIMIZE_O3)
    if emit_llvm:
        cmd.append(FLAG_EMIT_LLVM)
    if collect_metadata:
        cmd.append(FLAG_COLLECT_METADATA)
    if debug:
        cmd.append("-g")
    if pipeline:
        cmd.append(f"{FLAG_PIPELINE}={pipeline}")
    if partition_fallback:
        cmd.append(f"{FLAG_PARTITION_FALLBACK}={partition_fallback}")
    if diagnose:
        cmd.append(FLAG_DIAGNOSE)
    if diagnose_output:
        cmd.extend([FLAG_DIAGNOSE_OUTPUT, str(diagnose_output)])
    if arts_only:
        cmd.append(f"{FLAG_ARTS_ONLY}={arts_only}")
    if start_from:
        cmd.append(f"{FLAG_START_FROM}={start_from}")
    if output:
        cmd.extend([FLAG_OUTPUT, str(output)])
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

def _find_arts_config(
    input_file: Path, pipeline_args: List[str], config: Optional["PlatformConfig"] = None
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
    candidate = input_file.parent.resolve() / "arts.cfg"
    if candidate.is_file():
        return candidate

    # Fall back to the default examples config
    if config:
        default = config.carts_dir / "tests" / "examples" / "arts.cfg"
        if default.is_file():
            return default

    return None


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
    cmd.extend([CGEIST_FLAG_RAISE_AFFINE, std_flag, "-O0", "-S",
                 "-D_POSIX_C_SOURCE=199309L"])  # Required for clock_gettime
    if with_openmp:
        cmd.append("-fopenmp")
    if with_debug_info:
        cmd.append(CGEIST_FLAG_PRINT_DEBUG_INFO)
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


def _compile_c_pipeline(
    config: PlatformConfig,
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

    carts_compile_bin = config.get_carts_tool("carts-compile")

    metadata_args: List[str] = []
    i = 0
    while i < len(pipeline_args):
        arg = pipeline_args[i]
        if arg == FLAG_ARTS_CONFIG:
            metadata_args.append(arg)
            if i + 1 < len(pipeline_args):
                metadata_args.append(pipeline_args[i + 1])
                i += 1
        elif arg.startswith(f"{FLAG_ARTS_CONFIG}="):
            metadata_args.append(arg)
        i += 1

    # Output file paths
    seq_mlir = Path(f"{base_name}_seq.mlir")
    metadata_mlir = Path(f"{base_name}_arts_metadata.mlir")
    par_mlir = Path(f"{base_name}.mlir")
    ll_file = Path(f"{base_name}-arts.ll")
    metadata_json = Path(DEFAULT_METADATA_FILENAME)

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
               FLAG_COLLECT_METADATA, FLAG_OUTPUT, str(metadata_mlir)]
        arts_cfg = _find_arts_config(input_file, pipeline_args, config)
        if arts_cfg and not metadata_args:
            cmd.extend([FLAG_ARTS_CONFIG, str(arts_cfg)])
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
        cmd = [str(carts_compile_bin), str(par_mlir)]
        if enable_pipeline_o3:
            cmd.append(FLAG_OPTIMIZE_O3)
        arts_cfg = _find_arts_config(input_file, pipeline_args, config)
        if arts_cfg:
            cmd.extend([FLAG_ARTS_CONFIG, str(arts_cfg)])
        if not skip_link:
            cmd.append(FLAG_EMIT_LLVM)
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


def _compile_all_pipelines(
    config: PlatformConfig,
    source_file: Path,
    output_dir: Optional[Path],
    extra_args: List[str],
) -> None:
    """Run pipeline and dump all intermediate pipeline steps."""
    if not source_file.is_file():
        print_error(f"Input file '{source_file}' not found")
        raise typer.Exit(1)

    carts_compile_bin = config.get_carts_tool("carts-compile")

    # Determine output directory
    out_dir = output_dir if output_dir else source_file.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    pipeline_tokens = _get_pipeline_tokens(config)
    pipeline_steps = pipeline_tokens.pipeline_sequence

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

        # Find arts-config if present
        config_file = source_file.parent / "arts.cfg"
        base_cmd = [str(carts_compile_bin), str(source_file), FLAG_OPTIMIZE_O3]
        if config_file.is_file():
            base_cmd.extend([FLAG_ARTS_CONFIG, str(config_file)])

        results = []  # List of (stage_name, success_bool)

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
        cmd = base_cmd + [FLAG_EMIT_LLVM, FLAG_OUTPUT, str(final_ll)] + extra_args
        rc = run_subprocess(cmd, check=False).returncode
        results.append(("emit-llvm", rc == 0))
        progress.advance(task)

    # Print summary table
    passed = sum(1 for _, ok in results if ok)
    failed = sum(1 for _, ok in results if not ok)

    table = Table(title="Pipeline Results")
    table.add_column("Pipeline", style="cyan")
    table.add_column("Status", justify="center")
    for pipeline_name, ok in results:
        status = f"[green]PASS[/green]" if ok else f"[red]FAIL[/red]"
        table.add_row(pipeline_name, status)
    console.print(table)

    console.print(
        f"\n[green]{passed} passed[/green], [red]{failed} failed[/red] "
        f"out of {len(results)} pipeline outputs"
    )
    print_success(f"Pipeline dumps written to {out_dir}")
