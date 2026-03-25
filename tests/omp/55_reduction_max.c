/* 55_reduction_max.c - parallel max using omp reduction(max:) */
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

    /* Find max serially for reference */
    int expected = A[0];
    for (i = 1; i < N; i++)
        if (A[i] > expected)
            expected = A[i];

    int max_val = A[0];
    #pragma omp parallel for reduction(max:max_val)
    for (i = 1; i < N; i++) {
        if (A[i] > max_val)
            max_val = A[i];
    }

    printf("max = %d (expected %d)\n", max_val, expected);
    free(A);

    return (max_val == expected) ? 0 : 1;
}
