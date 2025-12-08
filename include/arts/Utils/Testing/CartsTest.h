///===----------------------------------------------------------------------===///
/// File: CartsTest.h
///
/// C test utilities for CARTS examples.
/// Provides consistent test status reporting and timing.
///
/// Usage:
///   #include "arts/Utils/Testing/CartsTest.h"
///
///   int main(int argc, char *argv[]) {
///       CARTS_TIMER_START();
///       // ... computation ...
///       if (result == expected) {
///           CARTS_TEST_PASS();
///       } else {
///           CARTS_TEST_FAIL("result mismatch");
///       }
///   }
///
/// Output format:
///   [CARTS] example_name: PASS (0.1234s)
///   [CARTS] example_name: FAIL - reason
///
/// Note: Link with libcartstest (-lcartstest) for the implementation.
///===----------------------------------------------------------------------===///

#ifndef CARTS_TEST_H
#define CARTS_TEST_H

#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

///===----------------------------------------------------------------------===///
/// Timing Utilities (inline - simple enough for Polygeist)
///===----------------------------------------------------------------------===///

/// Global timer start value
static double _carts_timer_start = 0.0;

/// Get current wall-clock time in seconds.
static inline double carts_get_time(void) {
#ifdef _OPENMP
  return omp_get_wtime();
#else
  return (double)clock() / CLOCKS_PER_SEC;
#endif
}

/// Start the timer
#define CARTS_TIMER_START() _carts_timer_start = carts_get_time()

/// Get elapsed time since CARTS_TIMER_START()
#define CARTS_TIMER_STOP() (carts_get_time() - _carts_timer_start)

///===----------------------------------------------------------------------===///
/// Result Reporting (implemented in libcartstest)
///===----------------------------------------------------------------------===///

/// Extract test name from __FILE__ path (library function)
const char *carts_extract_test_name(const char *path);

/// Report test pass with optional timing (library function)
int carts_test_pass_impl(const char *file, double elapsed);

/// Report test failure with reason (library function)
int carts_test_fail_impl(const char *file, const char *reason);

/// Report test pass with timing and exit
#define CARTS_TEST_PASS()                                                      \
  return carts_test_pass_impl(__FILE__, CARTS_TIMER_STOP())

/// Report test pass without timing
#define CARTS_TEST_PASS_NO_TIME() return carts_test_pass_impl(__FILE__, -1)

/// Report test failure with reason and exit
#define CARTS_TEST_FAIL(reason) return carts_test_fail_impl(__FILE__, reason)

#ifdef __cplusplus
}
#endif

#endif // CARTS_TEST_H
