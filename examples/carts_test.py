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
matplotlib.use('Agg')
sns.set_theme(style="whitegrid")
CUSTOM_COLORS = ["#1f77b4", "#ff7f0e"]  # blue, orange


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
            print(f"Warning: Base example path '{base_path}' not found.")
            continue
        for item in os.listdir(base_path):
            full_item_path = os.path.join(base_path, item)
            if os.path.isdir(full_item_path):
                # Ignore any path that is inside a 'benchmarks' directory
                if 'benchmarks' in full_item_path.split(os.sep):
                    continue
                config_path = os.path.join(full_item_path, 'test_config.yaml')
                if os.path.isfile(config_path):
                    example_dirs.append(full_item_path)
    return example_dirs


def load_test_config(example_dir):
    config_path = os.path.join(example_dir, 'test_config.yaml')
    if not os.path.isfile(config_path):
        return None
    with open(config_path, 'r') as f:
        return yaml.safe_load(f)


def run_program_instance(executable_path, program_args_str, num_iterations, example_name, version_name, timeout_seconds):
    """Runs a program instance, collects times, and correctness status."""
    times = []
    correctness_status = "UNKNOWN"
    timeout_occurred_in_any_iteration = False
    base_return = {
        "times_seconds": [],
        "min_seconds": None, "max_seconds": None, "avg_seconds": None,
        "median_seconds": None, "stdev_seconds": None,
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

            # Refined regex: looks for "finished in", captures float, then "seconds", case insensitive
            time_match = re.search(
                r"finished in\s+([0-9]*\.?[0-9]+)\s*seconds?", output, re.IGNORECASE)
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
        except FileNotFoundError:  # Should be caught by the check above, but as a safeguard
            print(
                f"Error: Executable {executable_path} not found during run iteration.")
            base_return["correctness_status"] = "NOT_RUN (Executable Missing During Iteration)"
            return base_return

    valid_times = [
        t for t in times if t is not None and isinstance(t, (float, int))]
    base_return["iterations_ran"] = len(valid_times)
    base_return["correctness_status"] = correctness_status

    if not valid_times:
        print(f"No valid execution times collected for {executable_path}.")
        if timeout_occurred_in_any_iteration:
            base_return["correctness_status"] = "TIMEOUT"
        elif base_return["correctness_status"] == "UNKNOWN":
            base_return["correctness_status"] = "NO_VALID_TIMES"
        return base_return

    base_return.update({
        "times_seconds": valid_times,
        "min_seconds": min(valid_times),
        "max_seconds": max(valid_times),
        "avg_seconds": statistics.mean(valid_times),
        "median_seconds": statistics.median(valid_times),
        "stdev_seconds": statistics.stdev(valid_times) if len(valid_times) > 1 else 0.0,
    })
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
                    "Min Time (s)": f"{perf_data.get('min_seconds'):.5f}" if perf_data.get('min_seconds') is not None else "",
                    "Max Time (s)": f"{perf_data.get('max_seconds'):.5f}" if perf_data.get('max_seconds') is not None else "",
                    "Average Time (s)": f"{perf_data.get('avg_seconds'):.5f}" if perf_data.get('avg_seconds') is not None else "",
                    "Median Time (s)": f"{perf_data.get('median_seconds'):.5f}" if perf_data.get('median_seconds') is not None else "",
                    "StdDev Time (s)": f"{perf_data.get('stdev_seconds'):.5f}" if perf_data.get('stdev_seconds') is not None else "",
                    "Individual Times (s)": ", ".join([f"{t:.5f}" for t in perf_data.get("times_seconds", [])]),
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


def generate_comparison_graph(results_data, output_image_file, output_pdf_file, threads=None):
    """Generates a bar chart with error bars and best result annotation, saves as PNG and PDF."""
    if not results_data:
        print("No data to generate graph.")
        return
    x_tick_labels = [
        f"{r['example_name']}\n(Size: {r['problem_size']})" for r in results_data]
    carts_avg_times = [r.get('carts', {}).get('avg_seconds') if r.get(
        'carts', {}).get('avg_seconds') is not None else 0 for r in results_data]
    omp_avg_times = [r.get('omp', {}).get('avg_seconds') if r.get(
        'omp', {}).get('avg_seconds') is not None else 0 for r in results_data]
    carts_std = [r.get('carts', {}).get('stdev_seconds') or 0 for r in results_data]
    omp_std = [r.get('omp', {}).get('stdev_seconds') or 0 for r in results_data]
    x = np.arange(len(x_tick_labels))
    width = 0.35
    fig_width = max(12, len(x_tick_labels) * 1.5)
    fig_height = 10
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))
    rects_carts = ax.bar(x - width/2, carts_avg_times, width, yerr=carts_std, label='CARTS',
                         color=CUSTOM_COLORS[0], capsize=5, alpha=0.9)
    rects_omp = ax.bar(x + width/2, omp_avg_times, width, yerr=omp_std, label='OpenMP',
                       color=CUSTOM_COLORS[1], capsize=5, alpha=0.9)
    ax.set_ylabel('Execution Time (s)', fontsize=16)
    ax.set_xlabel('Example & Problem Size', fontsize=16)
    title = 'Performance Comparison: CARTS vs OpenMP'
    if threads is not None:
        title += f' (Threads: {threads})'
    ax.set_title(title, fontsize=18)
    ax.set_xticks(x)
    ax.set_xticklabels(x_tick_labels, rotation=45, ha="right", fontsize=12)
    ax.legend(title="Version", fontsize=14, title_fontsize=15, loc='upper right')
    ax.grid(axis='y', linestyle='--', alpha=0.7)
    for i, (c, o) in enumerate(zip(carts_avg_times, omp_avg_times)):
        if c and (not o or c < o):
            ax.annotate("★", xy=(x[i] - width/2, c), xytext=(0, -20), textcoords="offset points", ha='center', va='bottom', fontsize=18, color='gold')
        elif o:
            ax.annotate("★", xy=(x[i] + width/2, o), xytext=(0, -20), textcoords="offset points", ha='center', va='bottom', fontsize=18, color='gold')
    plt.tight_layout(rect=[0, 0.05, 1, 1])
    try:
        plt.savefig(output_image_file, bbox_inches='tight', dpi=300)
        plt.savefig(output_pdf_file, bbox_inches='tight')
        print(f"Comparison graph saved to {output_image_file} and {output_pdf_file}.")
    except Exception as e:
        print(f"Error saving graph: {e}")
    plt.close(fig)


def generate_speedup_distribution_plot(speedups, output_image_file):
    if not speedups:
        print("No speedup data for distribution plot.")
        return
    fig, ax = plt.subplots(figsize=(8, 6))
    sns.histplot(speedups, bins=10, kde=True, ax=ax, color=sns.color_palette()[2])
    ax.set_title('Distribution of Speedups (OMP/CARTS)', fontsize=16)
    ax.set_xlabel('Speedup (OMP Time / CARTS Time)', fontsize=14)
    ax.set_ylabel('Count', fontsize=14)
    plt.tight_layout()
    try:
        plt.savefig(output_image_file, bbox_inches='tight', dpi=300)
        plt.savefig(output_image_file.replace('.png', '.pdf'), bbox_inches='tight')
        # print(f"Speedup distribution plot saved to {output_image_file} and PDF.")
    except Exception as e:
        print(f"Error saving speedup distribution plot: {e}")
    plt.close(fig)


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


def generate_markdown_report(results_data, system_info, output_md_file, graph_image_file, csv_file_name, speedup_graph_file, aggregate_stats, speedup_dist_file, images_dir, pdfs_dir):
    if not results_data:
        print("No data to generate Markdown report.")
        return
    from collections import defaultdict
    example_groups = defaultdict(list)
    for r in results_data:
        example_groups[r['example_name']].append(r)
    dep_versions = get_dependency_versions()
    with open(output_md_file, 'w') as mdfile:
        mdfile.write(f"# Performance Comparison Report\n\n")
        mdfile.write(f"**Run Timestamp**: {system_info.get('timestamp', 'N/A')}\n\n")
        mdfile.write("## System Information\n")
        mdfile.write(f"- **OS Platform**: {system_info.get('os_platform', 'N/A')}\n")
        mdfile.write(f"- **OS Uname**: {system_info.get('os_uname', 'N/A')}\n")
        mdfile.write(f"- **CPU Model**: {system_info.get('cpu_model', 'N/A')}\n")
        mdfile.write(f"- **CPU Cores**: {system_info.get('cpu_cores', 'N/A')}\n")
        mdfile.write(f"- **Total Memory**: {system_info.get('memory_total_mb', 'N/A')} MB\n")
        mdfile.write(f"- **Clang Version**: `{system_info.get('clang_version', 'N/A')}`\n\n")
        mdfile.write("## Aggregate Statistics\n")
        mdfile.write(f"- **Geometric Mean Speedup (OMP/CARTS):** {aggregate_stats['geometric_mean_speedup']:.2f}x\n")
        mdfile.write(f"- **CARTS Wins:** {aggregate_stats['carts_wins']}\n")
        mdfile.write(f"- **OMP Wins:** {aggregate_stats['omp_wins']}\n")
        mdfile.write(f"- **Ties:** {aggregate_stats['ties']}\n")
        mdfile.write(f"- **Max Speedup:** {aggregate_stats['max_speedup'][0]:.2f}x ({aggregate_stats['max_speedup'][1]})\n")
        mdfile.write(f"- **Min Speedup:** {aggregate_stats['min_speedup'][0]:.2f}x ({aggregate_stats['min_speedup'][1]})\n")
        mdfile.write("\n## Correctness Summary\n")
        for k, v in aggregate_stats['correctness_counts'].items():
            mdfile.write(f"- **{k}:** {v}\n")
        mdfile.write("\n## Notable Results\n")
        mdfile.write(f"- **Largest Speedup:** {aggregate_stats['max_speedup'][0]:.2f}x ({aggregate_stats['max_speedup'][1]})\n")
        mdfile.write(f"- **Largest Slowdown:** {aggregate_stats['min_speedup'][0]:.2f}x ({aggregate_stats['min_speedup'][1]})\n")
        per_thread_stats = per_thread_aggregate_stats(results_data)
        if per_thread_stats:
            mdfile.write("\n## Per-Thread Results\n")
            for threads in sorted(per_thread_stats.keys()):
                stats = per_thread_stats[threads]
                mdfile.write(f"\n### Threads = {threads}\n")
                mdfile.write(f"**Analysis:** ")
                if stats['geometric_mean_speedup'] > 1.05:
                    mdfile.write(f"CARTS is generally faster than OpenMP for {threads} threads (geometric mean speedup: {stats['geometric_mean_speedup']:.2f}x). ")
                elif stats['geometric_mean_speedup'] < 0.95:
                    mdfile.write(f"OpenMP is generally faster than CARTS for {threads} threads (geometric mean speedup: {stats['geometric_mean_speedup']:.2f}x). ")
                else:
                    mdfile.write(f"Performance is similar between CARTS and OpenMP for {threads} threads. ")
                mdfile.write(f"CARTS wins: {stats['carts_wins']}, OMP wins: {stats['omp_wins']}, ties: {stats['ties']}. ")
                mdfile.write(f"Max speedup: {stats['max_speedup'][0]:.2f}x ({stats['max_speedup'][1]}), min speedup: {stats['min_speedup'][0]:.2f}x ({stats['min_speedup'][1]}).\n\n")
                mdfile.write("| Run ID | Example | Problem Size | CARTS Avg Time (s) | OMP Avg Time (s) | CARTS Correctness | OMP Correctness | Speedup | Build Status |\n")
                mdfile.write("|---|---|---|---|---|---|---|---|---|\n")
                for r_item in results_data:
                    if r_item.get('threads') != threads:
                        continue
                    run_id = r_item.get("run_id", "")
                    carts_perf = r_item.get("carts", {})
                    omp_perf = r_item.get("omp", {})
                    carts_avg_seconds = carts_perf.get('avg_seconds')
                    omp_avg_seconds = omp_perf.get('avg_seconds')
                    carts_time_str = f"{carts_avg_seconds:.5f}" if carts_avg_seconds is not None else "0.00000"
                    omp_time_str = f"{omp_avg_seconds:.5f}" if omp_avg_seconds is not None else "0.00000"
                    carts_correct = carts_perf.get('correctness_status', 'UNKNOWN')
                    omp_correct = omp_perf.get('correctness_status', 'UNKNOWN')
                    speedup_val = r_item.get("speedup_carts_vs_omp")
                    speedup_str = f"{speedup_val:.2f}x" if isinstance(speedup_val, float) else (speedup_val if speedup_val else "N/A")
                    status = r_item.get("status", "processed")
                    mdfile.write(f"| {run_id} "
                                 f"| {r_item.get('example_name', 'N/A')} "
                                 f"| {r_item.get('problem_size', 'N/A')} "
                                 f"| {carts_time_str} "
                                 f"| {omp_time_str} "
                                 f"| {carts_correct} "
                                 f"| {omp_correct} "
                                 f"| {speedup_str} "
                                 f"| {status} |\n")
                # Per-thread images (smaller in MD)
                mdfile.write(f"\n- <img src='images/threads{threads}_comparison.png' width='400'> ([PDF](pdfs/threads{threads}_comparison.pdf))\n")
                mdfile.write(f"- <img src='images/threads{threads}_speedup.png' width='400'> ([PDF](pdfs/threads{threads}_speedup.pdf))\n")
                mdfile.write(f"- <img src='images/threads{threads}_scatter.png' width='400'> ([PDF](pdfs/threads{threads}_scatter.pdf))\n")
                mdfile.write(f"- <img src='images/threads{threads}_speedup_dist.png' width='400'> ([PDF](pdfs/threads{threads}_speedup_dist.pdf))\n")
        mdfile.write(f"\n## Per-Example Scaling Analysis\n")
        for example, runs in example_groups.items():
            mdfile.write(f"\n### Example: {example}\n")
            mdfile.write(f"**Scaling Analysis:** ")
            thread_counts = sorted(set(r['threads'] for r in runs if r.get('threads') is not None))
            if len(thread_counts) > 1:
                mdfile.write(f"Scaling, speedup, and efficiency are plotted below for {len(thread_counts)} thread counts. Look for linear speedup and high efficiency for ideal scaling.\n\n")
            else:
                mdfile.write(f"Only one thread count available; scaling analysis not possible.\n\n")
            mdfile.write(f"- <img src='images/scaling_{example}.png' width='400'> ([PDF](pdfs/scaling_{example}.pdf))\n")
            mdfile.write(f"- <img src='images/speedup_{example}.png' width='400'> ([PDF](pdfs/speedup_{example}.pdf))\n")
            mdfile.write(f"- <img src='images/efficiency_{example}.png' width='400'> ([PDF](pdfs/efficiency_{example}.pdf))\n")
        mdfile.write(f"\n## Detailed Data\nSee CSV: [{os.path.basename(csv_file_name)}]({os.path.basename(csv_file_name)})\n")
        failures = collect_failure_table(results_data)
        if failures:
            mdfile.write("\n## Failure Table\n")
            mdfile.write("| Example | Problem Size | Version | Status | Error Message |\n")
            mdfile.write("|---|---|---|---|---|\n")
            for f in failures:
                mdfile.write(f"| {f['example']} | {f['problem_size']} | {f['version']} | {f['status']} | {f['error']} |\n")
        reproducibility = get_reproducibility_info()
        mdfile.write("\n## Reproducibility\n")
        mdfile.write(f"- **Python Version:** {reproducibility['python_version']}\n")
        mdfile.write(f"- **Git Commit:** {reproducibility['git_commit']}\n")
        mdfile.write("- **Relevant Environment Variables:**\n")
        for k, v in reproducibility['env'].items():
            mdfile.write(f"    - {k}: {v}\n")
        mdfile.write("- **Python Packages:**\n")
        for pkg in reproducibility['pip_freeze']:
            mdfile.write(f"    - {pkg}\n")
        mdfile.write("\n## Dependency Versions\n")
        for dep, ver in dep_versions.items():
            mdfile.write(f"- **{dep}:** {ver}\n")


def generate_speedup_graph(results_data, output_image_file, threads=None):
    if not results_data:
        print("No data to generate speedup graph.")
        return
    valid_results = [
        r for r in results_data
        if r.get("speedup_carts_vs_omp") is not None and
        isinstance(r.get("speedup_carts_vs_omp"), (float, int)) and
        r.get("carts", {}).get("avg_seconds") is not None and r.get("carts", {}).get("avg_seconds") > 0 and
        r.get("omp", {}).get("avg_seconds") is not None
    ]
    if not valid_results:
        print("No valid data points with speedup information to generate speedup graph.")
        return
    x_tick_labels = [
        f"{r['example_name']}\n(Size: {r['problem_size']})" for r in valid_results
    ]
    speedup_values = [r["speedup_carts_vs_omp"] for r in valid_results]
    x = np.arange(len(x_tick_labels))
    fig_width = max(10, len(x_tick_labels) * 1.2)
    fig_height = 8
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))
    # Use the same color as the speedup distribution plot (pleasant blue/purple)
    speedup_color = sns.color_palette("deep")[0]
    rects_speedup = ax.bar(
        x, speedup_values, color=speedup_color, edgecolor='black', linewidth=1)
    ax.set_ylabel('Speedup (OMP Time / CARTS Time)')
    ax.set_xlabel('Example & Problem Size')
    title = 'Speedup: CARTS vs OpenMP'
    if threads is not None:
        title += f' (Threads: {threads})'
    ax.set_title(title)
    ax.set_xticks(x)
    ax.set_xticklabels(x_tick_labels, rotation=45, ha="right")
    ax.grid(axis='y', linestyle='--', alpha=0.7)
    ax.axhline(1, color='grey', lw=1, linestyle='--')
    for rect in rects_speedup:
        height = rect.get_height()
        ax.annotate(f'{height:.2f}x',
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 3),
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=9)
    plt.tight_layout(rect=[0, 0.05, 1, 0.95])
    try:
        plt.savefig(output_image_file, bbox_inches='tight')
        print(f"Speedup graph saved to {output_image_file}")
    except Exception as e:
        print(f"Error saving speedup graph: {e}")
    plt.close(fig)


def generate_scatter_plot(results_data, output_image_file):
    x = []  # OMP times
    y = []  # CARTS times
    labels = []
    for r in results_data:
        omp = r.get('omp', {}).get('avg_seconds')
        carts = r.get('carts', {}).get('avg_seconds')
        if omp is not None and carts is not None:
            x.append(omp)
            y.append(carts)
            labels.append(f"{r.get('example_name')}\n{r.get('problem_size')}")
    if not x or not y:
        print("No data for scatter plot.")
        return
    fig, ax = plt.subplots(figsize=(8, 8))
    sns.scatterplot(x=x, y=y, ax=ax, s=80, color=sns.color_palette()[3])
    ax.plot([min(x+y), max(x+y)], [min(x+y), max(x+y)], 'k--', lw=1, label='y=x')
    ax.set_xlabel('OMP Avg Time (s)', fontsize=14)
    ax.set_ylabel('CARTS Avg Time (s)', fontsize=14)
    ax.set_title('OMP vs CARTS Execution Time', fontsize=16)
    for i, label in enumerate(labels):
        ax.annotate(label, (x[i], y[i]), fontsize=8, alpha=0.7)
    ax.legend()
    plt.tight_layout()
    try:
        plt.savefig(output_image_file, bbox_inches='tight', dpi=300)
        plt.savefig(output_image_file.replace('.png', '.pdf'), bbox_inches='tight')
    except Exception as e:
        print(f"Error saving scatter plot: {e}")
    plt.close(fig)


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


def ensure_images_dir(output_dir):
    images_dir = os.path.join(output_dir, "images")
    os.makedirs(images_dir, exist_ok=True)
    return images_dir


def ensure_pdfs_dir(output_dir):
    pdfs_dir = os.path.join(output_dir, "pdfs")
    os.makedirs(pdfs_dir, exist_ok=True)
    return pdfs_dir


def per_thread_aggregate_stats(results_data):
    """Return a dict mapping thread count to aggregate stats for that thread count."""
    thread_stats = {}
    for threads in sorted(set(r['threads'] for r in results_data)):
        thread_results = [r for r in results_data if r['threads'] == threads]
        if not thread_results:
            continue
        thread_stats[threads] = compute_aggregate_statistics(thread_results)
    return thread_stats


def per_example_aggregate_stats(results_data):
    """Return a dict mapping example_name to aggregate stats for that example."""
    example_stats = {}
    for example in sorted(set(r['example_name'] for r in results_data)):
        example_results = [r for r in results_data if r['example_name'] == example]
        if not example_results:
            continue
        example_stats[example] = compute_aggregate_statistics(example_results)
    return example_stats


def run_all_benchmarks(example_directories, args, system_info):
    run_counter = 1
    all_run_results_data = []
    for example_dir in example_directories:
        example_name = os.path.basename(example_dir)
        print(f"\nProcessing example: {example_name} in {example_dir}")
        test_config = load_test_config(example_dir)
        if not test_config:
            print(f"No test_config.yaml found for {example_name}, skipping.")
            continue
        problem_sizes = test_config.get("problem_sizes", [])
        iterations_list = test_config.get("iterations", [])
        args_list = test_config.get("args", [""] * len(problem_sizes))
        threads_list = test_config.get("threads", [1])
        if not (problem_sizes and iterations_list and len(problem_sizes) == len(iterations_list)):
            print(f"Invalid or missing problem_sizes/iterations in test_config.yaml for {example_name}, skipping.")
            continue
        run_plan = list(zip(problem_sizes, iterations_list, args_list))
        arts_cfg_path = os.path.join(example_dir, "arts.cfg")
        current_arts_config = parse_arts_cfg(arts_cfg_path)
        for threads in threads_list:
            update_arts_cfg_threads(arts_cfg_path, threads)
            if not run_make_command("clean", example_dir):
                print(f"'make clean' failed for {example_name} (threads={threads}). Skipping all problem sizes for this example.")
                for problem_size, num_iterations_for_size, _ in run_plan:
                    all_run_results_data.append({
                        "run_id": run_counter,
                        "example_name": example_name, "problem_size": problem_size,
                        "iterations_for_this_size": num_iterations_for_size, "carts_config": current_arts_config,
                        "carts": {"correctness_status": "NOT_RUN (Make Clean Failed)", "iterations_requested": num_iterations_for_size},
                        "omp": {"correctness_status": "NOT_RUN (Make Clean Failed)", "iterations_requested": num_iterations_for_size},
                        "status": "make clean failed", "speedup_carts_vs_omp": None,
                        "threads": threads
                    })
                    run_counter += 1
                continue
            if not run_make_command("all", example_dir):
                print(f"'make all' failed for {example_name} (threads={threads}). Skipping all problem sizes for this example.")
                for problem_size, num_iterations_for_size, _ in run_plan:
                    all_run_results_data.append({
                        "run_id": run_counter,
                        "example_name": example_name, "problem_size": problem_size,
                        "iterations_for_this_size": num_iterations_for_size, "carts_config": current_arts_config,
                        "carts": {"correctness_status": "NOT_RUN (Make All Failed)", "iterations_requested": num_iterations_for_size},
                        "omp": {"correctness_status": "NOT_RUN (Make All Failed)", "iterations_requested": num_iterations_for_size},
                        "status": "make all failed", "speedup_carts_vs_omp": None,
                        "threads": threads
                    })
                    run_counter += 1
                continue
            for problem_config_str, num_iterations_for_size, arg_str in run_plan:
                print(f"\n  Running {example_name} with problem configuration: '{problem_config_str}' for {num_iterations_for_size} iterations, threads={threads}")
                current_run_result = {
                    "run_id": run_counter,
                    "example_name": example_name,
                    "problem_size": problem_config_str,
                    "iterations_for_this_size": num_iterations_for_size,
                    "carts_config": current_arts_config,
                    "carts": None,
                    "omp": None,
                    "status": "build successful, run pending",
                    "speedup_carts_vs_omp": None,
                    "threads": threads
                }
                carts_exe_path = os.path.join(example_dir, example_name)
                carts_perf = run_program_instance(
                    carts_exe_path, problem_config_str + (f" {arg_str}" if arg_str else ""), num_iterations_for_size, example_name,
                    "CARTS", args.timeout_seconds)
                current_run_result["carts"] = carts_perf
                if not carts_perf or not carts_perf.get("times_seconds"):
                    current_run_result["carts_status"] = f"CARTS run failed for config '{problem_config_str}'"
                omp_exe_path = os.path.join(example_dir, f"{example_name}_omp")
                orig_env = os.environ.copy()
                os.environ["OMP_NUM_THREADS"] = str(threads)
                omp_perf = run_program_instance(
                    omp_exe_path, problem_config_str + (f" {arg_str}" if arg_str else ""), num_iterations_for_size, example_name,
                    "C", args.timeout_seconds)
                os.environ.clear()
                os.environ.update(orig_env)
                current_run_result["omp"] = omp_perf
                if not omp_perf or not omp_perf.get("times_seconds"):
                    current_run_result["omp_status"] = f"C/OpenMP run failed for config '{problem_config_str}'"
                if carts_perf.get("avg_seconds") and omp_perf.get("avg_seconds") and carts_perf["avg_seconds"] > 1e-12:
                    speedup = omp_perf["avg_seconds"] / carts_perf["avg_seconds"]
                    current_run_result["speedup_carts_vs_omp"] = speedup
                current_run_result["status"] = "run completed"
                all_run_results_data.append(current_run_result)
                run_counter += 1
    return all_run_results_data


def generate_all_thread_graphs(all_run_results_data, images_dir, pdfs_dir):
    for threads in sorted(set(r['threads'] for r in all_run_results_data)):
        thread_results = [r for r in all_run_results_data if r['threads'] == threads]
        if not thread_results:
            continue
        thread_prefix = f"threads{threads}_"
        graph_file_path = os.path.join(images_dir, f"{thread_prefix}comparison.png")
        graph_pdf_path = os.path.join(pdfs_dir, f"{thread_prefix}comparison.pdf")
        speedup_graph_file_path = os.path.join(images_dir, f"{thread_prefix}speedup.png")
        speedup_pdf_path = os.path.join(pdfs_dir, f"{thread_prefix}speedup.pdf")
        scatter_file_path = os.path.join(images_dir, f"{thread_prefix}scatter.png")
        scatter_pdf_path = os.path.join(pdfs_dir, f"{thread_prefix}scatter.pdf")
        speedup_dist_file = os.path.join(images_dir, f"{thread_prefix}speedup_dist.png")
        speedup_dist_pdf = os.path.join(pdfs_dir, f"{thread_prefix}speedup_dist.pdf")
        aggregate_stats = compute_aggregate_statistics(thread_results)
        generate_comparison_graph(thread_results, graph_file_path, graph_pdf_path, threads=threads)
        generate_speedup_graph(thread_results, speedup_graph_file_path, threads=threads)
        generate_scatter_plot(thread_results, scatter_file_path)
        generate_speedup_distribution_plot(aggregate_stats['speedups'], speedup_dist_file)


def clear_output_dir(output_dir):
    for item in os.listdir(output_dir):
        if item == '.gitignore':
            continue
        item_path = os.path.join(output_dir, item)
        if os.path.isdir(item_path):
            shutil.rmtree(item_path)
        else:
            os.remove(item_path)


def generate_example_scaling_plots(results_data, images_dir, pdfs_dir):
    from collections import defaultdict
    example_groups = defaultdict(list)
    for r in results_data:
        example_groups[r['example_name']].append(r)
    for example, runs in example_groups.items():
        runs = [r for r in runs if r.get('threads') is not None and r.get('carts', {}).get('avg_seconds') is not None and r.get('omp', {}).get('avg_seconds') is not None]
        if not runs:
            continue
        threads_sorted = sorted(set(r['threads'] for r in runs))
        carts_times = [min([rr['carts']['avg_seconds'] for rr in runs if rr['threads'] == t]) for t in threads_sorted]
        omp_times = [min([rr['omp']['avg_seconds'] for rr in runs if rr['threads'] == t]) for t in threads_sorted]
        # Scaling plot (absolute times)
        fig, ax = plt.subplots(figsize=(8,6))
        ax.plot(threads_sorted, carts_times, marker='o', color=CUSTOM_COLORS[0], label='CARTS')
        ax.plot(threads_sorted, omp_times, marker='o', color=CUSTOM_COLORS[1], label='OpenMP')
        ax.set_xlabel('Threads')
        ax.set_ylabel('Execution Time (s)')
        ax.set_title(f'Scaling: {example} (Threads: {threads_sorted})')
        ax.set_xscale('log', base=2)
        ax.set_xticks(threads_sorted)
        ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
        ax.legend()
        ax.grid(True, which='both', linestyle='--', alpha=0.7)
        plt.tight_layout()
        img_path = os.path.join(images_dir, f'scaling_{example}.png')
        pdf_path = os.path.join(pdfs_dir, f'scaling_{example}.pdf')
        plt.savefig(img_path, bbox_inches='tight', dpi=300)
        plt.savefig(pdf_path, bbox_inches='tight')
        plt.close(fig)
        # Speedup plot
        base_carts = carts_times[0]
        base_omp = omp_times[0]
        carts_speedup = [base_carts / t if t > 0 else 0 for t in carts_times]
        omp_speedup = [base_omp / t if t > 0 else 0 for t in omp_times]
        fig, ax = plt.subplots(figsize=(8,6))
        ax.plot(threads_sorted, carts_speedup, marker='o', color=sns.color_palette("deep")[0], label='CARTS')
        ax.plot(threads_sorted, omp_speedup, marker='o', color=sns.color_palette("deep")[1], label='OpenMP')
        ax.set_xlabel('Threads')
        ax.set_ylabel('Speedup (vs 1 thread)')
        ax.set_title(f'Strong Scaling Speedup: {example} (Threads: {threads_sorted})')
        ax.set_xscale('log', base=2)
        ax.set_xticks(threads_sorted)
        ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
        ax.legend()
        ax.grid(True, which='both', linestyle='--', alpha=0.7)
        plt.tight_layout()
        img_path = os.path.join(images_dir, f'speedup_{example}.png')
        pdf_path = os.path.join(pdfs_dir, f'speedup_{example}.pdf')
        plt.savefig(img_path, bbox_inches='tight', dpi=300)
        plt.savefig(pdf_path, bbox_inches='tight')
        plt.close(fig)
        # Efficiency plot
        carts_eff = [s/t if t > 0 else 0 for s, t in zip(carts_speedup, threads_sorted)]
        omp_eff = [s/t if t > 0 else 0 for s, t in zip(omp_speedup, threads_sorted)]
        fig, ax = plt.subplots(figsize=(8,6))
        ax.plot(threads_sorted, carts_eff, marker='o', color=CUSTOM_COLORS[0], label='CARTS')
        ax.plot(threads_sorted, omp_eff, marker='o', color=CUSTOM_COLORS[1], label='OpenMP')
        ax.set_xlabel('Threads')
        ax.set_ylabel('Efficiency (Speedup/Threads)')
        ax.set_title(f'Efficiency: {example} (Threads: {threads_sorted})')
        ax.set_xscale('log', base=2)
        ax.set_xticks(threads_sorted)
        ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter())
        ax.legend()
        ax.grid(True, which='both', linestyle='--', alpha=0.7)
        plt.tight_layout()
        img_path = os.path.join(images_dir, f'efficiency_{example}.png')
        pdf_path = os.path.join(pdfs_dir, f'efficiency_{example}.pdf')
        plt.savefig(img_path, bbox_inches='tight', dpi=300)
        plt.savefig(pdf_path, bbox_inches='tight')
        plt.close(fig)


def main():
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")
    print("CARTS Performance Comparison Script")
    parser = argparse.ArgumentParser(
        description="Run and compare performance of example programs across various problem sizes.")
    parser.add_argument("--timeout_seconds", type=int, default=300,
                        help="Timeout in seconds for each individual program run (default: 300s).")
    parser.add_argument("--output_prefix", type=str, default="performance",
                        help="Prefix for all output files (e.g., 'my_run' will generate 'my_run_results.json', 'my_run_graph.png', etc.). Default: 'performance'")
    parser.add_argument("--example_base_dirs", nargs='+',
                        default=["examples/parallel", "examples/tasking"], help="Base directories for examples.")
    parser.add_argument("--target_examples", type=str, nargs='+', default=None,
                        help="Optional: List of specific example names to run (e.g., addition dotproduct). If not provided, all found examples are run.")
    parser.add_argument('--clear_output', type=bool, default=True,
                       help='Clear the output directory before running (default: True). Set to False to keep previous results.')
    args = parser.parse_args()
    print(f"[carts-test] Arguments parsed: {args}")
    system_info = get_system_info()
    original_cwd = os.getcwd()
    output_dir = os.path.join(original_cwd, "output")
    os.makedirs(output_dir, exist_ok=True)
    if args.clear_output:
        clear_output_dir(output_dir)
    images_dir = ensure_images_dir(output_dir)
    pdfs_dir = ensure_pdfs_dir(output_dir)
    print(f"Output will be saved to directory: {output_dir}")
    output_json_path = os.path.join(output_dir, f"{args.output_prefix}_results.json")
    csv_file_path = os.path.join(output_dir, f"{args.output_prefix}_report.csv")
    graph_file_path = os.path.join(images_dir, f"{args.output_prefix}_comparison.png")
    md_file_path = os.path.join(output_dir, f"{args.output_prefix}_summary.md")
    speedup_graph_file_path = os.path.join(images_dir, f"{args.output_prefix}_speedup.png")
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
    all_run_results_data = run_all_benchmarks(example_directories, args, system_info)
    aggregate_stats = compute_aggregate_statistics(all_run_results_data)
    per_thread_stats = per_thread_aggregate_stats(all_run_results_data)
    per_example_stats = per_example_aggregate_stats(all_run_results_data)
    dep_versions = get_dependency_versions()
    reproducibility = get_reproducibility_info()
    failures = collect_failure_table(all_run_results_data)
    # Generate all reports/plots as before
    with open(output_json_path, "w") as f:
        # Read the markdown report as a string
        try:
            with open(md_file_path, "r") as mdfile:
                report_markdown = mdfile.read()
        except Exception:
            report_markdown = ""
        # List all generated images and PDFs
        def list_files(dir_path, exts=(".png", ".pdf")):
            files = []
            for ext in exts:
                files.extend(sorted([os.path.join(os.path.basename(dir_path), f) for f in os.listdir(dir_path) if f.endswith(ext)]))
            return files
        generated_images = list_files(images_dir, (".png",)) + list_files(pdfs_dir, (".pdf",))
        # Compose the new JSON structure
        final_json_output = {
            "system_information": system_info,
            "benchmark_results": all_run_results_data,
            "aggregate_statistics": aggregate_stats,
            "per_thread_statistics": per_thread_stats,
            "per_example_statistics": per_example_stats,
            "dependency_versions": dep_versions,
            "reproducibility": reproducibility,
            "failures": failures,
            "report_markdown": report_markdown,
            "generated_images": generated_images
        }
        json.dump(final_json_output, f, indent=2)
    generate_csv_report(all_run_results_data, csv_file_path, system_info)
    generate_comparison_graph(all_run_results_data, graph_file_path, graph_file_path.replace('.png', '.pdf'))
    speedup_dist_file = graph_file_path.replace('_comparison.png', '_speedup_dist.png')
    generate_speedup_distribution_plot(aggregate_stats['speedups'], speedup_dist_file)
    generate_markdown_report(all_run_results_data, system_info,
                             md_file_path, graph_file_path, csv_file_path, speedup_graph_file_path, aggregate_stats, speedup_dist_file, images_dir, pdfs_dir)
    generate_all_thread_graphs(all_run_results_data, images_dir, pdfs_dir)
    generate_example_scaling_plots(all_run_results_data, images_dir, pdfs_dir)
    print("\nDumping Comparison Results (JSON)")
    json.dumps(final_json_output, indent=2)
    print("Script finished.")
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")


if __name__ == "__main__":
    main()
