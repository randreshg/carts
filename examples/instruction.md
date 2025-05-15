# CARTS TESTING INFRASTRUCTURE

## How to Run the Benchmarking and Report Script

1. **(Optional) Activate your Python environment**  
   If you use a virtual environment, activate it first:
   ```bash
   source <your-venv>/bin/activate
   ```

2. **Install required Python packages**  
   (If not already installed)
   ```bash
   pip install numpy matplotlib pyyaml seaborn pandas
   ```

3. **Build all required binaries**  
   Make sure your CARTS and OpenMP example binaries are built and available in their respective directories.  
   (If you use a build system or script, run it now.)

4. **Run the benchmarking script**  
   From the project root, run:
   ```bash
   python3 examples/carts_test.py
   ```
   This will:
   - Auto-discover all examples with a `test_config.yaml`
   - Run all benchmarks for all problem sizes, thread counts, and iterations specified in each config
   - Collect performance and correctness data for both CARTS and OpenMP versions

5. **Output files**  
   After completion, you will find the following in the `output/` directory:
   - `performance_results.json` — all results and metadata (for the interactive app)
   - `performance_report.csv` — detailed results in CSV format
   - `performance_summary.md` — publication-quality Markdown report
   - `images/` and `pdfs/` — all generated plots and graphs

6. **Custom options**  
   You can customize the run with these arguments:
   - `--output_prefix <prefix>`: Change the output file prefix (default: `performance`)
   - `--timeout_seconds <seconds>`: Set a timeout for each run (default: 300)
   - `--example_base_dirs <dir1> <dir2> ...`: Specify which example directories to search
   - `--target_examples <ex1> <ex2> ...`: Only run specific examples by name
   - `--clear_output False`: Keep previous results in the output directory

   **Example:**
   ```bash
   python3 examples/carts_test.py --output_prefix myrun --timeout_seconds 600 --target_examples addition dotproduct
   ```


## How to Run the Interactive Report

1. **Install required Python packages:**
   ```bash
   pip install dash pandas plotly
   ```

2. **Generate the results JSON:**
   - Run your benchmarking script:
     ```bash
     python3 examples/carts_test.py
     ```
   - This will produce `output/performance_results.json` (or similar, depending on your `--output_prefix`).

3. **Run the Dash app:**
   ```bash
   python3 examples/interactive_report.py
   ```

4. **Open the app in your browser:**
   - By default, the app will print a URL like:
     ```
     Dash is running on http://127.0.0.1:8050/
     ```
   - Open this URL in your web browser.

---

### If Using VS Code Remote (SSH/WSL):

- **Ensure the last line of `interactive_report.py` is:**
  ```python
  app.run_server(debug=True, host='0.0.0.0', port=8050)
  ```
- **Set up port forwarding:**
  - In VS Code, use the "Ports" tab or run:
    ```bash
    ssh -L 8050:localhost:8050 <your-remote>
    ```
  - Or, in the VS Code UI, forward port 8050.
- **Open the forwarded URL in your local browser:**
  ```
  http://localhost:8050/
  ```

---

**Troubleshooting:**
- If you get a "file not found" error, make sure `output/performance_results.json` exists and is up to date.
- If you change the output prefix, update the path in `interactive_report.py` accordingly.
