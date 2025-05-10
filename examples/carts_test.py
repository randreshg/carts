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
matplotlib.use('Agg')


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


def find_example_directories(base_example_paths):
    """Finds subdirectories in the given base example paths."""
    example_dirs = []
    for base_path in base_example_paths:
        if not os.path.isdir(base_path):
            print(f"Warning: Base example path '{base_path}' not found.")
            continue
        for item in os.listdir(base_path):
            full_item_path = os.path.join(base_path, item)
            if os.path.isdir(full_item_path):
                example_dirs.append(full_item_path)
    return example_dirs


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
        "Example Name", "Problem Size", "Version", "Iterations Requested", "Iterations Ran",
        "Correctness Status", "Min Time (s)", "Max Time (s)", "Average Time (s)",
        "Median Time (s)", "StdDev Time (s)", "Speedup (OMP/CARTS)",
        "Individual Times (s)", "CARTS Threads", "CARTS Nodes", "Overall Example Status"
    ]

    with open(output_csv_file, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        for r_item in results_data:
            example_name = r_item.get("example_name", "N/A")
            problem_size = r_item.get("problem_size", "N/A")
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
                    "Example Name": example_name,
                    "Problem Size": problem_size,
                    "Version": version_label,
                    "Iterations Requested": perf_data.get("iterations_requested", r_item.get("iterations_for_this_size", "N/A")),
                    "Iterations Ran": perf_data.get("iterations_ran", 0),
                    "Correctness Status": perf_data.get("correctness_status", "UNKNOWN"),
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


def generate_markdown_report(results_data, system_info, output_md_file, graph_image_file, csv_file_name, speedup_graph_file):
    """Generates a Markdown report summarizing the benchmark run."""
    if not results_data:
        print("No data to generate Markdown report.")
        return

    with open(output_md_file, 'w') as mdfile:
        mdfile.write(f"# Performance Comparison Report\n\n")
        mdfile.write(
            f"**Run Timestamp**: {system_info.get('timestamp', 'N/A')}\n\n")

        mdfile.write("## System Information\n")
        mdfile.write(
            f"- **OS Platform**: {system_info.get('os_platform', 'N/A')}\n")
        mdfile.write(f"- **OS Uname**: {system_info.get('os_uname', 'N/A')}\n")
        mdfile.write(
            f"- **CPU Model**: {system_info.get('cpu_model', 'N/A')}\n")
        mdfile.write(
            f"- **CPU Cores**: {system_info.get('cpu_cores', 'N/A')}\n")
        mdfile.write(
            f"- **Total Memory**: {system_info.get('memory_total_mb', 'N/A')} MB\n")
        mdfile.write(
            f"- **Clang Version**: `{system_info.get('clang_version', 'N/A')}`\n\n")

        mdfile.write("## Summary Table\n")
        mdfile.write("| Example | Problem Size | CARTS Avg Time (s) | OMP Avg Time (s) | CARTS Correctness | OMP Correctness | Speedup | CARTS Threads | CARTS Nodes | Build Status |\n")
        mdfile.write("|---|---|---|---|---|---|---|---|---|---|\n")

        for r_item in results_data:
            carts_perf = r_item.get("carts", {})
            omp_perf = r_item.get("omp", {})

            carts_avg_seconds = carts_perf.get('avg_seconds')
            omp_avg_seconds = omp_perf.get('avg_seconds')

            carts_time_str = f"{carts_avg_seconds:.5f}" if carts_avg_seconds is not None else "0.00000"
            omp_time_str = f"{omp_avg_seconds:.5f}" if omp_avg_seconds is not None else "0.00000"

            carts_correct = carts_perf.get('correctness_status', 'UNKNOWN')
            omp_correct = omp_perf.get('correctness_status', 'UNKNOWN')

            speedup_val = r_item.get("speedup_carts_vs_omp")  # Corrected key
            speedup_str = f"{speedup_val:.2f}x" if isinstance(
                speedup_val, float) else (speedup_val if speedup_val else "N/A")

            carts_cfg = r_item.get("carts_config", {})
            threads = carts_cfg.get("threads", "-")
            nodes = carts_cfg.get("nodeCount", "-")
            status = r_item.get("status", "processed")

            mdfile.write(f"| {r_item.get('example_name', 'N/A')} "
                         f"| {r_item.get('problem_size', 'N/A')} "
                         f"| {carts_time_str} "
                         f"| {omp_time_str} "
                         f"| {carts_correct} "
                         f"| {omp_correct} "
                         f"| {speedup_str} "
                         f"| {threads} "
                         f"| {nodes} "
                         f"| {status} |\n")

        mdfile.write(
            f"\n## Performance Graph\n![Performance Graph]({os.path.basename(graph_image_file)})\n\n")
        mdfile.write(
            f"## Speedup Graph (CARTS vs OpenMP)\n![Speedup Graph]({os.path.basename(speedup_graph_file)})\n\n")
        mdfile.write(
            f"## Detailed Data\nSee CSV: [{os.path.basename(csv_file_name)}]({os.path.basename(csv_file_name)})\n")

    print(f"Markdown report saved to {output_md_file}")


def generate_comparison_graph(results_data, output_image_file):
    """Generates a bar chart with distinct fill colors for versions and edge colors for correctness."""
    ax = None
    if not results_data:
        print("No data to generate graph.")
        return

    x_tick_labels = [
        f"{r['example_name']}\n(Size: {r['problem_size']})" for r in results_data]

    carts_avg_times = [r.get('carts', {}).get('avg_seconds') if r.get(
        'carts', {}).get('avg_seconds') is not None else 0 for r in results_data]
    omp_avg_times = [r.get('omp', {}).get('avg_seconds') if r.get(
        'omp', {}).get('avg_seconds') is not None else 0 for r in results_data]

    carts_correctness = [r.get('carts', {}).get(
        'correctness_status', 'NOT_RUN') for r in results_data]
    omp_correctness = [r.get('omp', {}).get(
        'correctness_status', 'NOT_RUN') for r in results_data]

    x = np.arange(len(x_tick_labels))
    width = 0.35
    fig_width = max(12, len(x_tick_labels) * 1.5)
    # Increase figure height slightly to accommodate legend at the bottom
    fig_height = 10
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))

    # 1. Define distinct fill colors for each version as requested
    carts_bar_fill_color = 'orange'
    omp_bar_fill_color = 'cornflowerblue'

    def get_edge_color_for_status(status):
        if status == 'CORRECT':
            return 'green'
        elif status == 'INCORRECT':
            return 'red'
        else:  # Covers UNKNOWN, NOT_RUN, make failed, etc.
            return 'black'

    carts_edge_colors = [get_edge_color_for_status(
        s) for s in carts_correctness]
    omp_edge_colors = [get_edge_color_for_status(
        s) for s in omp_correctness]

    rects_carts = ax.bar(x - width/2, carts_avg_times, width, label='CARTS',
                         color=carts_bar_fill_color, edgecolor=carts_edge_colors, linewidth=1.5, alpha=0.9)
    rects_omp = ax.bar(x + width/2, omp_avg_times, width, label='OpenMP',
                       color=omp_bar_fill_color, edgecolor=omp_edge_colors, linewidth=1.5, alpha=0.9)

    # Add some text for labels, title and custom x-axis tick labels, etc.
    ax.set_ylabel('Execution Time (s)')
    ax.set_xlabel('Example & Problem Size')
    ax.set_title('Performance Comparison: CARTS vs OpenMP')
    ax.set_xticks(x)
    ax.set_xticklabels(x_tick_labels, rotation=45, ha="right")
    # ax.legend(title="Version") # Legend is now custom and placed at the bottom
    ax.grid(axis='y', linestyle='--', alpha=0.7)

    # Call autolabel_bars to add text labels to the bars
    autolabel_bars(rects_carts, ax, results_data, 'carts')
    autolabel_bars(rects_omp, ax, results_data, 'omp')

    from matplotlib.patches import Patch

    legend_handles = [
        Patch(facecolor=carts_bar_fill_color, edgecolor='black',
              linewidth=1, label='CARTS'),
        Patch(facecolor=omp_bar_fill_color, edgecolor='black',
              linewidth=1, label='OpenMP'),
        Patch(facecolor='white', edgecolor='green',
              linewidth=1.5, label='Edge: Result Correct'),
        Patch(facecolor='white', edgecolor='red',
              linewidth=1.5, label='Edge: Result Incorrect'),
        Patch(facecolor='white', edgecolor='black',
              linewidth=1.5, label='Edge: Unknown/Not Run/Error')
    ]

    # 3. Position legend at the bottom
    # ncol can be adjusted based on how many items you have to make it fit well horizontally
    ax.legend(handles=legend_handles, title="Legend",
              loc='upper center', bbox_to_anchor=(0.5, -0.15),
              fancybox=True, shadow=True, ncol=len(legend_handles),
              fontsize=10, title_fontsize=12)
    # rect=[left, bottom, right, top]
    plt.tight_layout(rect=[0, 0.05, 1, 1])
    try:
        # Use bbox_inches for tight layout
        plt.savefig(output_image_file, bbox_inches='tight')
        print(f"Comparison graph saved to {output_image_file}")
    except Exception as e:
        print(f"Error saving graph: {e}")
    plt.close(fig)  # Close the figure to free memory


def generate_speedup_graph(results_data, output_image_file):
    """Generates a bar chart showing the speedup of CARTS vs OpenMP."""
    if not results_data:
        print("No data to generate speedup graph.")
        return

    # Filter out entries where speedup might not be calculable or meaningful
    valid_results = [
        r for r in results_data
        if r.get("speedup_carts_vs_omp") is not None and
        isinstance(r.get("speedup_carts_vs_omp"), (float, int)) and
        # CARTS ran and took time
        r.get("carts", {}).get("avg_seconds") is not None and r.get("carts", {}).get("avg_seconds") > 0 and
        r.get("omp", {}).get("avg_seconds") is not None  # OMP ran
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

    rects_speedup = ax.bar(
        x, speedup_values, color='lightgreen', edgecolor='darkgreen', linewidth=1)

    ax.set_ylabel('Speedup (OMP Time / CARTS Time)')
    ax.set_xlabel('Example & Problem Size')
    ax.set_title('Speedup: CARTS vs OpenMP')
    ax.set_xticks(x)
    ax.set_xticklabels(x_tick_labels, rotation=45, ha="right")
    ax.grid(axis='y', linestyle='--', alpha=0.7)
    # Add a line at y=1 for reference
    ax.axhline(1, color='grey', lw=1, linestyle='--')

    # Autolabel for speedup values
    for rect in rects_speedup:
        height = rect.get_height()
        ax.annotate(f'{height:.2f}x',
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 3),  # 3 points vertical offset
                    textcoords="offset points",
                    ha='center', va='bottom', fontsize=9)

    # Adjust rect to prevent title/label cutoff
    plt.tight_layout(rect=[0, 0.05, 1, 0.95])
    try:
        plt.savefig(output_image_file, bbox_inches='tight')
        print(f"Speedup graph saved to {output_image_file}")
    except Exception as e:
        print(f"Error saving speedup graph: {e}")
    plt.close(fig)


def autolabel_bars(rects, ax, version_data_list, version_name):
    """Attach a text label above each bar in *rects*, displaying its height and status, and speedup for CARTS."""
    for i, rect in enumerate(rects):
        height = rect.get_height()
        # This is an item from results_data
        current_bar_data = version_data_list[i]

        # Correctly fetch exec_time and status from the nested structure
        version_specific_data = current_bar_data.get(version_name, {})
        # exec_time = version_specific_data.get('avg_seconds') # Removed
        status = version_specific_data.get('correctness_status', 'Unknown')

        # exec_time_str = f"{exec_time:.5f}s" if isinstance(exec_time, float) else (str(exec_time) if exec_time is not None else "N/A") # Removed
        # Show status if not simply CORRECT/OK/UNKNOWN or a known failure state
        status_str = f" [{status}]" if status and status not in ["CORRECT", "OK", "UNKNOWN",
                                                                 "NOT_RUN (Make Clean Failed)", "NOT_RUN (Make All Failed)", "TIMEOUT", "NO_VALID_TIMES"] else ""

        # Only show status string if it's relevant
        label_text = f"{status_str}"

        # Removed speedup text from here

        # Only annotate if there's something to show (e.g., a non-default status)
        if label_text:
            ax.annotate(label_text,
                        xy=(rect.get_x() + rect.get_width() / 2, height),
                        xytext=(0, 3),  # 3 points vertical offset
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=8, rotation=90)  # Added rotation for better readability if bars are close


def main():
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")
    print("CARTS Performance Comparison Script")
    parser = argparse.ArgumentParser(
        description="Run and compare performance of example programs across various problem sizes.")
    parser.add_argument("--problem_sizes", nargs='+', required=True,
                        help="List of problem configurations (e.g., 100 or '100 10' for size and tile). Each configuration is a string.")
    parser.add_argument("--iterations_per_size", type=int, nargs='+', required=True,
                        help="List of iteration counts for each problem size configuration, or a single value for all.")
    parser.add_argument("--timeout_seconds", type=int, default=300,
                        help="Timeout in seconds for each individual program run (default: 300s).")
    # Removed old output file arguments
    parser.add_argument("--output_prefix", type=str, default="performance",
                        help="Prefix for all output files (e.g., 'my_run' will generate 'my_run_results.json', 'my_run_graph.png', etc.). Default: 'performance'")
    parser.add_argument("--example_base_dirs", nargs='+',
                        default=["examples/parallel"], help="Base directories for examples.")
    # graph_output_file, csv_output_file, md_output_file removed
    parser.add_argument("--target_examples", type=str, nargs='+', default=None,
                        help="Optional: List of specific example names to run (e.g., addition dotproduct). If not provided, all found examples are run.")

    args = parser.parse_args()
    print(f"[carts-test] Arguments parsed: {args}")

    if len(args.iterations_per_size) == 1:
        iterations_for_sizes = args.iterations_per_size * \
            len(args.problem_sizes)
    elif len(args.iterations_per_size) != len(args.problem_sizes):
        print("Error: --iterations_per_size must have 1 element or same number of elements as --problem_sizes.")
        return
    else:
        iterations_for_sizes = args.iterations_per_size

    run_plan = list(zip(args.problem_sizes, iterations_for_sizes))
    print(f"Run plan (Size: Iterations): {run_plan}")

    # Get current working directory where script is invoked
    system_info = get_system_info()
    original_cwd = os.getcwd()
    output_dir = os.path.join(original_cwd, "output")
    os.makedirs(output_dir, exist_ok=True)
    print(f"Output will be saved to directory: {output_dir}")

    # Construct filenames using the output_prefix
    output_json_path = os.path.join(
        output_dir, f"{args.output_prefix}_results.json")
    csv_file_path = os.path.join(
        output_dir, f"{args.output_prefix}_report.csv")
    graph_file_path = os.path.join(
        output_dir, f"{args.output_prefix}_comparison.png")
    md_file_path = os.path.join(output_dir, f"{args.output_prefix}_summary.md")
    speedup_graph_file_path = os.path.join(
        output_dir, f"{args.output_prefix}_speedup.png")

    absolute_example_base_dirs = [os.path.join(
        original_cwd, path) for path in args.example_base_dirs]

    example_directories = find_example_directories(absolute_example_base_dirs)
    if not example_directories:
        print("No example directories found.")
        return
    print(
        f"Found {len(example_directories)} example directories: {[os.path.basename(d) for d in example_directories]}")

    if args.target_examples:
        original_found_count = len(example_directories)
        filtered_example_dirs = [
            d for d in example_directories if os.path.basename(d) in args.target_examples]

        if not filtered_example_dirs:
            print(
                f"Error: None of the target examples ({', '.join(args.target_examples)}) were found among the {original_found_count} discovered example directories.")
            print(f"Searched in: {absolute_example_base_dirs}")
            # Check if any of the target_examples were misspelled or not in the discovered list
            discovered_names = [os.path.basename(
                d) for d in example_directories]
            for target in args.target_examples:
                if target not in discovered_names:
                    print(
                        f"  - Target '{target}' was not found in the discovered list: {discovered_names}")
            return

        example_directories = filtered_example_dirs
        print(
            f"Filtered to run only the specified examples: {args.target_examples} -> Found: {[os.path.basename(d) for d in example_directories]}")
    else:
        print("No specific target examples provided, running all found examples.")

    all_run_results_data = []
    for example_dir in example_directories:
        example_name = os.path.basename(example_dir)
        print(f"\nProcessing example: {example_name} in {example_dir}")

        arts_cfg_path = os.path.join(example_dir, "arts.cfg")
        current_arts_config = parse_arts_cfg(arts_cfg_path)

        if not run_make_command("clean", example_dir):
            print(
                f"'make clean' failed for {example_name}. Skipping all problem sizes for this example.")
            for problem_size, num_iterations_for_size in run_plan:
                all_run_results_data.append({
                    "example_name": example_name, "problem_size": problem_size,
                    "iterations_for_this_size": num_iterations_for_size, "carts_config": current_arts_config,
                    "carts": {"correctness_status": "NOT_RUN (Make Clean Failed)", "iterations_requested": num_iterations_for_size},
                    "omp": {"correctness_status": "NOT_RUN (Make Clean Failed)", "iterations_requested": num_iterations_for_size},
                    "status": "make clean failed", "speedup_carts_vs_omp": None
                })
            continue

        if not run_make_command("all", example_dir):
            print(
                f"'make all' failed for {example_name}. Skipping all problem sizes for this example.")
            for problem_size, num_iterations_for_size in run_plan:
                all_run_results_data.append({
                    "example_name": example_name, "problem_size": problem_size,
                    "iterations_for_this_size": num_iterations_for_size, "carts_config": current_arts_config,
                    "carts": {"correctness_status": "NOT_RUN (Make All Failed)", "iterations_requested": num_iterations_for_size},
                    "omp": {"correctness_status": "NOT_RUN (Make All Failed)", "iterations_requested": num_iterations_for_size},
                    "status": "make all failed", "speedup_carts_vs_omp": None
                })
            continue

        for problem_config_str, num_iterations_for_size in run_plan:
            print(
                f"\n  Running {example_name} with problem configuration: '{problem_config_str}' for {num_iterations_for_size} iterations")

            current_run_result = {
                "example_name": example_name,
                "problem_size": problem_config_str,
                "iterations_for_this_size": num_iterations_for_size,
                "carts_config": current_arts_config,
                "carts": None,
                "omp": None,
                "status": "build successful, run pending",
                "speedup_carts_vs_omp": None
            }

            carts_exe_path = os.path.join(example_dir, example_name)
            carts_perf = run_program_instance(
                carts_exe_path, problem_config_str, num_iterations_for_size, example_name,
                "CARTS", args.timeout_seconds)
            current_run_result["carts"] = carts_perf
            if not carts_perf or not carts_perf.get("times_seconds"):
                current_run_result[
                    "carts_status"] = f"CARTS run failed for config '{problem_config_str}'"

            omp_exe_path = os.path.join(example_dir, f"{example_name}_omp")
            omp_perf = run_program_instance(
                omp_exe_path, problem_config_str, num_iterations_for_size, example_name,
                "C", args.timeout_seconds)
            current_run_result["omp"] = omp_perf
            if not omp_perf or not omp_perf.get("times_seconds"):
                current_run_result[
                    "omp_status"] = f"C/OpenMP run failed for config '{problem_config_str}'"

            if carts_perf.get("avg_seconds") and omp_perf.get("avg_seconds") and carts_perf["avg_seconds"] > 1e-12:
                speedup = omp_perf["avg_seconds"] / carts_perf["avg_seconds"]
                current_run_result["speedup_carts_vs_omp"] = speedup

            current_run_result["status"] = "run completed"
            all_run_results_data.append(current_run_result)

    final_json_output = {"system_information": system_info,
                         "benchmark_results": all_run_results_data}

    # Save files into the 'output' subdirectory using new prefixed paths
    with open(output_json_path, "w") as f:
        json.dump(final_json_output, f, indent=2)

    generate_csv_report(all_run_results_data, csv_file_path, system_info)
    generate_comparison_graph(all_run_results_data, graph_file_path)
    generate_markdown_report(all_run_results_data, system_info,
                             md_file_path, graph_file_path, csv_file_path, speedup_graph_file_path)
    generate_speedup_graph(all_run_results_data, speedup_graph_file_path)

    print("\n--- All Comparison Results (JSON) ---")
    print(json.dumps(final_json_output, indent=2))
    print("Script finished.")
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")


if __name__ == "__main__":
    main()
