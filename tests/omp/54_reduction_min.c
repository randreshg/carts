/* 54_reduction_min.c - parallel min using omp reduction(min:) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = (i * 37 + 13) % 10000;

    /* Find min serially for reference */
    int expected = A[0];
    for (i = 1; i < N; i++)
        if (A[i] < expected)
            expected = A[i];

    int min_val = A[0];
    #pragma omp parallel for reduction(min:min_val)
    for (i = 1; i < N; i++) {
        if (A[i] < min_val)
            min_val = A[i];
    }

    printf("min = %d (expected %d)\n", min_val, expected);
    free(A);

    return (min_val == expected) ? 0 : 1;
}
