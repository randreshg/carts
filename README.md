# CARTS
Author: Rafael Andres Herrera Guaitero
Email: rafaelhg@udel.edu

### Dependencies
- ARTS
- LLVM 18
- Polygeist

---

## Running an Example (Benchmarking & Profiling)
1. **Activate your environment and ensure dependencies are built.**
2. **Run the benchmarking tool:**

```bash
python3 carts_test.py \
  --example_base_dirs tasking \
  --output_prefix performance \
  --target_examples matrixmul \
  --timeout_seconds 300 \
  --profiling True
```

- This will benchmark and profile all examples found in the specified directories.
- To run only specific examples, add `--target_examples addition dotproduct` (space-separated names).
- To disable profiling, use `--profiling False`.
- To customize perf events, use `--perf_events instructions cycles L1-dcache-loads ...`.

3. **Results:**
   - Output files are saved in the `output/` directory:
     - `performance_results.json` (all results)
     - `performance_report.csv` (timing)
     - `performance_profiling_report.csv` (perf counters)
     - Plots and PDFs in `output/images/` and `output/pdfs/`

4. **View Interactive Report:**

```bash
python3 examples/interactive_report.py
```
- Open your browser to `http://localhost:8050` to explore results, profiling tables, and graphs interactively.
- Use the Profiling Results tab to filter by example, threads, problem size, args, and to view advanced metrics (MPKI, CPI, IPC, bandwidth, miss rates, stalls/instr, etc).

---

For more details, see the comments in `examples/carts_test.py` and `examples/interactive_report.py`.