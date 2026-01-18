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
#include <cstdlib>
#include <dlfcn.h>

#ifdef _OPENMP
#include <omp.h>
#endif

extern "C" {

///===----------------------------------------------------------------------===///
/// High-Precision Wall Clock Timing
///===----------------------------------------------------------------------===///

/// Get current time in nanoseconds using std::chrono::steady_clock
uint64_t carts_e2e_timer_get_time_ns(void) {
  auto now = std::chrono::steady_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      now.time_since_epoch());
  return static_cast<uint64_t>(ns.count());
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
/// Reporting Controls
///===----------------------------------------------------------------------===///

static int carts_benchmarks_should_report_init(void) {
  const char *v = std::getenv("CARTS_BENCHMARKS_REPORT_INIT");
  if (!v || !*v)
    return 0;
  return !(v[0] == '0' && v[1] == '\0');
}

///===----------------------------------------------------------------------===///
/// ARTS Init Time (process start -> entering mainBody)
///===----------------------------------------------------------------------===///

static uint64_t carts_benchmarks_process_start_ns = 0;
static int carts_benchmarks_reported_arts_init = 0;

__attribute__((constructor)) static void carts_benchmarks_ctor(void) {
  carts_benchmarks_process_start_ns = carts_e2e_timer_get_time_ns();
}

static int carts_benchmarks_is_arts_binary(void) {
  /// OpenMP reference binaries do not link against ARTS, but CARTS-generated
  /// binaries do. Use symbol presence as a robust discriminator even when
  /// `artsConfig` env var is not set (e.g., using ./arts.cfg in CWD).
  return dlsym(RTLD_DEFAULT, "artsRT") != nullptr;
}

__attribute__((noinline)) static void carts_benchmarks_report_arts_init(void) {
  if (!carts_benchmarks_should_report_init())
    return;
  if (__sync_lock_test_and_set(&carts_benchmarks_reported_arts_init, 1))
    return;

  uint64_t start_ns = carts_benchmarks_process_start_ns;
  if (!start_ns)
    start_ns = carts_e2e_timer_get_time_ns();
  uint64_t now_ns = carts_e2e_timer_get_time_ns();
  double elapsed_sec = (double)(now_ns - start_ns) * 1e-9;
  printf("init.arts: %.9fs\n", elapsed_sec);
  fflush(stdout);
}

///===----------------------------------------------------------------------===///
/// Benchmark Start Hook
///===----------------------------------------------------------------------===///

__attribute__((noinline)) void carts_benchmarks_start(void) {
  if (carts_benchmarks_is_arts_binary()) {
    carts_benchmarks_report_arts_init();
    return;
  }

  /// OpenMP reference path. Safe as a no-op when OpenMP is not enabled.
#ifdef _OPENMP
  uint64_t t0 = carts_e2e_timer_get_time_ns();
  carts_benchmarks_warmup_omp();
  uint64_t t1 = carts_e2e_timer_get_time_ns();
  if (carts_benchmarks_should_report_init()) {
    double elapsed_sec = (double)(t1 - t0) * 1e-9;
    printf("init.omp: %.9fs\n", elapsed_sec);
    fflush(stdout);
  }
#else
  (void)0;
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
