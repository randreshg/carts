///===----------------------------------------------------------------------===///
/// File: CartsBenchmarks.cpp
///
/// Implementation of CARTS benchmark utilities.
/// This file MUST be compiled with -O0 to prevent the empty parallel region
/// from being optimized away.
///===----------------------------------------------------------------------===///

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>

#ifdef _OPENMP
#include <omp.h>
#endif

extern "C" {

///===----------------------------------------------------------------------===///
/// High-Precision Wall Clock Timing
///===----------------------------------------------------------------------===///

static const char *carts_e2e_timer_name = nullptr;
static uint64_t carts_e2e_timer_start_ns = 0;
static const char *carts_startup_timer_name = nullptr;
static uint64_t carts_startup_timer_start_ns = 0;
static const char *carts_verification_timer_name = nullptr;
static uint64_t carts_verification_timer_start_ns = 0;
static const char *carts_cleanup_timer_name = nullptr;
static uint64_t carts_cleanup_timer_start_ns = 0;
static double carts_program_timer_start_sec = 0.0;
static double carts_kernel_timer_start_sec = 0.0;
static double carts_kernel_timer_accum_sec = 0.0;
static thread_local double carts_parallel_timer_start_sec = 0.0;
static thread_local double carts_task_timer_start_sec = 0.0;

static int carts_benchmarks_worker_id(void) {
#ifdef _OPENMP
  return omp_get_thread_num();
#else
  return 0;
#endif
}

/// Get current time in nanoseconds using std::chrono::steady_clock
uint64_t carts_e2e_timer_get_time_ns(void) {
  auto now = std::chrono::steady_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      now.time_since_epoch());
  return static_cast<uint64_t>(ns.count());
}

double carts_bench_get_time(void) {
  return (double)carts_e2e_timer_get_time_ns() * 1e-9;
}

__attribute__((noinline)) void carts_e2e_timer_start(const char *name) {
  carts_e2e_timer_name = name;
  carts_e2e_timer_start_ns = carts_e2e_timer_get_time_ns();
}

__attribute__((noinline)) void carts_e2e_timer_stop(void) {
  uint64_t start_ns = carts_e2e_timer_start_ns;
  uint64_t end_ns = carts_e2e_timer_get_time_ns();
  const char *name = carts_e2e_timer_name ? carts_e2e_timer_name : "unnamed";
  double elapsed_sec = start_ns ? (double)(end_ns - start_ns) * 1e-9 : 0.0;
  printf("e2e.%s: %.9fs\n", name, elapsed_sec);
  fflush(stdout);
}

__attribute__((noinline)) void carts_bench_timer_start(void) {
  carts_program_timer_start_sec = carts_bench_get_time();
}

__attribute__((noinline)) double carts_bench_timer_stop(void) {
  if (carts_program_timer_start_sec == 0.0)
    return 0.0;
  return carts_bench_get_time() - carts_program_timer_start_sec;
}

__attribute__((noinline)) void carts_bench_timer_print(void) {
  printf("time: %.6f\n", carts_bench_timer_stop());
  fflush(stdout);
}

__attribute__((noinline)) void carts_kernel_timer_start(const char *name) {
  (void)name;
  carts_kernel_timer_accum_sec = 0.0;
  carts_kernel_timer_start_sec = carts_bench_get_time();
}

__attribute__((noinline)) void carts_kernel_timer_stop(const char *name) {
  const char *kernelName = name ? name : "unnamed";
  double elapsed_sec =
      carts_kernel_timer_start_sec == 0.0
          ? 0.0
          : carts_bench_get_time() - carts_kernel_timer_start_sec;
  printf("kernel.%s: %.9fs\n", kernelName, elapsed_sec);
  fflush(stdout);
}

__attribute__((noinline)) void carts_kernel_timer_accum(const char *name) {
  (void)name;
  if (carts_kernel_timer_start_sec == 0.0)
    return;
  double now_sec = carts_bench_get_time();
  carts_kernel_timer_accum_sec += now_sec - carts_kernel_timer_start_sec;
  carts_kernel_timer_start_sec = now_sec;
}

__attribute__((noinline)) void carts_kernel_timer_print(const char *name) {
  const char *kernelName = name ? name : "unnamed";
  printf("kernel.%s: %.9fs\n", kernelName, carts_kernel_timer_accum_sec);
  fflush(stdout);
}

///===----------------------------------------------------------------------===///
/// Section Timers (startup, verification, cleanup)
///===----------------------------------------------------------------------===///

__attribute__((noinline)) void carts_startup_timer_start(const char *name) {
  carts_startup_timer_name = name;
  carts_startup_timer_start_ns = carts_e2e_timer_get_time_ns();
}

__attribute__((noinline)) void carts_startup_timer_stop(void) {
  uint64_t start_ns = carts_startup_timer_start_ns;
  uint64_t end_ns = carts_e2e_timer_get_time_ns();
  const char *name =
      carts_startup_timer_name ? carts_startup_timer_name : "unnamed";
  double elapsed_sec = start_ns ? (double)(end_ns - start_ns) * 1e-9 : 0.0;
  printf("startup.%s: %.9fs\n", name, elapsed_sec);
  fflush(stdout);
}

__attribute__((noinline)) void carts_verification_timer_start(const char *name) {
  carts_verification_timer_name = name;
  carts_verification_timer_start_ns = carts_e2e_timer_get_time_ns();
}

__attribute__((noinline)) void carts_verification_timer_stop(void) {
  uint64_t start_ns = carts_verification_timer_start_ns;
  uint64_t end_ns = carts_e2e_timer_get_time_ns();
  const char *name =
      carts_verification_timer_name ? carts_verification_timer_name : "unnamed";
  double elapsed_sec = start_ns ? (double)(end_ns - start_ns) * 1e-9 : 0.0;
  printf("verification.%s: %.9fs\n", name, elapsed_sec);
  fflush(stdout);
}

__attribute__((noinline)) void carts_cleanup_timer_start(const char *name) {
  carts_cleanup_timer_name = name;
  carts_cleanup_timer_start_ns = carts_e2e_timer_get_time_ns();
}

__attribute__((noinline)) void carts_cleanup_timer_stop(void) {
  uint64_t start_ns = carts_cleanup_timer_start_ns;
  uint64_t end_ns = carts_e2e_timer_get_time_ns();
  const char *name =
      carts_cleanup_timer_name ? carts_cleanup_timer_name : "unnamed";
  double elapsed_sec = start_ns ? (double)(end_ns - start_ns) * 1e-9 : 0.0;
  printf("cleanup.%s: %.9fs\n", name, elapsed_sec);
  fflush(stdout);
}

__attribute__((noinline)) void carts_parallel_timer_start(const char *name) {
  (void)name;
  carts_parallel_timer_start_sec = carts_bench_get_time();
}

__attribute__((noinline)) void carts_parallel_timer_stop(const char *name) {
  const char *regionName = name ? name : "unnamed";
  double elapsed_sec =
      carts_parallel_timer_start_sec == 0.0
          ? 0.0
          : carts_bench_get_time() - carts_parallel_timer_start_sec;
  printf("parallel.%s[worker=%d]: %.6fs\n", regionName,
         carts_benchmarks_worker_id(), elapsed_sec);
  fflush(stdout);
}

__attribute__((noinline)) void carts_task_timer_start(const char *name) {
  (void)name;
  carts_task_timer_start_sec = carts_bench_get_time();
}

__attribute__((noinline)) void carts_task_timer_stop(const char *name) {
  const char *taskName = name ? name : "unnamed";
  double elapsed_sec =
      carts_task_timer_start_sec == 0.0
          ? 0.0
          : carts_bench_get_time() - carts_task_timer_start_sec;
  printf("task.%s[worker=%d]: %.6fs\n", taskName, carts_benchmarks_worker_id(),
         elapsed_sec);
  fflush(stdout);
}

///===----------------------------------------------------------------------===///
/// OMP Thread Pool Warmup
///===----------------------------------------------------------------------===///

/// Pre-warm OMP thread pool with empty parallel region.
/// Compiled with -O0 to prevent the optimizer from removing this.
#ifdef _OPENMP
__attribute__((noinline)) void carts_benchmarks_warmup_omp(void) {
#pragma omp parallel
  {
    /// Intentionally empty - forces thread pool creation
  }
}
#else
__attribute__((noinline)) void carts_benchmarks_warmup_omp(void) {
  /// No-op for non-OMP builds
}
#endif

///===----------------------------------------------------------------------===///
/// Benchmark Start Hook
///===----------------------------------------------------------------------===///

static int carts_benchmarks_is_arts_binary(void) {
  return dlsym(RTLD_DEFAULT, "artsRT") != nullptr;
}

__attribute__((noinline)) void carts_benchmarks_start(void) {
  if (carts_benchmarks_is_arts_binary())
    return;

  /// OpenMP reference path: warm up thread pool for fair comparison.
#ifdef _OPENMP
  carts_benchmarks_warmup_omp();
#endif
}

///===----------------------------------------------------------------------===///
/// Checksum/Result Reporting
///===----------------------------------------------------------------------===///

void carts_bench_checksum_d(double value) {
  printf("checksum: %.12e\n", value);
}
void carts_bench_checksum_f(float value) { printf("checksum: %.6e\n", value); }
void carts_bench_checksum_i(int value) { printf("checksum: %d\n", value); }
void carts_bench_checksum_l(long value) { printf("checksum: %ld\n", value); }
void carts_bench_rms_error(double error) {
  printf("RMS error: %.12e\n", error);
}
void carts_bench_result_named(const char *name, double value) {
  printf("%s: %.12e\n", name, value);
}
void carts_bench_sum(double value) { printf("sum: %.12e\n", value); }
void carts_bench_total(double value) { printf("total: %.12e\n", value); }
int carts_bench_verify_d(double actual, double expected, double tolerance) {
  double diff;
  if (expected == 0.0)
    diff = fabs(actual);
  else
    diff = fabs((actual - expected) / expected);

  int passed = (diff < tolerance);
  if (passed) {
    printf("Verification: PASS (checksum: %.12e)\n", actual);
  } else {
    printf(
        "Verification: FAIL (checksum: %.12e, expected: %.12e, diff: %.2e)\n",
        actual, expected, diff);
  }

  return passed ? 0 : 1;
}

} /// extern "C"
