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
    print(f"run_comparison.py: Running 'make {make_command}' in {cwd_path}...")
    try:
        process = subprocess.run(
            ["make", make_command], cwd=cwd_path, check=True, text=True, capture_output=True
        )
        print(
            f"run_comparison.py: 'make {make_command}' successful in {cwd_path}.")
        return True
    except subprocess.CalledProcessError as e:
        print(
            f"run_comparison.py: Error during 'make {make_command}' in {cwd_path}: {e.stderr}")
        return False
    except FileNotFoundError:
        print(f"run_comparison.py: Error: 'make' command not found.")
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
    # Flag to track if any iteration timed out
    timeout_occurred_in_any_iteration = False

    base_return = {
        "times_seconds": [],
        "min_seconds": None, "max_seconds": None, "avg_seconds": None,
        "median_seconds": None, "stdev_seconds": None,
        "iterations_ran": 0, "iterations_requested": num_iterations,
        # Default if exec not found
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
                r"finished in\\s+(\\d+\\.\\d+)\\s*seconds", output, re.IGNORECASE)
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
            # If multiple iterations, the last correctness status will be taken.

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
    # Update with potentially found status
    base_return["correctness_status"] = correctness_status

    if not valid_times:
        print(f"No valid execution times collected for {executable_path}.")
        # If no times AND no explicit correctness
        if timeout_occurred_in_any_iteration:  # Prioritize TIMEOUT status
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
        "Median Time (s)", "StdDev Time (s)", "Speedup (C_OMP/CARTS)",
        "Individual Times (s)", "CARTS Threads", "CARTS Nodes", "Overall Example Status"
    ]

    with open(output_csv_file, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        for r_item in results_data:
            example_name = r_item.get("example_name", "N/A")
            problem_size = r_item.get("problem_size", "N/A")
            overall_example_status = r_item.get("status", "processed")
            carts_config = r_item.get("carts_omp_config", {})
            carts_threads_val = carts_config.get("threads", "")
            carts_nodes_val = carts_config.get("nodeCount", "")
            speedup_val = r_item.get("speedup_carts_vs_comp")
            speedup_str = f"{speedup_val:.2f}x" if isinstance(
                speedup_val, float) else (speedup_val if speedup_val else "")

            versions_to_process = [
                ("carts_omp", "CARTS"), ("c_omp", "OpenMP")]
            for version_key, version_label in versions_to_process:
                perf_data = r_item.get(version_key, {})
                row_data = {
                    "Example Name": example_name,
                    "Problem Size": problem_size,
                    "Version": version_label,
                    "Iterations Requested": perf_data.get("iterations_requested", r_item.get("iterations_for_this_size", "N/A")),
                    "Iterations Ran": perf_data.get("iterations_ran", 0),
                    "Correctness Status": perf_data.get("correctness_status", "UNKNOWN"),
                    "Min Time (s)": f"{perf_data.get('min_seconds'):.6f}" if perf_data.get('min_seconds') is not None else "",
                    "Max Time (s)": f"{perf_data.get('max_seconds'):.6f}" if perf_data.get('max_seconds') is not None else "",
                    "Average Time (s)": f"{perf_data.get('avg_seconds'):.6f}" if perf_data.get('avg_seconds') is not None else "",
                    "Median Time (s)": f"{perf_data.get('median_seconds'):.6f}" if perf_data.get('median_seconds') is not None else "",
                    "StdDev Time (s)": f"{perf_data.get('stdev_seconds'):.6f}" if perf_data.get('stdev_seconds') is not None else "",
                    "Individual Times (s)": ", ".join([f"{t:.6f}" for t in perf_data.get("times_seconds", [])]),
                    "Speedup (C_OMP/CARTS)": speedup_str if version_key == "carts_omp" else "",
                    "CARTS Threads": carts_threads_val if version_key == "carts_omp" else "",
                    "CARTS Nodes": carts_nodes_val if version_key == "carts_omp" else "",
                    "Overall Example Status": overall_example_status if perf_data.get("times_seconds") else r_item.get(f"{version_key}_status", "run failed or no data")
                }
                writer.writerow(row_data)
    print(f"CSV report saved to {output_csv_file}")


def generate_markdown_report(results_data, system_info, output_md_file, graph_image_file, csv_file_name):
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
        mdfile.write("| Example | Problem Size | CARTS Avg Time (s) | C/OMP Avg Time (s) | CARTS Correctness | C/OMP Correctness | Speedup | CARTS Threads | CARTS Nodes | Build Status |\n")
        mdfile.write("|---|---|---|---|---|---|---|---|---|---|\n")

        for r_item in results_data:
            carts_perf = r_item.get("carts_omp", {})
            c_omp_perf = r_item.get("c_omp", {})

            carts_avg_seconds = carts_perf.get('avg_seconds')
            c_omp_avg_seconds = c_omp_perf.get('avg_seconds')

            carts_time_str = f"{carts_avg_seconds:.5f}" if carts_avg_seconds is not None else "0.00000"
            c_omp_time_str = f"{c_omp_avg_seconds:.5f}" if c_omp_avg_seconds is not None else "0.00000"

            carts_correct = carts_perf.get('correctness_status', 'UNKNOWN')
            c_omp_correct = c_omp_perf.get('correctness_status', 'UNKNOWN')

            speedup_val = r_item.get("speedup_carts_vs_comp")
            speedup_str = f"{speedup_val:.2f}x" if isinstance(
                speedup_val, float) else (speedup_val if speedup_val else "N/A")

            carts_cfg = r_item.get("carts_omp_config", {})
            threads = carts_cfg.get("threads", "-")
            nodes = carts_cfg.get("nodeCount", "-")
            status = r_item.get("status", "processed")

            mdfile.write(f"| {r_item.get('example_name', 'N/A')} "
                         f"| {r_item.get('problem_size', 'N/A')} "
                         f"| {carts_time_str} "
                         f"| {c_omp_time_str} "
                         f"| {carts_correct} "
                         f"| {c_omp_correct} "
                         f"| {speedup_str} "
                         f"| {threads} "
                         f"| {nodes} "
                         f"| {status} |\n")

        mdfile.write(
            f"\n## Performance Graph\n![Performance Graph]({os.path.basename(graph_image_file)})\n\n")
        mdfile.write(
            f"## Detailed Data\nSee CSV: [{os.path.basename(csv_file_name)}]({os.path.basename(csv_file_name)})\n")

    print(f"Markdown report saved to {output_md_file}")


def generate_comparison_graph(results_data, output_image_file):
    """Generates a bar chart with distinct fill colors for versions and edge colors for correctness."""
    ax = None  # Initialize ax
    if not results_data:
        print("No data to generate graph.")
        return

    x_tick_labels = [
        f"{r['example_name']}\n(Size: {r['problem_size']})" for r in results_data]

    carts_avg_times = [r.get('carts_omp', {}).get('avg_seconds') if r.get(
        'carts_omp', {}).get('avg_seconds') is not None else 0 for r in results_data]
    c_omp_avg_times = [r.get('c_omp', {}).get('avg_seconds') if r.get(
        'c_omp', {}).get('avg_seconds') is not None else 0 for r in results_data]

    carts_correctness = [r.get('carts_omp', {}).get(
        'correctness_status', 'NOT_RUN') for r in results_data]
    c_omp_correctness = [r.get('c_omp', {}).get(
        'correctness_status', 'NOT_RUN') for r in results_data]

    x = np.arange(len(x_tick_labels))
    width = 0.35
    fig_width = max(12, len(x_tick_labels) * 1.5)
    # Increase figure height slightly to accommodate legend at the bottom
    fig_height = 10
    fig, ax = plt.subplots(figsize=(fig_width, fig_height))

    # 1. Define distinct fill colors for each version as requested
    carts_bar_fill_color = 'orange'      # CARTS: Orange
    c_omp_bar_fill_color = 'cornflowerblue'  # OpenMP: Blue

    def get_edge_color_for_status(status):
        if status == 'CORRECT':
            return 'green'
        elif status == 'INCORRECT':
            return 'red'
        else:  # Covers UNKNOWN, NOT_RUN, make failed, etc.
            return 'black'

    carts_edge_colors = [get_edge_color_for_status(
        s) for s in carts_correctness]
    c_omp_edge_colors = [get_edge_color_for_status(
        s) for s in c_omp_correctness]

    rects1 = ax.bar(x - width/2, carts_avg_times, width,
                    label='CARTS',  # Updated label
                    color=carts_bar_fill_color,
                    edgecolor=carts_edge_colors,
                    linewidth=1.5, alpha=0.9)

    rects2 = ax.bar(x + width/2, c_omp_avg_times, width,
                    label='OpenMP',  # Updated label
                    color=c_omp_bar_fill_color,
                    edgecolor=c_omp_edge_colors,
                    linewidth=1.5, alpha=0.9)

    ax.set_ylabel('Average Execution Time (seconds)', fontsize=14)
    ax.set_xlabel('Example & Problem Size', fontsize=14)
    ax.set_title('Performance Comparison: CARTS vs OpenMP',
                 fontsize=16, pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(x_tick_labels, rotation=45, ha="right", fontsize=10)
    ax.tick_params(axis='y', labelsize=10)
    ax.grid(axis='y', linestyle='--', alpha=0.7)

    from matplotlib.patches import Patch

    legend_handles = [
        Patch(facecolor=carts_bar_fill_color, edgecolor='black',
              linewidth=1, label='CARTS'),  # Updated label
        Patch(facecolor=c_omp_bar_fill_color, edgecolor='black',
              linewidth=1, label='OpenMP'),  # Updated label
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
              # Positioned below the plot area
              loc='upper center', bbox_to_anchor=(0.5, -0.15),
              # Arrange horizontally
              fancybox=True, shadow=True, ncol=len(legend_handles),
              fontsize=10, title_fontsize=12)

    # Adjust layout to make space for legend at the bottom
    # The `bottom` parameter in `rect` might need to be increased if legend is clipped
    plt.tight_layout(rect=[0, 0.05, 1, 1])  # rect=[left, bottom, right, top]
    try:
        # Use bbox_inches for tight layout
        plt.savefig(output_image_file, bbox_inches='tight')
        print(f"Comparison graph saved to {output_image_file}")
    except Exception as e:
        print(f"Error saving graph: {e}")
    plt.close(fig)  # Close the figure to free memory


def autolabel_bars(rects, correctness_statuses, all_results_for_speedup, version_key, ax_arg):  # Add ax_arg
    for i, rect in enumerate(rects):
        height = rect.get_height()
        status = correctness_statuses[i]
        label_text = f'{height:.5f}s'
        if version_key == 'carts_omp':
            speedup_val = all_results_for_speedup[i].get(
                "speedup_carts_vs_comp")
            if speedup_val and isinstance(speedup_val, float):
                label_text += f"\n({speedup_val:.2f}x)"
        text_color = 'black'
        if status == 'INCORRECT':
            text_color = 'white'
        if height == 0 and not (status == 'CORRECT' and height == 0):
            ax_arg.text(rect.get_x() + rect.get_width()/2., 0.01 * ax_arg.get_ylim()[1], status.replace(" ", "\n").replace("(", "\n("),  # Use ax_arg
                        ha='center', va='bottom', color='darkgrey', fontsize=6, rotation=90)
        elif height > 0:
            ax_arg.text(rect.get_x() + rect.get_width()/2., height, label_text,  # Use ax_arg
                        ha='center', va='bottom', fontsize=7, color=text_color,
                        bbox=dict(facecolor='white', alpha=0.6, pad=1, edgecolor='none') if status == 'INCORRECT' else None)


def main():
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")
    print("CARTS Performance Comparison Script")
    parser = argparse.ArgumentParser(
        description="Run and compare performance of example programs across various problem sizes.")
    parser.add_argument("--problem_sizes", nargs='+', required=True,
                        help="List of problem configurations (e.g., 100 or '100 10' for size and tile). Each configuration is a string.")
    parser.add_argument("--iterations_per_size", type=int, nargs='+', required=True,
                        help="List of iteration counts for each problem size configuration, or a single value for all.")
    parser.add_argument("--timeout_seconds", type=int, default=300,  # Added timeout argument
                        help="Timeout in seconds for each individual program run (default: 300s).")
    parser.add_argument("--output_file", type=str,
                        default="all_examples_comparison_results.json", help="JSON output file.")
    parser.add_argument("--example_base_dirs", nargs='+',
                        default=["examples/parallel"], help="Base directories for examples.")
    parser.add_argument("--graph_output_file", type=str,
                        default="performance_comparison.png", help="Comparison graph PNG file.")
    parser.add_argument("--csv_output_file", type=str,
                        default="performance_report.csv", help="Detailed CSV report file.")
    parser.add_argument("--md_output_file", type=str,
                        default="performance_summary.md", help="Markdown summary report file.")
    parser.add_argument("--target_examples", type=str, nargs='+', default=None,
                        help="Optional: List of specific example names to run (e.g., addition dotproduct). If not provided, all found examples are run.")

    args = parser.parse_args()
    print(f"run_comparison.py: Arguments parsed: {args}")

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

    system_info = get_system_info()
    script_location_dir = os.path.dirname(os.path.realpath(__file__))
    # Assuming script is in examples/ ...
    project_root_dir = os.path.abspath(
        os.path.join(script_location_dir, '../../../'))

    # Get current working directory where script is invoked
    original_cwd = os.getcwd()
    output_dir = os.path.join(original_cwd, "output")
    os.makedirs(output_dir, exist_ok=True)
    print(f"Output will be saved to directory: {output_dir}")

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
                    "example_name": example_name, "problem_size": problem_size, "iterations_for_this_size": num_iterations_for_size,
                    "carts_omp_config": current_arts_config,
                    "carts_omp": {"correctness_status": "NOT_RUN (Make Clean Failed)", "iterations_requested": num_iterations_for_size},
                    "c_omp": {"correctness_status": "NOT_RUN (Make Clean Failed)", "iterations_requested": num_iterations_for_size},
                    "status": "make clean failed", "speedup_carts_vs_comp": None
                })
            continue

        if not run_make_command("all", example_dir):
            print(
                f"'make all' failed for {example_name}. Skipping all problem sizes for this example.")
            for problem_size, num_iterations_for_size in run_plan:
                all_run_results_data.append({
                    "example_name": example_name, "problem_size": problem_size, "iterations_for_this_size": num_iterations_for_size,
                    "carts_omp_config": current_arts_config,
                    "carts_omp": {"correctness_status": "NOT_RUN (Make All Failed)", "iterations_requested": num_iterations_for_size},
                    "c_omp": {"correctness_status": "NOT_RUN (Make All Failed)", "iterations_requested": num_iterations_for_size},
                    "status": "make all failed", "speedup_carts_vs_comp": None
                })
            continue

        for problem_config_str, num_iterations_for_size in run_plan:
            print(
                f"\n  Running {example_name} with problem configuration: '{problem_config_str}' for {num_iterations_for_size} iterations")

            current_run_result = {
                "example_name": example_name,
                "problem_size": problem_config_str,
                "iterations_for_this_size": num_iterations_for_size,
                "carts_omp_config": current_arts_config,
                "carts_omp": None,
                "c_omp": None,
                "status": "build successful, run pending",
                "speedup_carts_vs_comp": None
            }

            carts_exe_path = os.path.join(example_dir, example_name)
            carts_perf = run_program_instance(
                carts_exe_path, problem_config_str, num_iterations_for_size, example_name, "CARTS", args.timeout_seconds)  # Pass timeout
            current_run_result["carts_omp"] = carts_perf
            if not carts_perf or not carts_perf.get("times_seconds"):
                current_run_result[
                    "carts_omp_status"] = f"CARTS run failed for config '{problem_config_str}'"

            c_omp_exe_path = os.path.join(example_dir, f"{example_name}_c_omp")
            c_omp_perf = run_program_instance(
                c_omp_exe_path, problem_config_str, num_iterations_for_size, example_name, "C", args.timeout_seconds)  # Pass timeout
            current_run_result["c_omp"] = c_omp_perf
            if not c_omp_perf or not c_omp_perf.get("times_seconds"):
                current_run_result[
                    "c_omp_status"] = f"C/OpenMP run failed for config '{problem_config_str}'"

            if carts_perf.get("avg_seconds") and c_omp_perf.get("avg_seconds") and carts_perf["avg_seconds"] > 1e-9:
                speedup = c_omp_perf["avg_seconds"] / carts_perf["avg_seconds"]
                current_run_result["speedup_carts_vs_comp"] = speedup

            current_run_result["status"] = "run completed"
            all_run_results_data.append(current_run_result)

    final_json_output = {"system_information": system_info,
                         "benchmark_results": all_run_results_data}

    # Save files into the 'output' subdirectory
    output_json_path = os.path.join(output_dir, args.output_file)
    print(f"\nSaving all results to {output_json_path}...")
    with open(output_json_path, "w") as f:
        json.dump(final_json_output, f, indent=2)
    print("All results saved to JSON.")

    csv_file_path = os.path.join(output_dir, args.csv_output_file)
    # csv_file_path now includes output_dir
    generate_csv_report(all_run_results_data, csv_file_path, system_info)

    graph_file_path = os.path.join(output_dir, args.graph_output_file)
    # graph_file_path now includes output_dir
    generate_comparison_graph(all_run_results_data, graph_file_path)

    md_file_path = os.path.join(output_dir, args.md_output_file)
    # Pass the base filenames for graph and CSV to markdown, as they will be relative to the MD file in the output dir
    generate_markdown_report(all_run_results_data, system_info,
                             md_file_path, args.graph_output_file, args.csv_output_file)

    print("\n--- All Comparison Results (JSON) ---")
    print(json.dumps(final_json_output, indent=2))
    print("Script finished.")
    print("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -")


if __name__ == "__main__":
    main()
