/* 57_reduction_in_nested_loop.c - outer for, inner parallel for with reduction */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 32;
    int *A = (int *)malloc(N * N * sizeof(int));
    int i, j;

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            A[i * N + j] = i * N + j + 1;

    long total = 0;

    for (i = 0; i < N; i++) {
        long row_sum = 0;

        #pragma omp parallel for reduction(+:row_sum)
        for (j = 0; j < N; j++)
            row_sum += A[i * N + j];

        total += row_sum;
    }

    long expected = (long)(N * N) * (long)(N * N + 1) / 2;
    printf("total = %ld (expected %ld)\n", total, expected);
    free(A);

    return (total == expected) ? 0 : 1;
}
