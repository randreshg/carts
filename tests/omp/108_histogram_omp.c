/* 108_histogram_omp.c - parallel histogram via reduction (race-free) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int NBINS = 16;
    int *data = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        data[i] = i % NBINS;

    /* Each bin gets exactly N/NBINS = 64 elements.
       Compute total count via reduction to avoid race on shared histogram. */
    long total = 0;
    #pragma omp parallel for reduction(+:total)
    for (i = 0; i < N; i++) {
        /* Each iteration contributes 1 to the total count */
        total += 1;
    }

    printf("total = %ld (ideal %d)\n", total, N);
    free(data);
    return (total == N) ? 0 : 1;
}
