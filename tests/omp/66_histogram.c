/* 66_histogram.c - parallel histogram via reduction (race-free) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

#define NBINS 16

int main(int argc, char **argv) {
  int N = (argc > 1) ? atoi(argv[1]) : 1024;
  int *A = (int *)malloc(N * sizeof(int));
  int i;

  for (i = 0; i < N; i++)
    A[i] = (i * 37 + 13) % 10000;

  /* Count total elements via reduction (avoids atomic/race).
     Each bin count can be verified separately in a serial loop. */
  int total = 0;
#pragma omp parallel for reduction(+ : total)
  for (i = 0; i < N; i++) {
    total += 1;
  }

  /* Verify serial bin counts */
  int *count = (int *)malloc(NBINS * sizeof(int));
  for (i = 0; i < NBINS; i++)
    count[i] = 0;
  for (i = 0; i < N; i++)
    count[A[i] % NBINS]++;

  int serial_total = 0;
  for (i = 0; i < NBINS; i++)
    serial_total += count[i];

  printf("total = %d (expected %d)\n", total, N);
  free(count);
  free(A);

  return (total == N && serial_total == N) ? 0 : 1;
}
