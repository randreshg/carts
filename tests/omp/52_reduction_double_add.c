/* 52_reduction_double_add.c - double sum reduction(+:sum) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    double *A = (double *)malloc(N * sizeof(double));
    int i;

    for (i = 0; i < N; i++)
        A[i] = (double)(i + 1);

    double sum = 0.0;

    #pragma omp parallel for reduction(+:sum)
    for (i = 0; i < N; i++)
        sum += A[i];

    double expected = (double)N * (double)(N + 1) / 2.0;
    printf("sum = %.1f (expected %.1f)\n", sum, expected);
    free(A);

    return (sum == expected) ? 0 : 1;
}
