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
matplotlib.use('Agg')
sns.set_theme(style="whitegrid")
CUSTOM_COLORS = ["#1f77b4", "#ff7f0e"]  # blue, orange

# --- Perf event groups ---
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

# Helper for safe float formatting (avoids TypeError on None)
def safe_fmt(val, fmt=".2f", na="N/A"):
    if val is None:
        return na
    try:
        return format(val, fmt)
    except Exception:
        return str(val)

# Helper to save figures as PNG and PDF
def save_figure(fig, img_path, pdf_path):
    try:
        fig.savefig(img_path, bbox_inches='tight', dpi=300)
        fig.savefig(pdf_path, bbox_inches='tight')
    except Exception as e:
        print(f"Error saving figure: {e}")

# Default perf events for profiling (user mode only, ':u' suffix is always used)
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

# Perf events selection logic (moved to top-level)
def get_grouped_perf_events(yaml_profiling_section, args=None, event_groups=EVENT_GROUPS):
    """
    Returns a list of event groups (each a list of events) to be run separately.
    """
    extra_events = set()
    if args and hasattr(args, 'perf_events') and args.perf_events:
        extra_events.update(args.perf_events)
    if yaml_profiling_section and 'perf_events' in yaml_profiling_section:
        extra_events.update(yaml_profiling_section['perf_events'])
    def ensure_user_mode(ev):
        return ev if ev.endswith(":u") or ev.endswith(":k") else ev+":u"
    all_events = list(dict.fromkeys(DEFAULT_PERF_EVENTS + [ensure_user_mode(e) for e in extra_events]))
    grouped = []
    used = set()
    for group_name, group_events in event_groups.items():
        group = [ev for ev in group_events if ev in all_events]
        if group:
            grouped.append(group)
            used.update(group)
    # Any events not in a group go in their own group
    other_events = [ev for ev in all_events if ev not in used]
    if other_events:
        grouped.append(other_events)
    return grouped

def get_system_info():
    """Gathers system hardware and software information."""
    info = {"timestamp": datetime.datetime.now().isoformat()}
    try:
        # CPU Info
        cpu_info_raw = subprocess.check_output(
            "lscpu", text=True, stderr=subprocess.DEVNULL).strip()
        cpu_model = re.search(r"Model name:\s*(.*)", cpu_info_raw)
        cpu_cores = re.search(r"^CPU\(s\):\s*(\d+)",
                              cpu_info_raw, re.MULTILINE)
        info["cpu_model"] = cpu_model.group(1).strip() if cpu_model else "N/A"
        info["cpu_cores"] = int(cpu_cores.group(1)) if cpu_cores else "N/A"
    except Exception as e:
        print(f"Could not get CPU info: {e}")
        info["cpu_model"], info["cpu_cores"] = "N/A", "N/A"

    try:
        # Memory Info (Total in MB)
        mem_info_raw = subprocess.check_output(
            "free -m", text=True, shell=True, stderr=subprocess.DEVNULL).strip()
        # First number after "Mem:"
        mem_total = re.search(r"Mem:\s*(\d+)", mem_info_raw)
        info["memory_total_mb"] = int(
            mem_total.group(1)) if mem_total else "N/A"
    except Exception as e:
        print(f"Could not get Memory info: {e}")
        info["memory_total_mb"] = "N/A"

    try:
        # Clang version - attempt to get from PATH (set by 'enable' script)
        clang_version_raw = subprocess.check_output(
            "clang --version", text=True, shell=True, stderr=subprocess.DEVNULL).strip()
        info["clang_version"] = clang_version_raw.splitlines()[
            0] if clang_version_raw else "N/A"
    except Exception as e:
        print(
            f"Could not get clang version (ensure it's in PATH from 'enable' script): {e}")
        info["clang_version"] = "N/A"

    info["os_platform"] = platform.platform()
    info["os_uname"] = " ".join(platform.uname())
    return info


def parse_arts_cfg(cfg_file_path):
    """Parses an arts.cfg file to extract threads and nodeCount."""
    config = {"threads": None, "nodeCount": None}
    if not os.path.exists(cfg_file_path):
        return config
    try:
        with open(cfg_file_path, 'r') as f:
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
        print(f"Error parsing {cfg_file_path}: {e}")
    return config


def run_make_command(make_command, cwd_path):
    """Runs a make command in the specified directory."""
    print(f"[carts-test]  Running 'make {make_command}' in {cwd_path}...")
    try:
        process = subprocess.run(
            ["make", make_command], cwd=cwd_path, check=True, text=True, capture_output=True
        )
        print(
            f"[carts-test]  'make {make_command}' successful in {cwd_path}.")
        return True
    except subprocess.CalledProcessError as e:
        print(
            f"[carts-test]  Error during 'make {make_command}' in {cwd_path}: {e.stderr}")
        return False
    except FileNotFoundError:
        print(f"[carts-test]  Error: 'make' command not found.")
        return False
    return True


def find_example_directories_with_config(base_example_paths):
    """Finds subdirectories in the given base example paths that contain a test_config.yaml."""
    example_dirs = []
    for base_path in base_example_paths:
        if not os.path.isdir(base_path):
            continue  # Silently ignore non-existent base paths
        found_any = False
        for item in os.listdir(base_path):
            full_item_path = os.path.join(base_path, item)
            if os.path.isdir(full_item_path):
                # Ignore any path that is inside a 'benchmarks' directory
                if 'benchmarks' in full_item_path.split(os.sep):
                    continue
                config_path = os.path.join(full_item_path, 'test_config.yaml')
                if os.path.isfile(config_path):
                    example_dirs.append(full_item_path)
                    found_any = True
        # If no subdir with yaml, just skip (no warning)
    return example_dirs


def load_test_config(example_dir):
    config_path = os.path.join(example_dir, 'test_config.yaml')
    if not os.path.isfile(config_path):
        return None
    with open(config_path, 'r') as f:
        config = yaml.safe_load(f)
    # Support both old and new config formats
    if 'benchmark' in config or 'profiling' in config:
        return config
    # If old format, wrap in 'benchmark'
    return {'benchmark': config}


def run_program_instance(executable_path, program_args_str, num_iterations, example_name, version_name, timeout_seconds):
    """Runs a program instance, collects times, and correctness status. Only stores raw timing data."""
    times = []
    correctness_status = "UNKNOWN"
    timeout_occurred_in_any_iteration = False
    base_return = {
        "times_seconds": [],
        "iterations_ran": 0, "iterations_requested": num_iterations,
        "correctness_status": "NOT_RUN (Executable Missing)"
    }
    if not os.path.exists(executable_path):
        print(f"Error: Executable {executable_path} not found. Skipping.")
        return base_return
    base_return["correctness_status"] = "UNKNOWN"
    program_arg_list = program_args_str.split()
    print(
        f"Running {version_name} ('{executable_path}') for '{example_name}', args=[{program_args_str}], iters={num_iterations}...")
    for i in range(num_iterations):
        try:
            process = subprocess.run(
                [executable_path] + program_arg_list, capture_output=True, text=True, check=True, cwd=os.path.dirname(executable_path),
                timeout=timeout_seconds
            )
            output = process.stdout
            time_match = re.search(
                r"Finished in\s+([0-9]*\.?[0-9]+)\s*seconds?", output, re.IGNORECASE)
            if time_match:
                current_time = float(time_match.group(1))
                times.append(current_time)
                print(f"  Iter {i+1}: {current_time:.6f}s")
            else:
                output_snippet = output[:200].replace('\n', ' ')
                print(
                    f"  Iter {i+1}: Time not parsed. Output snippet: {output_snippet}...")
                times.append(None)
            correctness_match = re.search(
                r"Result: (CORRECT|INCORRECT)", output)
            if correctness_match:
                correctness_status = correctness_match.group(1)
        except subprocess.TimeoutExpired:
            print(
                f"  Iter {i+1}: TIMEOUT after {timeout_seconds}s for {executable_path}.")
            times.append(None)
            timeout_occurred_in_any_iteration = True
        except subprocess.CalledProcessError as e:
            print(f"Error running {executable_path} (iter {i+1}): {e.stderr}")
            times.append(None)
        except FileNotFoundError:
            print(
                f"Error: Executable {executable_path} not found during run iteration.")
            base_return["correctness_status"] = "NOT_RUN (Executable Missing During Iteration)"
            return base_return
    valid_times = [t for t in times if t is not None and isinstance(t, (float, int))]
    base_return["iterations_ran"] = len(valid_times)
    base_return["correctness_status"] = correctness_status
    base_return["times_seconds"] = valid_times
    return base_return


def generate_csv_report(results_data, output_csv_file, system_info):
    """Generates a CSV report with detailed statistics and system info."""
    if not results_data:
        print("No data to generate CSV report.")
        return
    fieldnames = [
        "Run ID", "Example Name", "Problem Size", "Threads", "Version", "Iterations Requested", "Iterations Ran",
        "Correctness Status", "Min Time (s)", "Max Time (s)", "Average Time (s)",
        "Median Time (s)", "StdDev Time (s)", "Speedup (OMP/CARTS)",
        "Individual Times (s)", "CARTS Threads", "CARTS Nodes", "Overall Example Status"
    ]
    with open(output_csv_file, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for r_item in results_data:
            run_id = r_item.get("run_id", "")
            example_name = r_item.get("example_name", "N/A")
            problem_size = r_item.get("problem_size", "N/A")
            threads_val = r_item.get("threads", "")
            overall_example_status = r_item.get("status", "processed")
            carts_config = r_item.get("carts_config", {})
            carts_threads_val = carts_config.get("threads", "")
            carts_nodes_val = carts_config.get("nodeCount", "")
            speedup_val = r_item.get("speedup_carts_vs_omp")
            if isinstance(speedup_val, float):
                speedup_str = f"{speedup_val:.2f}x"
            elif speedup_val is not None and speedup_val != "":
                speedup_str = str(speedup_val)
            else:
                speedup_str = "N/A"
            versions_to_process = [
                ("carts", "CARTS"), ("omp", "OpenMP")]
            for version_key, version_label in versions_to_process:
                perf_data = r_item.get(version_key, {})
                row_data = {
                    "Run ID": run_id,
                    "Example Name": example_name,
                    "Problem Size": problem_size,
                    "Threads": threads_val,
                    "Version": version_label,
                    "Iterations Requested": perf_data.get('iterations_requested', r_item.get('iterations_for_this_size', "N/A")),
                    "Iterations Ran": perf_data.get('iterations_ran', 0),
                    "Correctness Status": perf_data.get('correctness_status', "UNKNOWN"),
                    "Min Time (s)": safe_fmt(perf_data.get('min_seconds')),
                    "Max Time (s)": safe_fmt(perf_data.get('max_seconds')),
                    "Average Time (s)": safe_fmt(perf_data.get('avg_seconds')),
                    "Median Time (s)": safe_fmt(perf_data.get('median_seconds')),
                    "StdDev Time (s)": safe_fmt(perf_data.get('stdev_seconds')),
                    "Individual Times (s)": ", ".join([safe_fmt(t) for t in perf_data.get("times_seconds", [])]),
                    "Speedup (OMP/CARTS)": speedup_str if version_key == "carts" else "",
                    "CARTS Threads": carts_threads_val if version_key == "carts" else "",
                    "CARTS Nodes": carts_nodes_val if version_key == "carts" else "",
                    "Overall Example Status": overall_example_status if perf_data.get("times_seconds") else r_item.get(f"{version_key}_status", "run failed or no data")
                }
                writer.writerow(row_data)
    print(f"CSV report saved to {output_csv_file}")


def compute_aggregate_statistics(results_data):
    speedups = []
    carts_wins = 0
    omp_wins = 0
    ties = 0
    correctness_counts = {"CORRECT": 0, "INCORRECT": 0, "UNKNOWN": 0, "TIMEOUT": 0, "NO_VALID_TIMES": 0, "OTHER": 0}
    max_speedup = (None, None)
    min_speedup = (None, None)
    for r in results_data:
        s = r.get("speedup_carts_vs_omp")
        if isinstance(s, (float, int)):
            speedups.append(s)
            if s > 1.05:
                carts_wins += 1
            elif s < 0.95:
                omp_wins += 1
            else:
                ties += 1
            if max_speedup[0] is None or s > max_speedup[0]:
                max_speedup = (s, r.get("example_name"))
            if min_speedup[0] is None or s < min_speedup[0]:
                min_speedup = (s, r.get("example_name"))
        for v in ["carts", "omp"]:
            status = r.get(v, {}).get("correctness_status", "OTHER")
            if status in correctness_counts:
                correctness_counts[status] += 1
            else:
                correctness_counts["OTHER"] += 1
    geom_mean = statistics.geometric_mean(speedups) if speedups else None
    return {
        "geometric_mean_speedup": geom_mean,
        "carts_wins": carts_wins,
        "omp_wins": omp_wins,
        "ties": ties,
        "max_speedup": max_speedup,
        "min_speedup": min_speedup,
        "correctness_counts": correctness_counts,
        "speedups": speedups
    }

def collect_failure_table(results_data):
    failures = []
    for r in results_data:
        for version in ["carts", "omp"]:
            perf = r.get(version, {})
            status = perf.get("correctness_status", "UNKNOWN")
            if status not in ["CORRECT", "OK"]:
                failures.append({
                    "example": r.get("example_name"),
                    "problem_size": r.get("problem_size"),
                    "version": version.upper(),
                    "status": status,
                    "error": perf.get("error_message", "")
                })
    return failures


def get_reproducibility_info():
    info = {}
    # Python version
    info['python_version'] = sys.version.replace('\n', ' ')
    # pip freeze
    try:
        pip_freeze = subprocess.check_output([sys.executable, '-m', 'pip', 'freeze'], text=True)
        info['pip_freeze'] = pip_freeze.strip().split('\n')
    except Exception as e:
        info['pip_freeze'] = [f"Error: {e}"]
    # Environment variables (filtered for brevity)
    info['env'] = {k: v for k, v in os.environ.items() if k.startswith('OMP') or k.startswith('CARTS') or k.startswith('PATH') or k.startswith('LD_')}
    # Git commit hash
    try:
        git_hash = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True, stderr=subprocess.DEVNULL).strip()
        info['git_commit'] = git_hash
    except Exception:
        info['git_commit'] = 'N/A'
    return info


def get_git_commit(path):
    try:
        return subprocess.check_output(['git', '-C', path, 'rev-parse', 'HEAD'], text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return 'N/A'


def get_dependency_versions():
    deps = {
        'arts': 'external/arts',
        'polygeist': 'external/Polygeist',
        'polygeist/llvm': 'external/Polygeist/llvm-project/llvm',
    }
    versions = {}
    for name, path in deps.items():
        versions[name] = get_git_commit(path)
    return versions


def get_next_available_dir(base_dir):
    """Finds the next available directory name by appending _1, _2, etc. if needed."""
    if not os.path.exists(base_dir) or (os.path.isdir(base_dir) and not os.listdir(base_dir)):
        return base_dir
    i = 1
    while True:
        candidate = f"{base_dir}_{i}"
        if not os.path.exists(candidate):
            return candidate
        i += 1


def run_all_benchmarks(example_directories, args, system_info):
    run_counter = 1
    all_benchmark_results = []  # Only timing/correctness (no_profile)
    all_profiling_results = []  # Only event_stats and timing from perf (profile)
    for example_dir in example_directories:
        example_name = os.path.basename(example_dir)
        print(f"\nProcessing example: {example_name} in {example_dir}")
        test_config = load_test_config(example_dir)
        if not test_config:
            print(f"No test_config.yaml found for {example_name}, skipping.")
            continue
        arts_cfg_path = os.path.join(example_dir, "arts.cfg")
        current_arts_config = parse_arts_cfg(arts_cfg_path)
        bench_cfg = test_config.get('benchmark', {})
        profiling_cfg = test_config.get('profiling', {})
        profiling_enabled = profiling_cfg.get('profiling_enabled', False)
        problem_sizes = bench_cfg.get("problem_sizes", [])
        iterations_list = bench_cfg.get("iterations", [])
        args_list = bench_cfg.get("args", [""] * len(problem_sizes))
        threads_list = bench_cfg.get("threads", [1])
        if not (problem_sizes and iterations_list and len(problem_sizes) == len(iterations_list)):
            print(f"Invalid or missing problem_sizes/iterations in test_config.yaml for {example_name}, skipping.")
            continue
        run_plan = list(zip(problem_sizes, iterations_list, args_list))
        perf_event_groups = get_grouped_perf_events(profiling_cfg, args) if profiling_enabled else None
        for threads in threads_list:
            update_arts_cfg_threads(arts_cfg_path, threads)
            if not run_make_command("clean", example_dir):
                print(f"'make clean' failed for {example_name} (threads={threads}). Skipping all problem sizes for this example.")
                continue
            if not run_make_command("all", example_dir):
                print(f"'make all' failed for {example_name} (threads={threads}). Skipping all problem sizes for this example.")
                continue
            for problem_config_str, num_iterations_for_size, arg_str in run_plan:
                for version, exe_name in [("CARTS", example_name), ("OMP", f"{example_name}_omp")]:
                    # 1. Run without profiling (benchmark)
                    no_profile = run_program_instance(
                        os.path.join(example_dir, exe_name),
                        problem_config_str + (f" {arg_str}" if arg_str else ""),
                        num_iterations_for_size, example_name, version, args.timeout_seconds)
                    result_no_profile = {
                        "run_id": run_counter,
                        "example_name": example_name,
                        "problem_size": problem_config_str,
                        "iterations_for_this_size": num_iterations_for_size,
                        "carts_config": current_arts_config,
                        "threads": threads,
                        "version": version,
                        "run_type": "no_profile",
                        "times_seconds": no_profile.get("times_seconds"),
                        "iterations_ran": no_profile.get("iterations_ran"),
                        "iterations_requested": no_profile.get("iterations_requested"),
                        "correctness_status": no_profile.get("correctness_status"),
                        # No event_stats in benchmark results
                    }
                    all_benchmark_results.append(result_no_profile)
                    run_counter += 1
                    # 2. Run with profiling (perf)
                    if profiling_enabled and perf_event_groups:
                        profile = run_perf_instance(
                            os.path.join(example_dir, exe_name),
                            problem_config_str + (f" {arg_str}" if arg_str else ""),
                            num_iterations_for_size, example_name, version, args.timeout_seconds, perf_event_groups)
                        result_profile = {
                            "run_id": run_counter,
                            "example_name": example_name,
                            "problem_size": problem_config_str,
                            "iterations_for_this_size": num_iterations_for_size,
                            "carts_config": current_arts_config,
                            "threads": threads,
                            "version": version,
                            "run_type": "profile",
                            "times_seconds": profile.get("times_seconds"),  # timing as measured by perf
                            "iterations_ran": profile.get("iterations_ran"),
                            "iterations_requested": profile.get("iterations_requested"),
                            "correctness_status": profile.get("correctness_status"),
                            "event_stats": profile.get("event_stats"),  # Only event_stats in profiling results
                        }
                        all_profiling_results.append(result_profile)
                        run_counter += 1
    return all_benchmark_results, all_profiling_results


def update_arts_cfg_threads(cfg_path, threads):
    # Update or add the 'threads' entry in arts.cfg
    lines = []
    found = False
    if os.path.exists(cfg_path):
        with open(cfg_path, 'r') as f:
            for line in f:
                if line.strip().startswith('threads'):
                    lines.append(f"threads = {threads}\n")
                    found = True
                else:
                    lines.append(line)
    if not found:
        lines.append(f"threads = {threads}\n")
    with open(cfg_path, 'w') as f:
        f.writelines(lines)


def run_perf_instance(executable_path, program_args_str, num_iterations, example_name, version_name, timeout_seconds, perf_event_groups):
    """Runs a program instance under perf, collects hardware counters and timing. Only stores raw event/timing data."""
    perf_data = {}
    times = []
    correctness_status = "UNKNOWN"
    base_return = {
        "event_stats": {},
        "iterations_ran": 0,
        "iterations_requested": num_iterations,
        "correctness_status": "NOT_RUN (Executable Missing)",
        "times_seconds": []
    }
    if not os.path.exists(executable_path):
        print(f"Error: Executable {executable_path} not found. Skipping perf run.")
        return base_return
    program_arg_list = program_args_str.split()
    for i in range(num_iterations):
        for group_events in perf_event_groups:
            if not group_events:
                continue
            try:
                perf_cmd = [
                    "perf", "stat", "-a", "-x", ",", "-e", ','.join(group_events), "--", executable_path
                ] + program_arg_list
                process = subprocess.run(
                    perf_cmd, capture_output=True, text=True, check=True, cwd=os.path.dirname(executable_path),
                    timeout=timeout_seconds
                )
                output = process.stdout + process.stderr
                print("=========================")
                print(f"[iter {i+1}, events {group_events}]")
                print(output)
                print("=========================")
                # Parse timing from output (only once per iteration)
                if group_events == perf_event_groups[0]:
                    time_match = re.search(r"Finished in\s+([0-9]*\.?[0-9]+)\s*seconds?", output, re.IGNORECASE)
                    if time_match:
                        current_time = float(time_match.group(1))
                        times.append(current_time)
                        print(f"  Iter {i+1}: {current_time:.6f}s (from perf)")
                    else:
                        output_snippet = output[:200].replace('\n', ' ')
                        print(f"  Iter {i+1}: Time not parsed. Output snippet: {output_snippet}...")
                        times.append(None)
                # Parse correctness if present (only once per iteration)
                if group_events == perf_event_groups[0]:
                    correctness_match = re.search(r"Result: (CORRECT|INCORRECT)", output)
                    if correctness_match:
                        correctness_status = correctness_match.group(1)
                # Parse perf output (CSV format: value,event,...)
                for line in output.splitlines():
                    parts = line.strip().split(",")
                    if len(parts) >= 3:
                        try:
                            val = float(parts[0].replace(',', '').replace('<not supported>', 'nan').replace('<not counted>', 'nan'))
                        except Exception:
                            val = float('nan')
                        event = parts[2].strip().split(":")[0] + (":" + parts[2].strip().split(":")[1] if ":" in parts[2] else "")
                        if event not in perf_data:
                            perf_data[event] = []
                        perf_data[event].append(val)
                        if i == 0:
                            print(f"[perf-parse] Iter {i+1} (events {group_events}): Event '{event}' Value {val}")
            except subprocess.TimeoutExpired:
                print(f"  Perf Iter {i+1} (events {group_events}): TIMEOUT after {timeout_seconds}s for {executable_path}.")
                for event in group_events:
                    if event not in perf_data:
                        perf_data[event] = []
                    perf_data[event].append(float('nan'))
                if group_events == perf_event_groups[0]:
                    times.append(None)
            except subprocess.CalledProcessError as e:
                print(f"Perf error running {executable_path} (iter {i+1}, events {group_events}): {e.stderr}")
                for event in group_events:
                    if event not in perf_data:
                        perf_data[event] = []
                    perf_data[event].append(float('nan'))
                if group_events == perf_event_groups[0]:
                    times.append(None)
            except FileNotFoundError:
                print(f"Perf: Executable {executable_path} not found during run iteration.")
                base_return["correctness_status"] = "NOT_RUN (Executable Missing During Iteration)"
                return base_return
    valid_times = [t for t in times if t is not None and isinstance(t, (float, int))]
    base_return["iterations_ran"] = len(valid_times)
    base_return["correctness_status"] = correctness_status
    base_return["times_seconds"] = valid_times
    # Only store raw event values per event
    for event in perf_data:
        vals = [v for v in perf_data[event] if isinstance(v, (float, int)) and not np.isnan(v)]
        base_return["event_stats"][event] = {"all": vals}
    return base_return


def aggregate_results_for_csv(benchmark_results, profiling_results):
    """
    Groups results by (example_name, problem_size, threads), computes statistics for each version, and calculates speedup.
    Also includes profiling event stats if available.
    Returns a list of dicts ready for CSV output.
    """
    from collections import defaultdict
    def compute_stats(times):
        if not times:
            return {k: None for k in ["min", "max", "avg", "median", "stdev"]}
        return {
            "min": min(times),
            "max": max(times),
            "avg": statistics.mean(times),
            "median": statistics.median(times),
            "stdev": statistics.stdev(times) if len(times) > 1 else 0.0,
        }
    # Group by (example_name, problem_size, threads)
    grouped = defaultdict(lambda: {"carts": None, "omp": None, "carts_profile": None, "omp_profile": None})
    all_events = set()
    for r in benchmark_results:
        key = (r["example_name"], r["problem_size"], r.get("threads", 1))
        if r["version"].upper() == "CARTS":
            grouped[key]["carts"] = r
        elif r["version"].upper() == "OMP":
            grouped[key]["omp"] = r
    for r in profiling_results:
        key = (r["example_name"], r["problem_size"], r.get("threads", 1))
        if r["version"].upper() == "CARTS":
            grouped[key]["carts_profile"] = r
        elif r["version"].upper() == "OMP":
            grouped[key]["omp_profile"] = r
        # Collect all event names
        event_stats = r.get("event_stats", {})
        all_events.update(event_stats.keys())
    all_events = sorted(all_events)
    # Build output rows
    output_rows = []
    for (example_name, problem_size, threads), versions in grouped.items():
        row = {
            "Example Name": example_name,
            "Problem Size": problem_size,
            "Threads": threads,
        }
        # For each version, compute stats
        for vkey, label in [("carts", "CARTS"), ("omp", "OpenMP")]:
            v = versions[vkey]
            if v:
                times = v.get("times_seconds", [])
                stats = compute_stats(times)
                row[f"{label} Min Time (s)"] = safe_fmt(stats["min"])
                row[f"{label} Max Time (s)"] = safe_fmt(stats["max"])
                row[f"{label} Avg Time (s)"] = safe_fmt(stats["avg"])
                row[f"{label} Median Time (s)"] = safe_fmt(stats["median"])
                row[f"{label} StdDev Time (s)"] = safe_fmt(stats["stdev"])
                row[f"{label} Times (s)"] = ", ".join([safe_fmt(t) for t in times])
                row[f"{label} Correctness Status"] = v.get("correctness_status", "UNKNOWN")
                row[f"{label} Iterations Ran"] = v.get("iterations_ran", 0)
                row[f"{label} Iterations Requested"] = v.get("iterations_requested", 0)
            else:
                for stat in ["Min", "Max", "Avg", "Median", "StdDev", "Times", "Correctness Status", "Iterations Ran", "Iterations Requested"]:
                    row[f"{label} {stat} (s)"] = "N/A"
        # For each profiling version, add event stats
        for vkey, label in [("carts_profile", "CARTS"), ("omp_profile", "OpenMP")]:
            v = versions[vkey]
            event_stats = v.get("event_stats", {}) if v else {}
            for event in all_events:
                vals = event_stats.get(event, {}).get("all", [])
                stats = compute_stats(vals)
                for stat in ["min", "max", "avg", "median", "stdev"]:
                    row[f"{label} {event} {stat}"] = safe_fmt(stats[stat])
        # Speedup (OMP/CARTS)
        try:
            carts_median = float(row["CARTS Median Time (s)"])
            omp_median = float(row["OpenMP Median Time (s)"])
            if carts_median > 0 and omp_median > 0:
                speedup = omp_median / carts_median
                row["Speedup (OMP/CARTS)"] = f"{speedup:.2f}x"
            else:
                row["Speedup (OMP/CARTS)"] = "N/A"
        except Exception:
            row["Speedup (OMP/CARTS)"] = "N/A"
        output_rows.append(row)
    return output_rows


def main():
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")
    print("CARTS Performance Comparison Script")
    parser = argparse.ArgumentParser(
        description="Run and compare performance of example programs across various problem sizes.",
        add_help=True)
    parser.add_argument("--timeout_seconds", type=int, default=300,
                        help="Timeout in seconds for each individual program run (default: 300s).")
    parser.add_argument("--output_prefix", type=str, default="performance",
                        help="Prefix for all output files (e.g., 'my_run' will generate 'my_run_results.json', 'my_run_graph.png', etc.). Default: 'performance'")
    parser.add_argument("--example_base_dirs", nargs='+',
                        default=["examples/parallel", "examples/tasking"], help="Base directories for examples.")
    parser.add_argument("--target_examples", type=str, nargs='+', default=None,
                        help="Optional: List of specific example names to run (e.g., addition dotproduct). If not provided, all found examples are run.")
    parser.add_argument('--clear_output', type=bool, default=False,
                       help='Clear the output directory before running (default: True). Set to False to keep previous results.')
    parser.add_argument('--perf_events', nargs='+', default=None,
                       help='List of perf events to collect (overrides YAML and default).')
    args = parser.parse_args()
    system_info = get_system_info()
    original_cwd = os.getcwd()
    base_output_dir = os.path.join(original_cwd, "output")
    if args.clear_output:
        output_dir = base_output_dir
        os.makedirs(output_dir, exist_ok=True)
        clear_output_dir(output_dir)
    else:
        output_dir = get_next_available_dir(base_output_dir)
        os.makedirs(output_dir, exist_ok=True)
    print(f"Output will be saved to directory: {output_dir}")
    output_json_path = os.path.join(output_dir, f"{args.output_prefix}_results.json")
    csv_file_path = os.path.join(output_dir, f"{args.output_prefix}_report.csv")
    graph_file_path = os.path.join(output_dir, f"{args.output_prefix}_comparison.png")
    speedup_graph_file_path = os.path.join(output_dir, f"{args.output_prefix}_speedup.png")
    absolute_example_base_dirs = [os.path.join(original_cwd, path) for path in args.example_base_dirs]
    example_directories = find_example_directories_with_config(absolute_example_base_dirs)
    if not example_directories:
        print("No example directories with test_config.yaml found.")
        return
    print(f"Found {len(example_directories)} example directories: {[os.path.basename(d) for d in example_directories]}")
    if args.target_examples:
        original_found_count = len(example_directories)
        filtered_example_dirs = [d for d in example_directories if os.path.basename(d) in args.target_examples]
        if not filtered_example_dirs:
            print(f"Error: None of the target examples ({', '.join(args.target_examples)}) were found among the {original_found_count} discovered example directories.")
            print(f"Searched in: {absolute_example_base_dirs}")
            discovered_names = [os.path.basename(d) for d in example_directories]
            for target in args.target_examples:
                if target not in discovered_names:
                    print(f"  - Target '{target}' was not found in the discovered list: {discovered_names}")
            return
        example_directories = filtered_example_dirs
        print(f"Filtered to run only the specified examples: {args.target_examples} -> Found: {[os.path.basename(d) for d in example_directories]}")
    else:
        print("No specific target examples provided, running all found examples.")
    all_benchmark_results, all_profiling_results = run_all_benchmarks(example_directories, args, system_info)
    aggregate_stats = compute_aggregate_statistics(all_benchmark_results + all_profiling_results)
    dep_versions = get_dependency_versions()
    reproducibility = get_reproducibility_info()
    failures = collect_failure_table(all_benchmark_results + all_profiling_results)
    # Generate all reports/plots as before
    with open(output_json_path, "w") as f:
        # Compose the new JSON structure
        final_json_output = {
            "system_information": system_info,
            "benchmark_results": all_benchmark_results,  # Only timing/correctness
            "profiling_results": all_profiling_results,  # Only event_stats and perf timing
            "aggregate_statistics": aggregate_stats,
            "dependency_versions": dep_versions,
            "reproducibility": reproducibility,
            "failures": failures
        }
        json.dump(final_json_output, f, indent=2)

    # --- Aggregated CSV output ---
    csv_rows = aggregate_results_for_csv(all_benchmark_results, all_profiling_results)
    if csv_rows:
        fieldnames = list(csv_rows[0].keys())
        with open(csv_file_path, 'w', newline='') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            for row in csv_rows:
                writer.writerow(row)
        print(f"Aggregated CSV report saved to {csv_file_path}")
    else:
        print("No data to write to CSV.")

    print("\nDumping Comparison Results (JSON)")
    json.dumps(final_json_output, indent=2)
    print("Script finished.")
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")


def clear_output_dir(output_dir):
    for item in os.listdir(output_dir):
        if item == '.gitignore':
            continue
        item_path = os.path.join(output_dir, item)
        if os.path.isdir(item_path):
            shutil.rmtree(item_path)
        else:
            os.remove(item_path)


if __name__ == "__main__":
    main()
