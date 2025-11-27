#include <stdio.h>
#include <stdlib.h>

static inline int clamp(int x, int lo, int hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <size>\n", argv[0]);
    return 1;
  }

  int N = atoi(argv[1]);
  if (N < 3) {
    printf("Size must be >= 3\n");
    return 1;
  }

  int *in = (int *)malloc(N * sizeof(int));
  int *out = (int *)malloc(N * sizeof(int));

  for (int i = 0; i < N; ++i)
    in[i] = i;

#pragma omp parallel for schedule(static)
  for (int i = 0; i < N; ++i) {
    int left = in[clamp(i - 1, 0, N - 1)];
    int center = in[i];
    int right = in[clamp(i + 1, 0, N - 1)];
    out[i] = left + center + right;
  }

  int checksum = 0;
  for (int i = 0; i < N; ++i)
    checksum += out[i];

  int expected_checksum = 3 * N * (N - 1) / 2;
  printf("Checksum: %d\n", checksum);
  printf("Expected checksum: %d\n", expected_checksum);

  if (checksum == expected_checksum) {
    printf("Verification: PASSED\n");
    free(in);
    free(out);
    return 0;
  } else {
    printf("Verification: FAILED\n");
    free(in);
    free(out);
    return 1;
  }
}
