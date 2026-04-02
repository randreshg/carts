"""Benchmark triage helpers for the CARTS CLI."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
import json
from pathlib import Path
import re
import shlex
import sys
from typing import Any, Dict, List, Optional, Sequence

from dekk import Argument, Colors, Exit, Option, console, print_error, print_info, print_success, print_warning

from scripts import SUBMODULE_BENCHMARKS, carts_cli_command, run_subprocess
from scripts.platform import get_config

@dataclass(frozen=True)
class ArtsBuildStamp:
    cflags: List[str]
    compile_args: List[str]


def _results_root() -> Path:
    config = get_config()
    return config.carts_dir / SUBMODULE_BENCHMARKS / "results"


def _benchmark_root() -> Path:
    config = get_config()
    return config.carts_dir / SUBMODULE_BENCHMARKS


def _resolve_benchmark_dir(benchmark: str) -> Path:
    root = _benchmark_root()
    candidate = (root / benchmark).resolve()
    if candidate.is_dir():
        return candidate
    raise Exit(f"Benchmark '{benchmark}' not found under {root}")


def _find_latest_results_json() -> Path:
    root = _results_root()
    candidates = sorted(
        root.glob("*/results.json"),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        raise Exit(f"No benchmark results found under {root}")
    return candidates[0]


def _load_results_entry(results_json: Path, benchmark: str, size: str,
                        threads: int) -> Dict[str, Any]:
    payload = json.loads(results_json.read_text(encoding="utf-8"))
    results = payload.get("results", [])
    matches = [
        entry for entry in results
        if entry.get("name") == benchmark
        and entry.get("size") == size
        and entry.get("config", {}).get("arts_threads") == threads
    ]
    if not matches:
        raise Exit(
            f"No result entry for benchmark='{benchmark}', size='{size}', "
            f"threads={threads} in {results_json}"
        )
    matches.sort(key=lambda entry: entry.get("timestamp", ""), reverse=True)
    return matches[0]


def _parse_makefile_var(makefile: Path, var_name: str) -> Optional[str]:
    pattern = re.compile(rf"^\s*{re.escape(var_name)}\s*[:?]?=\s*(.*?)\s*$")
    for line in makefile.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            value = match.group(1)
            if " #" in value:
                value = value.split(" #", 1)[0]
            return value.strip()
    return None


def _resolve_source_file(benchmark_dir: Path) -> Path:
    makefile = benchmark_dir / "Makefile"
    source_name = _parse_makefile_var(makefile, "SRC") if makefile.is_file() else None
    if source_name:
        source_path = benchmark_dir / source_name
        if source_path.is_file():
            return source_path

    candidates = sorted(
        path for path in benchmark_dir.iterdir()
        if path.suffix.lower() in {".c", ".cpp"}
    )
    if len(candidates) == 1:
        return candidates[0]
    raise Exit(f"Could not resolve benchmark source in {benchmark_dir}")


def _parse_arts_build_stamp(stamp_path: Path) -> ArtsBuildStamp:
    text = stamp_path.read_text(encoding="utf-8").strip()
    match = re.match(r"^CFLAGS=(.*?)\|COMPILE_ARGS=(.*?)\|CFG=", text)
    if not match:
        raise Exit(f"Unrecognized ARTS build stamp format: {stamp_path}")
    cflags = shlex.split(match.group(1).strip())
    compile_args = shlex.split(match.group(2).strip())
    return ArtsBuildStamp(cflags=cflags, compile_args=compile_args)


def _write_pipeline_manifest(output_dir: Path) -> Path:
    manifest_path = output_dir / "pipeline-manifest.json"
    cmd = carts_cli_command("pipeline", "--json")
    result = run_subprocess(cmd, capture_output=True, check=False)
    if result.returncode != 0:
        raise Exit("Failed to query pipeline manifest")
    manifest_path.write_text(result.stdout or "", encoding="utf-8")
    return manifest_path


def _load_pipeline_sequence(manifest_path: Path) -> List[str]:
    payload = json.loads(manifest_path.read_text(encoding="utf-8"))
    sequence = payload.get("pipeline_sequence", [])
    if not isinstance(sequence, list) or not all(isinstance(item, str) for item in sequence):
        raise Exit(f"Invalid pipeline manifest payload: {manifest_path}")
    return sequence


def _stage_output_path(output_dir: Path, source_file: Path, stage: str) -> Path:
    return output_dir / f"{source_file.stem}.{stage}.mlir"


def _run_triage_build(benchmark: str, size: str, threads: int, timeout: int) -> None:
    cmd = [
        *carts_cli_command("benchmarks", "run"),
        benchmark,
        "--size",
        size,
        "--threads",
        str(threads),
        "--timeout",
        str(timeout),
    ]
    result = run_subprocess(cmd, check=False, realtime=True)
    if result.returncode != 0:
        print_warning(
            "Benchmark run exited non-zero. Continuing so existing artifacts can be triaged."
        )


def _dump_stage(parallel_mlir: Path, source_file: Path, arts_config: Path,
                compile_args: Sequence[str], stage: str, output_dir: Path) -> Path:
    output_path = _stage_output_path(output_dir, source_file, stage)
    cmd = [
        *carts_cli_command("compile"),
        str(parallel_mlir),
        f"--pipeline={stage}",
        "-o",
        str(output_path),
        "--arts-config",
        str(arts_config),
    ]
    cmd.extend(compile_args)

    result = run_subprocess(
        cmd,
        check=False,
        realtime=True,
    )
    if result.returncode != 0:
        raise Exit(f"Failed to dump stage '{stage}' for {parallel_mlir}")
    return output_path


def _dump_all_pipelines(parallel_mlir: Path, arts_config: Path,
                        compile_args: Sequence[str], output_dir: Path) -> None:
    cmd = [
        *carts_cli_command("compile"),
        str(parallel_mlir),
        "--all-pipelines",
        "-o",
        str(output_dir),
        "--arts-config",
        str(arts_config),
    ]
    cmd.extend(compile_args)
    result = run_subprocess(cmd, check=False, realtime=True)
    if result.returncode != 0:
        raise Exit(f"Failed to dump all pipelines for {parallel_mlir}")


def triage_benchmark(
    benchmark: str = Argument(
        ...,
        help="Benchmark path relative to external/carts-benchmarks (example: monte-carlo/ensemble)",
    ),
    size: str = Option("small", "--size", help="Benchmark size"),
    threads: int = Option(2, "--threads", help="Thread count to run/filter"),
    timeout: int = Option(120, "--timeout", help="Benchmark timeout in seconds"),
    stages: Optional[str] = Option(
        None,
        "--stages",
        help="Comma-separated pipeline stages to dump. Defaults to compiler-driven --all-pipelines output.",
    ),
    output_dir: Optional[Path] = Option(
        None,
        "--output-dir",
        help="Directory for pipeline dumps and manifest (defaults under /tmp)",
    ),
    rerun: bool = Option(
        True,
        "--rerun/--no-rerun",
        help="Run the benchmark before collecting artifacts",
    ),
):
    """Run a benchmark triage workflow using CARTS commands and runner artifacts."""
    benchmark_dir = _resolve_benchmark_dir(benchmark)
    source_file = _resolve_source_file(benchmark_dir)

    if rerun:
        print_info(
            f"Running benchmark triage target '{benchmark}' with size={size}, threads={threads}"
        )
        _run_triage_build(benchmark, size, threads, timeout)

    results_json = _find_latest_results_json()
    result_entry = _load_results_entry(results_json, benchmark, size, threads)
    artifacts = result_entry.get("artifacts", {})
    build_dir = Path(artifacts["build_dir"])
    arts_config = Path(artifacts["arts_config"])
    arts_log = Path(artifacts["arts_log"])
    omp_log = Path(artifacts["omp_log"])

    stamp = _parse_arts_build_stamp(build_dir / ".arts_cflags")
    parallel_mlir = build_dir / f"{source_file.stem}.mlir"
    if not parallel_mlir.is_file():
        raise Exit(f"Parallel MLIR artifact not found: {parallel_mlir}")

    if output_dir is None:
        stamp_name = benchmark.replace("/", "__")
        timestamp = datetime.utcnow().strftime("%Y%m%d_%H%M%S")
        output_dir = Path("/tmp") / "carts-triage" / f"{stamp_name}_{size}_{threads}t_{timestamp}"
    output_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = _write_pipeline_manifest(output_dir)
    pipeline_sequence = _load_pipeline_sequence(manifest_path)

    console.print()
    console.print(f"[{Colors.INFO}]Benchmark:[/{Colors.INFO}] {benchmark}")
    console.print(f"[{Colors.INFO}]Source:[/{Colors.INFO}] {source_file}")
    console.print(f"[{Colors.INFO}]Parallel MLIR:[/{Colors.INFO}] {parallel_mlir}")
    console.print(f"[{Colors.INFO}]Results:[/{Colors.INFO}] {results_json}")
    console.print(f"[{Colors.INFO}]ARTS log:[/{Colors.INFO}] {arts_log}")
    console.print(f"[{Colors.INFO}]OMP log:[/{Colors.INFO}] {omp_log}")
    console.print(f"[{Colors.INFO}]Output dir:[/{Colors.INFO}] {output_dir}")
    console.print(f"[{Colors.INFO}]Pipeline manifest:[/{Colors.INFO}] {manifest_path}")

    verification = result_entry.get("verification", {})
    run_arts = result_entry.get("run_arts", {})
    run_omp = result_entry.get("run_omp", {})
    console.print(
        f"[{Colors.INFO}]Verification:[/{Colors.INFO}] "
        f"correct={verification.get('correct')} "
        f"arts_checksum={verification.get('arts_checksum')} "
        f"omp_checksum={verification.get('omp_checksum')}"
    )
    console.print(
        f"[{Colors.INFO}]Run status:[/{Colors.INFO}] "
        f"arts={run_arts.get('status')} omp={run_omp.get('status')}"
    )
    console.print()

    if stages:
        stage_list = [stage.strip() for stage in stages.split(",") if stage.strip()]
        invalid = [stage for stage in stage_list if stage not in pipeline_sequence]
        if invalid:
            raise Exit(
                "Unknown pipeline stage(s): " + ", ".join(invalid) +
                ". Use `carts pipeline --json` for the source of truth."
            )
        dumped: List[Path] = []
        for stage in stage_list:
            print_info(f"Dumping pipeline stage: {stage}")
            dumped.append(
                _dump_stage(parallel_mlir, source_file, arts_config,
                            stamp.compile_args, stage, output_dir)
            )
        print_success("Benchmark triage artifacts collected")
        console.print(f"[{Colors.INFO}]Dumped stages:[/{Colors.INFO}]")
        for path in dumped:
            console.print(f"  - {path}")
        return

    print_info("Dumping all pipeline stages using compiler manifest order")
    _dump_all_pipelines(parallel_mlir, arts_config, stamp.compile_args, output_dir)
    print_success("Benchmark triage artifacts collected")
    console.print(f"[{Colors.INFO}]Dump mode:[/{Colors.INFO}] all-pipelines")
