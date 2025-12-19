///===----------------------------------------------------------------------===///
/// File: CartsBenchmarks.h
///
/// C benchmark utilities for CARTS benchmarks.
/// Provides standardized checksum reporting and kernel timing.
///
/// Usage:
///   #include "arts/Utils/Benchmarks/CartsBenchmarks.h"
///
///   int main() {
///       CARTS_KERNEL_TIMER_START("kernel");
///       kernel_function(...);
///       CARTS_KERNEL_TIMER_STOP("kernel");
///
///       // Compute checksum inline (NOT via helper functions - CARTS
///       limitation) double checksum = 0.0; for (int i = 0; i < N; i++)
///       checksum += data[i]; CARTS_BENCH_CHECKSUM(checksum); return 0;
///   }
///
/// Output format:
///   kernel.name: 0.1234s
///   checksum: 1.234567e+00
///
/// Note: Link with libcartsbenchmarks (-lcartsbenchmarks) for the
/// implementation.
///===----------------------------------------------------------------------===///

#ifndef CARTS_BENCHMARKS_H
#define CARTS_BENCHMARKS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

///===----------------------------------------------------------------------===///
/// Benchmark Start/Stop (call to set up fair timing)
///===----------------------------------------------------------------------===///

/// Pre-warm OMP thread pool. Implemented in CartsBenchmarks.cpp (compiled -O0).
void carts_benchmarks_warmup_omp(void);

/// Benchmark start hook for fair timing comparisons.
/// For OpenMP reference builds this warms up the OMP thread pool and may emit
/// `init.omp: <sec>s` when CARTS_BENCHMARKS_REPORT_INIT=1.
/// Implemented in CartsBenchmarks.cpp (compiled -O0).
void carts_benchmarks_start(void);

/// For OMP: calls extern warmup function to initialize thread pool before
/// timing. For ARTS: no-op (ARTS initializes at startup).
#ifdef CARTS_BENCHMARKS_OMP
#define CARTS_BENCHMARKS_START() carts_benchmarks_start()
#else
#define CARTS_BENCHMARKS_START() ((void)0)
#endif

#define CARTS_BENCHMARKS_STOP() ((void)0)

///===----------------------------------------------------------------------===///
/// End-to-End Timing (high-precision wall clock via std::chrono::steady_clock)
///
/// Use for fair ARTS vs OMP comparison. Measures wall clock time including
/// DAG construction (ARTS) and parallel region overhead (OMP).
///
/// Usage:
///   CARTS_E2E_TIMER_START("kernel_name");
///   kernel(...);
///   CARTS_E2E_TIMER_STOP();
///
/// Output format:
///   e2e.kernel_name: 0.123456789s
///===----------------------------------------------------------------------===///

/// Global timer state for E2E timing
static uint64_t _carts_e2e_timer_start_ns = 0;
static const char *_carts_e2e_timer_name = NULL;

/// Get current time in nanoseconds (implemented in CartsBenchmarks.cpp)
uint64_t carts_e2e_timer_get_time_ns(void);

#define CARTS_E2E_TIMER_START(name)                                            \
  do {                                                                         \
    _carts_e2e_timer_name = name;                                              \
    _carts_e2e_timer_start_ns = carts_e2e_timer_get_time_ns();                 \
  } while (0)

#define CARTS_E2E_TIMER_STOP()                                                 \
  do {                                                                         \
    uint64_t elapsed_ns =                                                      \
        carts_e2e_timer_get_time_ns() - _carts_e2e_timer_start_ns;             \
    printf("e2e.%s: %.9fs\n", _carts_e2e_timer_name,                           \
           (double)elapsed_ns * 1e-9);                                         \
  } while (0)

///===----------------------------------------------------------------------===///
/// Legacy Timing Utilities (inline - Polygeist compatible)
///===----------------------------------------------------------------------===///

/// Global timer start value for program-level timing
static double _carts_bench_timer_start = 0.0;

/// Kernel timer start value (single kernel at a time)
static double _carts_kernel_timer_start = 0.0;

/// Accumulated kernel time (for iterative methods)
static double _carts_kernel_timer_accum = 0.0;

/// Get current wall-clock time in seconds.
static inline double carts_bench_get_time(void) {
#ifdef _OPENMP
  return omp_get_wtime();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

/// Start program-level timer
#define CARTS_BENCH_TIMER_START()                                              \
  _carts_bench_timer_start = carts_bench_get_time()

/// Get elapsed program time since CARTS_BENCH_TIMER_START()
#define CARTS_BENCH_TIMER_STOP()                                               \
  (carts_bench_get_time() - _carts_bench_timer_start)

/// Print program-level elapsed time
#define CARTS_BENCH_TIMER_PRINT()                                              \
  printf("time: %.6f\n", CARTS_BENCH_TIMER_STOP())

///===----------------------------------------------------------------------===///
/// Kernel Timing (for fine-grained measurement)
///===----------------------------------------------------------------------===///

/// Start timing a kernel (resets accumulator)
#define CARTS_KERNEL_TIMER_START(name)                                         \
  do {                                                                         \
    _carts_kernel_timer_accum = 0.0;                                           \
    _carts_kernel_timer_start = carts_bench_get_time();                        \
  } while (0)

/// Stop kernel timer and print elapsed time
#define CARTS_KERNEL_TIMER_STOP(name)                                          \
  do {                                                                         \
    double elapsed = carts_bench_get_time() - _carts_kernel_timer_start;       \
    printf("kernel.%s: %.6fs\n", name, elapsed);                               \
  } while (0)

/// Accumulate kernel time (for iterative methods - call each iteration)
#define CARTS_KERNEL_TIMER_ACCUM(name)                                         \
  do {                                                                         \
    _carts_kernel_timer_accum +=                                               \
        carts_bench_get_time() - _carts_kernel_timer_start;                    \
    _carts_kernel_timer_start = carts_bench_get_time();                        \
  } while (0)

/// Print accumulated kernel time (call after all iterations)
#define CARTS_KERNEL_TIMER_PRINT(name)                                         \
  printf("kernel.%s: %.6fs\n", name, _carts_kernel_timer_accum)

///===----------------------------------------------------------------------===///
/// Parallel Region & Task Timing (thread-safe, per-worker)
///
/// These macros measure time spent in parallel regions and tasks separately.
/// Used to analyze whether delayed MLIR optimizations (CSE, DCE) due to DB
/// partitioning affect sequential kernel performance.
///
/// Usage:
///   #pragma omp parallel
///   {
///       CARTS_PARALLEL_TIMER_START("gemm");
///       #pragma omp task
///       {
///           CARTS_TASK_TIMER_START("gemm:kernel");
///           kernel_computation(...);
///           CARTS_TASK_TIMER_STOP("gemm:kernel");
///       }
///       #pragma omp taskwait
///       CARTS_PARALLEL_TIMER_STOP("gemm");
///   }
///
/// Output format:
///   parallel.gemm[worker=0]: 0.001234s
///   task.gemm:kernel[worker=0]: 0.001100s
///
/// Enable with: -DCARTS_ENABLE_TIMING or #define CARTS_ENABLE_TIMING
/// Disabled by default to avoid overhead in production runs.
///===----------------------------------------------------------------------===///

#ifdef CARTS_ENABLE_TIMING

/// Thread-local storage for per-worker timing (C11 _Thread_local)
/// Each OpenMP thread has its own timer state for thread-safety
static _Thread_local double _carts_parallel_timer_start = 0.0;
static _Thread_local double _carts_task_timer_start = 0.0;

/// Start timing a parallel region (call inside #pragma omp parallel)
#define CARTS_PARALLEL_TIMER_START(name)                                       \
  _carts_parallel_timer_start = carts_bench_get_time()

/// Stop parallel region timer and print elapsed time with worker ID
#define CARTS_PARALLEL_TIMER_STOP(name)                                        \
  do {                                                                         \
    double elapsed = carts_bench_get_time() - _carts_parallel_timer_start;     \
    printf("parallel.%s[worker=%d]: %.6fs\n", name, omp_get_thread_num(),      \
           elapsed);                                                           \
  } while (0)

/// Start timing a task (sequential kernel work inside #pragma omp task)
#define CARTS_TASK_TIMER_START(name)                                           \
  _carts_task_timer_start = carts_bench_get_time()

/// Stop task timer and print elapsed time with worker ID
#define CARTS_TASK_TIMER_STOP(name)                                            \
  do {                                                                         \
    double elapsed = carts_bench_get_time() - _carts_task_timer_start;         \
    printf("task.%s[worker=%d]: %.6fs\n", name, omp_get_thread_num(),          \
           elapsed);                                                           \
  } while (0)

#else /* !CARTS_ENABLE_TIMING */

#define CARTS_PARALLEL_TIMER_START(name) ((void)0)
#define CARTS_PARALLEL_TIMER_STOP(name) ((void)0)
#define CARTS_TASK_TIMER_START(name) ((void)0)
#define CARTS_TASK_TIMER_STOP(name) ((void)0)

#endif /* CARTS_ENABLE_TIMING */

///===----------------------------------------------------------------------===///
/// Checksum/Result Reporting (implemented in libcartsbenchmarks)
///===----------------------------------------------------------------------===///

/// Print checksum as double (most common case)
/// Output: "checksum: <value>"
void carts_bench_checksum_d(double value);

/// Print checksum as float
/// Output: "checksum: <value>"
void carts_bench_checksum_f(float value);

/// Print checksum as integer
/// Output: "checksum: <value>"
void carts_bench_checksum_i(int value);

/// Print checksum as long
/// Output: "checksum: <value>"
void carts_bench_checksum_l(long value);

/// Print RMS error (for verification benchmarks)
/// Output: "RMS error: <value>"
void carts_bench_rms_error(double error);

/// Print a named result value
/// Output: "<name>: <value>"
void carts_bench_result_named(const char *name, double value);

/// Print sum (alternative to checksum for some benchmarks)
/// Output: "sum: <value>"
void carts_bench_sum(double value);

/// Print total (for aggregation benchmarks)
/// Output: "total: <value>"
void carts_bench_total(double value);

/// Verify result against expected value with tolerance
/// Returns 0 if passed, 1 if failed
/// Output: "Verification: PASS (checksum: <value>)" or
///         "Verification: FAIL (checksum: <value>, expected: <value>, diff:
///         <value>)"
int carts_bench_verify_d(double actual, double expected, double tolerance);

///===----------------------------------------------------------------------===///
/// Convenience Macros
///===----------------------------------------------------------------------===///

/// Generic checksum macro - automatically selects correct function
#if __STDC_VERSION__ >= 201112L
#define CARTS_BENCH_CHECKSUM(val)                                              \
  _Generic((val),                                                              \
      double: carts_bench_checksum_d,                                          \
      float: carts_bench_checksum_f,                                           \
      int: carts_bench_checksum_i,                                             \
      long: carts_bench_checksum_l,                                            \
      default: carts_bench_checksum_d)(val)
#else
#define CARTS_BENCH_CHECKSUM(val) carts_bench_checksum_d((double)(val))
#endif

/// RMS error reporting
#define CARTS_BENCH_RMS_ERROR(error) carts_bench_rms_error(error)

/// Named result reporting
#define CARTS_BENCH_RESULT(name, value) carts_bench_result_named(name, value)

/// Sum reporting
#define CARTS_BENCH_SUM(value) carts_bench_sum(value)

/// Total reporting
#define CARTS_BENCH_TOTAL(value) carts_bench_total(value)

/// Verification with tolerance (returns 0 on pass, 1 on fail)
#define CARTS_BENCH_VERIFY(actual, expected, tol)                              \
  carts_bench_verify_d((double)(actual), (double)(expected), (double)(tol))

#ifdef __cplusplus
}
#endif

#endif // CARTS_BENCHMARKS_H
