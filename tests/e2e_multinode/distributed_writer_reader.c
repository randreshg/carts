// RUN: %carts compile %s -O3 --arts-config %arts_multinode_config -o %t_arts
// RUN: env ARTS_CONFIG=%arts_multinode_config artsConfig=%arts_multinode_config %t_arts | %FileCheck %s
// CHECK: [CARTS] distributed writer-reader: PASS

#include <stdio.h>
#include <stdlib.h>

int main(void) {
  const int n = 4096;
  int *written = (int *)malloc((unsigned long)n * sizeof(int));
  int *observed = (int *)malloc((unsigned long)n * sizeof(int));
  if (!written || !observed) {
    printf("[CARTS] distributed writer-reader: FAIL alloc\n");
    free(written);
    free(observed);
    return 1;
  }

#pragma omp parallel for
  for (int i = 0; i < n; ++i)
    written[i] = i + 1;

#pragma omp parallel for
  for (int i = 0; i < n; ++i) {
    int next = (i + 1 == n) ? 0 : i + 1;
    observed[i] = written[i] + written[next];
  }

  long long checksum = 0;
  for (int i = 0; i < n; ++i)
    checksum += observed[i];

  long long expected = (long long)n * (n + 1);
  if (checksum != expected) {
    printf(
        "[CARTS] distributed writer-reader: FAIL checksum=%lld expected=%lld\n",
        checksum, expected);
    free(written);
    free(observed);
    return 1;
  }

  printf("[CARTS] distributed writer-reader: PASS checksum=%lld\n", checksum);
  free(written);
  free(observed);
  return 0;
}
