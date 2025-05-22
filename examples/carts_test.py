import numpy as np
import matplotlib.pyplot as plt
import subprocess
import re
import json
import argparse
import os
import statistics
import glob
import csv
import datetime
import platform
import matplotlib
import yaml
import seaborn as sns
import sys
import shutil
import pandas as pd
import math
import logging
from pathlib import Path
from collections import defaultdict
from typing import List, Dict, Any, Optional, Tuple, Union

# --- Basic Logging Setup ---
logging.basicConfig(level=logging.INFO, format='[%(levelname)s] %(asctime)s - %(message)s')
logger = logging.getLogger(__name__)

# --- Matplotlib and Seaborn Configuration ---
matplotlib.use('Agg')
sns.set_theme(style="whitegrid")
CUSTOM_COLORS = ["#1f77b4", "#ff7f0e"]  # blue, orange

# --- Performance Event Configuration ---
EVENT_GROUPS = {
    "Cache1": [
        "L1-dcache-loads:u", "L1-dcache-load-misses:u",
        "L1-dcache-stores:u", "L1-icache-load-misses:u",
        "cache-references:u", "cache-misses:u",
    ],
    "Cache2": [
        "L2-dcache-loads:u", "L2-dcache-load-misses:u",
        "l2_rqsts.all_code_rd:u", "l2_rqsts.code_rd_hit:u",
        "l2_rqsts.code_rd_miss:u", "l2_rqsts.miss:u",
    ],
    "Cache3": [
       "L3-dcache-loads:u", "L3-dcache-load-misses:u",
       "mem_inst_retired.all_loads:u", "mem_inst_retired.all_stores:u",
    ],
    "Clocks/Frequency": ["cycles:u", "cpu-clock:u", "task-clock:u"],
    "Stalls": ["stalled-cycles-frontend:u", "stalled-cycles-backend:u",
               "cycle_activity.stalls_total:u", "cycle_activity.stalls_mem_any:u",
               "resource_stalls.sb:u", "resource_stalls.scoreboard:u"],
    "TLB": ["dTLB-load-misses:u", "iTLB-load-misses:u", "dTLB-store-misses:u"],
    "Instructions": ["instructions:u"],
}

BASE_EVENTS = [
    "L1-dcache-loads", "L1-dcache-load-misses", "L1-dcache-stores", "L1-icache-load-misses",
    "l2_rqsts.all_code_rd", "l2_rqsts.code_rd_hit", "l2_rqsts.code_rd_miss", "l2_rqsts.miss",
    "dTLB-load-misses", "dTLB-store-misses", "iTLB-load-misses",
    "cycle_activity.stalls_total", "cycle_activity.stalls_mem_any", "resource_stalls.sb", "resource_stalls.scoreboard",
    "cycles", "instructions", "cpu-clock", "task-clock",
    "mem_inst_retired.all_loads", "mem_inst_retired.all_stores",
    "cache-references", "cache-misses"
]
DEFAULT_PERF_EVENTS = [e + ":u" for e in BASE_EVENTS]


# --- Helper Function (Module Level) ---
def safe_fmt(val: Optional[Union[float, int]], fmt: str = ".2f", na_val: str = "N/A") -> str:
    if val is None or (isinstance(val, float) and math.isnan(val)):
        return na_val
    try:
        return format(val, fmt)
    except (TypeError, ValueError):
        return str(val)

# --- Manager Classes ---

class SystemInfoManager:
    """Manages gathering of system, reproducibility, and dependency information."""
    def __init__(self):
        self._system_info: Optional[Dict[str, Any]] = None
        self._reproducibility_info: Optional[Dict[str, Any]] = None
        self._dependency_versions: Optional[Dict[str, str]] = None

    def get_system_info(self) -> Dict[str, Any]:
        if self._system_info is None:
            info = {"timestamp": datetime.datetime.now().isoformat()}
            try:
                cpu_info_raw = subprocess.check_output("lscpu", text=True, stderr=subprocess.DEVNULL).strip()
                cpu_model = re.search(r"Model name:\s*(.*)", cpu_info_raw)
                cpu_cores = re.search(r"^CPU\(s\):\s*(\d+)", cpu_info_raw, re.MULTILINE)
                info["cpu_model"] = cpu_model.group(1).strip() if cpu_model else "N/A"
                info["cpu_cores"] = int(cpu_cores.group(1)) if cpu_cores else "N/A"
            except Exception as e:
                logger.warning(f"Could not get CPU info: {e}")
                info["cpu_model"], info["cpu_cores"] = "N/A", "N/A"
            try:
                mem_info_raw = subprocess.check_output("free -m", text=True, shell=True, stderr=subprocess.DEVNULL).strip()
                mem_total = re.search(r"Mem:\s*(\d+)", mem_info_raw)
                info["memory_total_mb"] = int(mem_total.group(1)) if mem_total else "N/A"
            except Exception as e:
                logger.warning(f"Could not get Memory info: {e}")
                info["memory_total_mb"] = "N/A"
            try:
                clang_version_raw = subprocess.check_output("clang --version", text=True, shell=True, stderr=subprocess.DEVNULL).strip()
                info["clang_version"] = clang_version_raw.splitlines()[0] if clang_version_raw else "N/A"
            except Exception as e:
                logger.warning(f"Could not get clang version: {e}")
                info["clang_version"] = "N/A"
            info["os_platform"] = platform.platform()
            info["os_uname"] = " ".join(platform.uname())
            self._system_info = info
        return self._system_info

    def _get_git_commit(self, path: Path) -> str:
        try:
            return subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True, stderr=subprocess.DEVNULL).strip()
        except Exception:
            return 'N/A'

    def get_dependency_versions(self, project_root: Path) -> Dict[str, str]:
        if self._dependency_versions is None:
            deps_paths = {
                'arts': project_root / 'external/arts',
                'polygeist': project_root / 'external/Polygeist',
                'polygeist_llvm': project_root / 'external/Polygeist/llvm-project/llvm',
            }
            versions = {}
            for name, path in deps_paths.items():
                versions[name] = self._get_git_commit(path)
            self._dependency_versions = versions
        return self._dependency_versions

    def get_reproducibility_info(self, project_root: Path) -> Dict[str, Any]:
        if self._reproducibility_info is None:
            info = {'python_version': sys.version.replace('\n', ' ')}
            try:
                pip_freeze = subprocess.check_output([sys.executable, '-m', 'pip', 'freeze'], text=True, stderr=subprocess.DEVNULL)
                info['pip_freeze'] = pip_freeze.strip().split('\n')
            except Exception as e:
                info['pip_freeze'] = [f"Error getting pip freeze: {e}"]
            info['env_vars'] = {k: v for k, v in os.environ.items() if k.startswith(('OMP_', 'CARTS_', 'PATH', 'LD_'))}
            info['git_commit_main_project'] = self._get_git_commit(project_root)
            self._reproducibility_info = info
        return self._reproducibility_info

class DirectoryManager:
    """Manages workspace directories and finding example configurations."""
    def __init__(self, project_root: Path, example_base_dirs_config: List[str], output_dir_name: str = "output"):
        self.project_root = project_root
        self.example_base_paths = [project_root / d for d in example_base_dirs_config]
        self.base_output_dir = project_root / output_dir_name
        self.output_dir: Optional[Path] = None

    def setup_output_directory(self, clear_existing: bool) -> Path:
        if clear_existing:
            self.output_dir = self.base_output_dir
            if self.output_dir.exists():
                self._clear_directory_contents(self.output_dir)
        else:
            self.output_dir = self._get_next_available_dir(self.base_output_dir)

        self.output_dir.mkdir(parents=True, exist_ok=True)
        logger.info(f"Output will be saved to directory: {self.output_dir.resolve()}")
        return self.output_dir

    def _clear_directory_contents(self, dir_path: Path):
        logger.info(f"Clearing contents of {dir_path}")
        for item in dir_path.iterdir():
            if item.name == '.gitignore':
                continue
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()

    def _get_next_available_dir(self, base_dir: Path) -> Path:
        if not base_dir.exists() or not any(base_dir.iterdir()):
            return base_dir
        i = 1
        while True:
            candidate_dir = Path(f"{base_dir}_{i}")
            if not candidate_dir.exists():
                return candidate_dir
            i += 1

    def find_example_directories(self, target_example_names: Optional[List[str]] = None) -> List[Path]:
        example_dirs = []
        for base_path in self.example_base_paths:
            if not base_path.is_dir():
                logger.warning(f"Example base path {base_path} does not exist or is not a directory.")
                continue
            for item_path in base_path.iterdir():
                if item_path.is_dir():
                    # Ignore any path that is inside a 'benchmarks' directory (as per original logic)
                    if 'benchmarks' in [p.name for p in item_path.parents]:
                        continue
                    config_file = item_path / 'test_config.yaml'
                    if config_file.is_file():
                        example_dirs.append(item_path)

        logger.info(f"Found {len(example_dirs)} example directories with test_config.yaml.")

        if target_example_names:
            filtered_dirs = [d for d in example_dirs if d.name in target_example_names]
            if len(filtered_dirs) != len(target_example_names):
                found_names = {d.name for d in filtered_dirs}
                missing_targets = [t for t in target_example_names if t not in found_names]
                logger.warning(f"Could not find all specified target examples. Missing: {missing_targets}")
            example_dirs = filtered_dirs
            logger.info(f"Filtered to {len(example_dirs)} target examples: {[d.name for d in example_dirs]}")

        return example_dirs

class ConfigManager:
    """Manages loading and interpreting test configurations for a specific example."""
    def __init__(self, example_dir: Path, cli_args: Optional[argparse.Namespace] = None):
        self.example_dir = example_dir
        self.cli_args = cli_args
        self.test_config_path = example_dir / 'test_config.yaml'
        self.arts_cfg_path = example_dir / 'arts.cfg'
        self._test_config: Optional[Dict[str, Any]] = None
        self._arts_config: Optional[Dict[str, Any]] = None

    def load_test_config(self) -> Dict[str, Any]:
        if self._test_config is None:
            if not self.test_config_path.is_file():
                logger.error(f"test_config.yaml not found in {self.example_dir}")
                return {}
            with open(self.test_config_path, 'r') as f:
                config = yaml.safe_load(f)
            # Support both old and new config formats
            if 'benchmark' in config or 'profiling' in config:
                self._test_config = config
            else: # Old format
                self._test_config = {'benchmark': config, 'profiling': {}}
        return self._test_config if self._test_config else {}

    def get_benchmark_config(self) -> Dict[str, Any]:
        return self.load_test_config().get('benchmark', {})

    def get_profiling_config(self) -> Dict[str, Any]:
        return self.load_test_config().get('profiling', {})

    def is_profiling_enabled(self) -> bool:
        return self.get_profiling_config().get('profiling_enabled', False)

    def is_counter_enabled(self) -> bool:
        return self.get_profiling_config().get('counter_enabled', False)

    def is_introspection_enabled(self) -> bool:
        return self.get_profiling_config().get('introspection_enabled', False)

    def get_run_plan(self) -> List[Tuple[str, int, str]]:
        bench_cfg = self.get_benchmark_config()
        problem_sizes = bench_cfg.get("problem_sizes", [])
        iterations_list = bench_cfg.get("iterations", [])
        args_list = bench_cfg.get("args", [""] * len(problem_sizes))

        if not (problem_sizes and iterations_list and len(problem_sizes) == len(iterations_list) == len(args_list)):
            logger.error(f"Invalid or mismatched problem_sizes/iterations/args in {self.test_config_path}")
            return []
        return list(zip(problem_sizes, iterations_list, args_list))

    def get_threads_list(self) -> List[int]:
        return self.get_benchmark_config().get("threads", [1])

    def parse_arts_cfg(self) -> Dict[str, Optional[Union[int, str]]]:
        if self._arts_config is None:
            config: Dict[str, Optional[Union[int, str]]] = {"threads": None, "nodeCount": None}
            if not self.arts_cfg_path.exists():
                self._arts_config = config
                return self._arts_config
            try:
                with open(self.arts_cfg_path, 'r') as f:
                    for line in f:
                        line = line.strip()
                        if line.startswith("#") or "=" not in line:
                            continue
                        key, value = line.split("=", 1)
                        key = key.strip()
                        value = value.strip()
                        if key == "threads":
                            config["threads"] = int(value)
                        elif key == "nodeCount":
                            config["nodeCount"] = int(value)
            except Exception as e:
                logger.error(f"Error parsing {self.arts_cfg_path}: {e}")
            self._arts_config = config
        return self._arts_config

    def update_arts_cfg_threads(self, num_threads: int) -> None:
        lines = []
        found = False
        if self.arts_cfg_path.exists():
            with open(self.arts_cfg_path, 'r') as f:
                for line in f:
                    if line.strip().startswith('threads'):
                        lines.append(f"threads = {num_threads}\n")
                        found = True
                    else:
                        lines.append(line)
        if not found:
            lines.append(f"threads = {num_threads}\n")
        with open(self.arts_cfg_path, 'w') as f:
            f.writelines(lines)
        self._arts_config = None # Invalidate cached arts_config

    def get_grouped_perf_events(self) -> Optional[List[List[str]]]:
        """Returns a list of event groups (each a list of events) to be run separately."""
        profiling_section = self.get_profiling_config()
        extra_events = set()
        if self.cli_args and hasattr(self.cli_args, 'perf_events') and self.cli_args.perf_events:
            extra_events.update(self.cli_args.perf_events)
        if profiling_section and 'perf_events' in profiling_section:
            extra_events.update(profiling_section['perf_events'])

        def ensure_user_mode(ev: str) -> str:
            return ev if ev.endswith((":u", ":k")) else ev + ":u"

        all_defined_events = DEFAULT_PERF_EVENTS + [ensure_user_mode(e) for e in extra_events]
        # Use dict.fromkeys to preserve order and remove duplicates
        unique_events = list(dict.fromkeys(all_defined_events))

        grouped_runs: List[List[str]] = []
        used_events = set()

        for _, group_config_events in EVENT_GROUPS.items():
            current_group_run = [ev for ev in group_config_events if ev in unique_events]
            if current_group_run:
                grouped_runs.append(current_group_run)
                used_events.update(current_group_run)

        remaining_events = [ev for ev in unique_events if ev not in used_events]
        if remaining_events:
            grouped_runs.append(remaining_events)

        return grouped_runs if grouped_runs else None


class ExecutionManager:
    """Handles execution of make commands and program instances (benchmark/profiling)."""
    def __init__(self, timeout_seconds: int):
        self.timeout_seconds = timeout_seconds

    def run_make_command(self, make_target: str, cwd: Path) -> bool:
        logger.info(f"Running 'make {make_target}' in {cwd}...")
        try:
            process = subprocess.run(
                ["make", make_target], cwd=cwd, check=True, text=True, capture_output=True
            )
            logger.debug(f"'make {make_target}' stdout: {process.stdout}")
            logger.info(f"'make {make_target}' successful in {cwd}.")
            return True
        except subprocess.CalledProcessError as e:
            logger.error(f"Error during 'make {make_target}' in {cwd}: {e.stderr}")
            return False
        except FileNotFoundError:
            logger.error(f"Error: 'make' command not found.")
            return False

    def _run_single_program(self, command_list: List[str], cwd: Path, iteration_num: int) -> Tuple[Optional[float], str, str]:
        """ Helper to run one iteration, returns (time_sec, correctness_status, output_stdout) """
        time_val: Optional[float] = None
        correctness = "UNKNOWN"
        full_output = ""
        try:
            process = subprocess.run(
                command_list, capture_output=True, text=True, check=True, cwd=cwd,
                timeout=self.timeout_seconds
            )
            full_output = process.stdout + "\n" + process.stderr

            time_match = re.search(r"Finished in\s+([0-9]*\.?[0-9]+)\s*seconds?", full_output, re.IGNORECASE)
            if time_match:
                time_val = float(time_match.group(1))
            else:
                logger.warning(f"Iter {iteration_num + 1}: Time not parsed. Output snippet: {full_output[:200].replace(chr(10), ' ')}...")

            correctness_match = re.search(r"Result: (CORRECT|INCORRECT)", full_output, re.IGNORECASE)
            if correctness_match:
                correctness = correctness_match.group(1)

        except subprocess.TimeoutExpired:
            logger.warning(f"Iter {iteration_num + 1}: TIMEOUT after {self.timeout_seconds}s for {command_list[0]}.")
            correctness = "TIMEOUT"
        except subprocess.CalledProcessError as e:
            logger.error(f"Error running {command_list[0]} (iter {iteration_num + 1}): {e.stderr}")
            full_output = e.stdout + "\n" + e.stderr
            correctness = "ERROR_RUNTIME"
        except FileNotFoundError:
            logger.error(f"Executable {command_list[0]} not found during run iteration.")
            correctness = "ERROR_MISSING_EXE"

        return time_val, correctness, full_output

    def run_benchmark_instance(self, executable_path: Path, program_args_str: str, num_iterations: int,
                               example_name: str, version_name: str) -> Dict[str, Any]:
        if not executable_path.exists():
            logger.error(f"Executable {executable_path} not found. Skipping benchmark.")
            return {"times_seconds": [], "iterations_ran": 0, "iterations_requested": num_iterations, "correctness_status": "NOT_RUN (Executable Missing)"}

        logger.info(f"Running benchmark: {version_name} ('{executable_path.name}') for '{example_name}', args='{program_args_str}', iters={num_iterations}...")

        times: List[Optional[float]] = []
        final_correctness = "UNKNOWN"

        program_arg_list = program_args_str.split()
        command = [str(executable_path)] + program_arg_list

        for i in range(num_iterations):
            time_val, iter_correctness, _ = self._run_single_program(command, executable_path.parent, i)
            times.append(time_val)
            if iter_correctness != "UNKNOWN":
                 if final_correctness == "UNKNOWN" or final_correctness == "CORRECT":
                    final_correctness = iter_correctness
            logger.debug(f"  Iter {i+1}: {safe_fmt(time_val, '.6f')}s, Status: {iter_correctness}")


        valid_times = [t for t in times if t is not None]
        return {
            "times_seconds": valid_times,
            "iterations_ran": len(valid_times),
            "iterations_requested": num_iterations,
            "correctness_status": final_correctness,
        }

    def run_profiling_instance(self, executable_path: Path, program_args_str: str, num_iterations: int,
                                example_name: str, version_name: str, perf_event_groups: List[List[str]]) -> Dict[str, Any]:
        if not executable_path.exists():
            logger.error(f"Executable {executable_path} not found. Skipping perf run.")
            return {"event_stats": {}, "times_seconds": [], "iterations_ran": 0, "iterations_requested": num_iterations, "correctness_status": "NOT_RUN (Executable Missing)"}

        logger.info(f"Running profiling: {version_name} ('{executable_path.name}') for '{example_name}', args='{program_args_str}', iters={num_iterations}...")

        event_data_accumulator: Dict[str, List[float]] = defaultdict(list)
        # Time from the first event group run of each iteration
        times_from_perf_runs: List[Optional[float]] = []
        final_correctness = "UNKNOWN"

        program_arg_list = program_args_str.split()

        for i in range(num_iterations):
            iter_correctness_overall = "UNKNOWN"
            time_for_this_iter: Optional[float] = None

            for group_idx, event_group in enumerate(perf_event_groups):
                if not event_group: continue

                perf_cmd = ["perf", "stat", "-a", "-x", ",", "-e", ','.join(event_group), "--", str(executable_path)] + program_arg_list

                time_val, iter_correctness_group, perf_output = self._run_single_program(perf_cmd, executable_path.parent, i)

                if group_idx == 0: # Capture time and correctness from the first group run
                    time_for_this_iter = time_val
                    if iter_correctness_group != "UNKNOWN":
                        if iter_correctness_overall == "UNKNOWN" or iter_correctness_overall == "CORRECT":
                             iter_correctness_overall = iter_correctness_group

                # Parse perf output
                for line in perf_output.splitlines():
                    parts = line.strip().split(",")
                    # Check for perf stat CSV format
                    if len(parts) >= 3 and not parts[0].strip().startswith("#"):
                        try:
                            val_str = parts[0].replace('<not supported>', 'nan').replace('<not counted>', 'nan')
                            val = float(val_str)
                            event_name_raw = parts[2].strip()
                            # Keep the :u suffix if present, as it's part of the requested event
                            event_name = event_name_raw
                            event_data_accumulator[event_name].append(val)
                        except ValueError:
                            pass

            times_from_perf_runs.append(time_for_this_iter)
            if iter_correctness_overall != "UNKNOWN":
                if final_correctness == "UNKNOWN" or final_correctness == "CORRECT":
                    final_correctness = iter_correctness_overall
            logger.debug(f"  Iter {i+1} (Profiling): {safe_fmt(time_for_this_iter, '.6f')}s, Status: {iter_correctness_overall}")

        processed_event_stats: Dict[str, Dict[str, List[float]]] = {}
        for event, values in event_data_accumulator.items():
            valid_values = [v for v in values if not math.isnan(v)]
            processed_event_stats[event] = {"all_values": valid_values}

        valid_times = [t for t in times_from_perf_runs if t is not None]
        return {
            "event_stats": processed_event_stats,
            "times_seconds": valid_times,
            "iterations_ran": len(valid_times),
            "iterations_requested": num_iterations,
            "correctness_status": final_correctness,
        }


class ResultsManager:
    """Manages collection, aggregation, and reporting of benchmark results."""
    def __init__(self):
        self._benchmark_runs: List[Dict[str, Any]] = []
        self._profiling_runs: List[Dict[str, Any]] = []

    def add_benchmark_run_data(self, data: Dict[str, Any]):
        self._benchmark_runs.append(data)

    def add_profiling_run_data(self, data: Dict[str, Any]):
        self._profiling_runs.append(data)

    def get_raw_benchmark_runs(self) -> List[Dict[str, Any]]:
        return self._benchmark_runs

    def get_raw_profiling_runs(self) -> List[Dict[str, Any]]:
        return self._profiling_runs

    def _compute_runtime_stats(self, times: List[float]) -> Dict[str, Optional[float]]:
        if not times:
            return {"min": None, "max": None, "avg": None, "median": None, "stdev": None}
        return {
            "min": min(times),
            "max": max(times),
            "avg": statistics.mean(times),
            "median": statistics.median(times),
            "stdev": statistics.stdev(times) if len(times) > 1 else 0.0,
        }

    def _compute_event_stats(self, values: List[float]) -> Dict[str, Optional[float]]:
        # Same computation as runtime, but for event counts
        return self._compute_runtime_stats(values)

    def prepare_aggregated_data(self) -> List[Dict[str, Any]]:
        """
        Aggregates benchmark and profiling data for comprehensive reporting.
        Ensures CARTS and OMP results for the same logical configuration (example, problem size, threads)
        are grouped together for speedup calculation.
        """
        # Key for grouping CARTS vs OMP for the same logical setup (example, problem size, common thread count)
        comparison_key_func = lambda r: (
            r["example_name"],
            r["problem_size"],
            r.get("threads", 1)
        )

        # Structure to hold grouped data before final row creation
        # Key: (example_name, problem_size, threads)
        # Value: {
        #     "CARTS_benchmark_data": {...}, "OMP_benchmark_data": {...},
        #     "CARTS_profiling_data": {...}, "OMP_profiling_data": {...},
        #     "carts_config_details": {...} # Specific arts.cfg for the CARTS run
        # }
        processed_runs: Dict[Tuple, Dict[str, Any]] = defaultdict(lambda: {
            "CARTS_benchmark_data": None, "OMP_benchmark_data": None,
            "CARTS_profiling_data": None, "OMP_profiling_data": None,
            "carts_config_details": {}
        })

        all_event_names = set()

        # Populate with benchmark data
        for run in self._benchmark_runs:
            key = comparison_key_func(run)
            version_upper = run["version"].upper() # CARTS or OMP
            processed_runs[key][f"{version_upper}_benchmark_data"] = run
            if version_upper == "CARTS" and run.get("carts_config"):
                processed_runs[key]["carts_config_details"] = run.get("carts_config", {})

        # Populate with profiling data
        for run in self._profiling_runs:
            key = comparison_key_func(run)
            version_upper = run["version"].upper()
            processed_runs[key][f"{version_upper}_profiling_data"] = run
            # Ensure carts_config_details is captured if it wasn't from a benchmark run (e.g., if only profiling CARTS)
            if version_upper == "CARTS" and not processed_runs[key]["carts_config_details"] and run.get("carts_config"):
                 processed_runs[key]["carts_config_details"] = run.get("carts_config", {})

            if "event_stats" in run:
                all_event_names.update(run["event_stats"].keys())

        sorted_event_names = sorted(list(all_event_names))
        output_rows = []

        for key_tuple, data_group in sorted(processed_runs.items()):
            example_name, problem_size, common_threads = key_tuple

            carts_config_specific = data_group.get("carts_config_details", {})

            row: Dict[str, Any] = {
                "Example Name": example_name,
                "Problem Size": problem_size,
                "Threads": common_threads,
                "CARTS Config Threads": carts_config_specific.get("threads", ""),
                "CARTS Config Nodes": carts_config_specific.get("nodeCount", ""),
            }

            carts_bench_run = data_group.get("CARTS_benchmark_data")
            omp_bench_run = data_group.get("OMP_benchmark_data")

            carts_times = carts_bench_run.get("times_seconds", []) if carts_bench_run else []
            omp_times = omp_bench_run.get("times_seconds", []) if omp_bench_run else []

            carts_runtime_stats = self._compute_runtime_stats(carts_times)
            omp_runtime_stats = self._compute_runtime_stats(omp_times)

            # Populate benchmark stats for CARTS
            row.update({
                "CARTS Median Time (s)": carts_runtime_stats["median"],
                "CARTS Avg Time (s)": carts_runtime_stats["avg"],
                "CARTS Min Time (s)": carts_runtime_stats["min"],
                "CARTS Max Time (s)": carts_runtime_stats["max"],
                "CARTS StdDev Time (s)": carts_runtime_stats["stdev"],
                "CARTS Times (s)": carts_times,
                "CARTS Correctness": carts_bench_run.get("correctness_status", "N/A") if carts_bench_run else "N/A",
                "CARTS Iterations Ran": carts_bench_run.get("iterations_ran", 0) if carts_bench_run else 0,
                "CARTS Iterations Requested": carts_bench_run.get("iterations_requested", 0) if carts_bench_run else 0,
            })

            # Populate benchmark stats for OMP
            row.update({
                "OMP Median Time (s)": omp_runtime_stats["median"],
                "OMP Avg Time (s)": omp_runtime_stats["avg"],
                "OMP Min Time (s)": omp_runtime_stats["min"],
                "OMP Max Time (s)": omp_runtime_stats["max"],
                "OMP StdDev Time (s)": omp_runtime_stats["stdev"],
                "OMP Times (s)": omp_times,
                "OMP Correctness": omp_bench_run.get("correctness_status", "N/A") if omp_bench_run else "N/A",
                "OMP Iterations Ran": omp_bench_run.get("iterations_ran", 0) if omp_bench_run else 0,
                "OMP Iterations Requested": omp_bench_run.get("iterations_requested", 0) if omp_bench_run else 0,
            })

            # Calculate Speedup (OMP median / CARTS median)
            if carts_runtime_stats["median"] is not None and omp_runtime_stats["median"] is not None and carts_runtime_stats["median"] > 0:
                row["Speedup (OMP/CARTS)"] = omp_runtime_stats["median"] / carts_runtime_stats["median"]
            else:
                row["Speedup (OMP/CARTS)"] = None

            # Process profiling data for events
            for version_prefix_upper in ["CARTS", "OMP"]:
                prof_run = data_group.get(f"{version_prefix_upper}_profiling_data")
                event_stats_raw = prof_run.get("event_stats", {}) if prof_run else {}

                for event_name in sorted_event_names:
                    values = event_stats_raw.get(event_name, {}).get("all_values", [])
                    event_summary_stats = self._compute_event_stats(values)
                    for stat_name_short in ["avg", "median", "min", "max", "stdev"]:
                        col_name = f"{version_prefix_upper} Event {event_name} {stat_name_short.capitalize()}"
                        row[col_name] = event_summary_stats[stat_name_short]
            output_rows.append(row)
        return output_rows


    def compute_overall_statistics(self, aggregated_data: List[Dict[str, Any]]) -> Dict[str, Any]:
        speedups = [r["Speedup (OMP/CARTS)"] for r in aggregated_data if r.get("Speedup (OMP/CARTS)") is not None]

        carts_wins = 0
        omp_wins = 0
        ties = 0
        max_speedup_val: Optional[float] = None
        min_speedup_val: Optional[float] = None
        max_speedup_example: Optional[str] = None
        min_speedup_example: Optional[str] = None

        for r in aggregated_data:
            s = r.get("Speedup (OMP/CARTS)")
            if s is not None:
                if s > 1.05: carts_wins +=1
                elif s < 0.95: omp_wins += 1
                else: ties += 1

                if max_speedup_val is None or s > max_speedup_val:
                    max_speedup_val = s
                    max_speedup_example = f"{r['Example Name']} (Size: {r['Problem Size']}, Threads: {r['Threads']})"
                if min_speedup_val is None or s < min_speedup_val:
                    min_speedup_val = s
                    min_speedup_example = f"{r['Example Name']} (Size: {r['Problem Size']}, Threads: {r['Threads']})"

        geom_mean = None
        if speedups:
            # Geometric mean requires positive values
            valid_speedups = [s for s in speedups if s > 0]
            if valid_speedups:
                 try:
                    geom_mean = statistics.geometric_mean(valid_speedups)
                 except statistics.StatisticsError:
                    logger.warning("Could not compute geometric mean of speedups (likely due to non-positive values after filtering).")


        correctness_counts = defaultdict(int)
        # Iterate raw benchmark runs for correctness summary, as this is per-run, not per-comparison-group
        for r_data in self._benchmark_runs:
            version_key = r_data["version"].upper()
            status = r_data.get('correctness_status', 'UNKNOWN')
            correctness_counts[f"{version_key}_{status}"] += 1

        return {
            "geometric_mean_speedup": geom_mean,
            "total_configurations_compared": len(speedups),
            "carts_faster_configs": carts_wins,
            "omp_faster_configs": omp_wins,
            "tied_configs": ties,
            "max_speedup": {"value": max_speedup_val, "example_config": max_speedup_example},
            "min_speedup": {"value": min_speedup_val, "example_config": min_speedup_example},
            "correctness_summary": dict(correctness_counts),
            "all_speedups_raw": speedups
        }

    def collect_failure_report(self, aggregated_data: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        failures = []
        # This can now iterate through the aggregated_data which has both CARTS and OMP correctness for a given config
        for r in aggregated_data:
            for version_label, version_prefix_key in [("CARTS", "CARTS"), ("OpenMP", "OMP")]:
                # Key for correctness in the aggregated row
                correctness_key = f"{version_prefix_key} Correctness"
                correctness = r.get(correctness_key)

                if correctness not in [None, "CORRECT", "OK", "N/A"]:
                    iterations_ran_key = f"{version_prefix_key} Iterations Ran"
                    failures.append({
                        "example": r["Example Name"],
                        "problem_size": r["Problem Size"],
                        "threads": r["Threads"],
                        "version": version_label,
                        "status": correctness,
                        "iterations_ran": r.get(iterations_ran_key, 0)
                    })
        return failures

    def write_csv_report(self, aggregated_data: List[Dict[str, Any]], output_path: Path) -> None:
        if not aggregated_data:
            logger.info("No aggregated data to write to CSV.")
            return

        # Dynamically determine fieldnames from the first row, ensuring consistent order
        # Fallback to an empty dict if aggregated_data is empty, though we check above.
        fieldnames = list(aggregated_data[0].keys()) if aggregated_data else []

        # Ensure common fields are first for better readability
        common_fields = [
            "Example Name", "Problem Size", "Threads",
            "CARTS Config Threads", "CARTS Config Nodes", "Speedup (OMP/CARTS)",
            "CARTS Median Time (s)", "OMP Median Time (s)",
            "CARTS Correctness", "OMP Correctness"
        ]
        # Start with common fields that are present, then add the rest
        ordered_fieldnames = [f for f in common_fields if f in fieldnames]
        ordered_fieldnames += sorted([f for f in fieldnames if f not in ordered_fieldnames])


        with open(output_path, 'w', newline='') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=ordered_fieldnames, restval="N/A", extrasaction='ignore')
            writer.writeheader()
            for row_dict in aggregated_data:
                # Format numerical data and lists for CSV
                formatted_row = {}
                for key, val in row_dict.items():
                    if isinstance(val, float):
                        formatted_row[key] = safe_fmt(val)
                    elif key.endswith("Times (s)") and isinstance(val, list):
                        formatted_row[key] = ", ".join([safe_fmt(t, ".6f") for t in val])
                    elif isinstance(val, list):
                         formatted_row[key] = ", ".join([str(x) for x in val])
                    else:
                        formatted_row[key] = val if val is not None else "N/A"
                writer.writerow(formatted_row)
        logger.info(f"Aggregated CSV report saved to {output_path}")

    def write_json_report(self, final_json_data: Dict[str, Any], output_path: Path) -> None:
        def default_serializer(obj):
            if isinstance(obj, Path):
                return str(obj)
            # Let the default error for unhandled types
            raise TypeError(f"Object of type {obj.__class__.__name__} is not JSON serializable")

        with open(output_path, "w") as f:
            json.dump(final_json_data, f, indent=2, default=default_serializer)
        logger.info(f"JSON report saved to {output_path}")

    def save_figure(self, fig, img_path: Path, pdf_path: Path):
        """Saves a matplotlib figure to both PNG and PDF."""
        try:
            fig.savefig(img_path, bbox_inches='tight', dpi=300)
            logger.info(f"Saved PNG figure to {img_path}")
            fig.savefig(pdf_path, bbox_inches='tight')
            logger.info(f"Saved PDF figure to {pdf_path}")
        except Exception as e:
            logger.error(f"Error saving figure: {e}")
        # Close the figure to free memory
        plt.close(fig)



# --- Main Orchestration Logic ---
def main():
    logger.info("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")
    logger.info("CARTS Performance Comparison Script")

    parser = argparse.ArgumentParser(description="Run and compare performance of example programs.")
    parser.add_argument("--timeout_seconds", type=int, default=300, help="Timeout for each program run (default: 300s).")
    parser.add_argument("--output_prefix", type=str, default="performance_results", help="Prefix for output files.")
    parser.add_argument("--example_base_dirs", nargs='+', default=["examples/parallel", "examples/tasking"], help="Base directories for examples.")
    parser.add_argument("--target_examples", nargs='+', default=None, help="Specific example names to run.")
    parser.add_argument('--clear_output', action='store_true', help='Clear the output directory before running if it matches base name.')
    parser.add_argument('--no_clear_output', action='store_false', dest='clear_output', help='Do not clear output directory, create a new one if base exists.')
    parser.set_defaults(clear_output=True)
    parser.add_argument('--perf_events', nargs='+', default=None, help='CLI override for perf events.')
    args = parser.parse_args()

    # Assuming script is in project root or similar
    project_root = Path(__file__).resolve().parent

    dir_manager = DirectoryManager(project_root, args.example_base_dirs, "output_benchmarks")
    output_dir = dir_manager.setup_output_directory(args.clear_output)

    sys_info_manager = SystemInfoManager()
    exec_manager = ExecutionManager(args.timeout_seconds)
    results_manager = ResultsManager()

    example_paths_to_run = dir_manager.find_example_directories(args.target_examples)
    if not example_paths_to_run:
        logger.info("No examples found to run. Exiting.")
        return

    run_id_counter = 1

    for example_path in example_paths_to_run:
        example_name = example_path.name
        logger.info(f"\nProcessing example: {example_name} in {example_path}")

        config_manager = ConfigManager(example_path, args)
        # Checks if test_config.yaml is valid
        if not config_manager.load_test_config():
            logger.warning(f"Skipping {example_name} due to missing or invalid test_config.yaml.")
            continue

        run_plan = config_manager.get_run_plan()
        threads_list_for_example = config_manager.get_threads_list()

        is_profiling_overall_enabled = config_manager.is_profiling_enabled()
        perf_event_groups_for_example = config_manager.get_grouped_perf_events() if is_profiling_overall_enabled else None

        if not run_plan:
            logger.warning(f"Skipping {example_name} due to invalid run plan in test_config.yaml.")
            continue

        for common_thread_count in threads_list_for_example:
            logger.info(f"Configuring for common_thread_count={common_thread_count} (for make, OMP_NUM_THREADS, and CARTS config).")
            # This updates arts.cfg based on the common_thread_count for CARTS runs
            config_manager.update_arts_cfg_threads(common_thread_count)
            # This sets OMP_NUM_THREADS for OpenMP runs
            os.environ["OMP_NUM_THREADS"] = str(common_thread_count)

            if not exec_manager.run_make_command("clean", example_path):
                logger.error(f"'make clean' failed for {example_name} (threads={common_thread_count}). Skipping problem sizes for this config.")
                continue
            # Make sure this builds both CARTS and OMP versions
            if not exec_manager.run_make_command("all", example_path):
                logger.error(f"'make all' failed for {example_name} (threads={common_thread_count}). Skipping problem sizes for this config.")
                continue

            # This is the specific arts.cfg content for CARTS runs under this common_thread_count
            # It should reflect what update_arts_cfg_threads set.
            current_arts_config_for_carts_runs = config_manager.parse_arts_cfg()


            for problem_size_str, num_iterations, specific_args_str in run_plan:
                executable_map = {
                    "CARTS": example_path / example_name,
                    "OMP": example_path / f"{example_name}_omp"
                }
                common_run_args_str = problem_size_str + (f" {specific_args_str}" if specific_args_str else "")

                for version_name, executable_file in executable_map.items():
                    # 1. Run Benchmark (no perf)
                    bench_result_data = exec_manager.run_benchmark_instance(
                        executable_file, common_run_args_str, num_iterations,
                        example_name, version_name
                    )
                    results_manager.add_benchmark_run_data({
                        "run_id": run_id_counter, "example_name": example_name, "problem_size": problem_size_str,
                        "threads": common_thread_count,
                        "version": version_name, "run_type": "benchmark",
                        # Store specific CARTS config only for CARTS runs
                        "carts_config": current_arts_config_for_carts_runs if version_name == "CARTS" else {},
                        "iterations_requested": num_iterations, **bench_result_data
                    })
                    run_id_counter += 1

                    # 2. Run Profiling (with perf), if enabled
                    if is_profiling_overall_enabled and perf_event_groups_for_example:
                        prof_result_data = exec_manager.run_profiling_instance(
                            executable_file, common_run_args_str, num_iterations,
                            example_name, version_name, perf_event_groups_for_example
                        )
                        results_manager.add_profiling_run_data({
                            "run_id": run_id_counter, "example_name": example_name, "problem_size": problem_size_str,
                            "threads": common_thread_count,
                            "version": version_name, "run_type": "profiling",
                            "carts_config": current_arts_config_for_carts_runs if version_name == "CARTS" else {},
                            "iterations_requested": num_iterations, **prof_result_data
                        })
                        run_id_counter += 1

    # --- Generate Reports ---
    logger.info("Aggregating results for reporting...")
    aggregated_data_for_reports = results_manager.prepare_aggregated_data()

    csv_output_path = output_dir / f"{args.output_prefix}_summary_report.csv"
    results_manager.write_csv_report(aggregated_data_for_reports, csv_output_path)

    overall_stats = results_manager.compute_overall_statistics(aggregated_data_for_reports)
    failure_report = results_manager.collect_failure_report(aggregated_data_for_reports)

    final_json_payload = {
        "system_information": sys_info_manager.get_system_info(),
        "dependency_versions": sys_info_manager.get_dependency_versions(project_root),
        "reproducibility_info": sys_info_manager.get_reproducibility_info(project_root),
        "command_line_args": vars(args),
        "overall_statistics": overall_stats,
        "failure_summary": failure_report,
        "detailed_aggregated_results": aggregated_data_for_reports,
        "raw_benchmark_runs": results_manager.get_raw_benchmark_runs(),
        "raw_profiling_runs": results_manager.get_raw_profiling_runs(),
    }
    json_output_path = output_dir / f"{args.output_prefix}_full_data.json"
    results_manager.write_json_report(final_json_payload, json_output_path)

    logger.info("\nComparison Results (Overall Statistics):")
    logger.info(json.dumps(overall_stats, indent=2, default=str))

    logger.info("Script finished.")
    logger.info("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")

if __name__ == "__main__":
    main()