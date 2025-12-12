///===----------------------------------------------------------------------===///
/// File: CartsBenchmarks.c
///
/// Implementation of CARTS benchmark utilities.
///===----------------------------------------------------------------------===///

#include <stdio.h>
#include <math.h>

/// Print checksum as double
void carts_bench_checksum_d(double value) {
  printf("checksum: %.12e\n", value);
}

/// Print checksum as float
void carts_bench_checksum_f(float value) {
  printf("checksum: %.6e\n", value);
}

/// Print checksum as integer
void carts_bench_checksum_i(int value) {
  printf("checksum: %d\n", value);
}

/// Print checksum as long
void carts_bench_checksum_l(long value) {
  printf("checksum: %ld\n", value);
}

/// Print RMS error
void carts_bench_rms_error(double error) {
  printf("RMS error: %.12e\n", error);
}

/// Print a named result value
void carts_bench_result_named(const char *name, double value) {
  printf("%s: %.12e\n", name, value);
}

/// Print sum
void carts_bench_sum(double value) {
  printf("sum: %.12e\n", value);
}

/// Print total
void carts_bench_total(double value) {
  printf("total: %.12e\n", value);
}

/// Verify result against expected value with tolerance
int carts_bench_verify_d(double actual, double expected, double tolerance) {
  double diff;
  if (expected == 0.0) {
    diff = fabs(actual);
  } else {
    diff = fabs((actual - expected) / expected);
  }

  int passed = (diff < tolerance);

  if (passed) {
    printf("Verification: PASS (checksum: %.12e)\n", actual);
  } else {
    printf("Verification: FAIL (checksum: %.12e, expected: %.12e, diff: %.2e)\n",
           actual, expected, diff);
  }

  return passed ? 0 : 1;
}
