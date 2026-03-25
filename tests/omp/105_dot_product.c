/* 105_dot_product.c - dot product with reduction */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    double *A = (double *)malloc(N * sizeof(double));
    double *B = (double *)malloc(N * sizeof(double));
    int i;

    for (i = 0; i < N; i++) {
        A[i] = (double)i;
        B[i] = (double)(N - i);
    }

    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (i = 0; i < N; i++)
        sum += A[i] * B[i];

    /* sum = sum_{i=0}^{1023} i*(1024-i) = 1024*sum(i) - sum(i^2) */
    /* = 1024*523776 - 357389824 = 536346624 - 357389824 = 178956800 */
    double expected = 178956800.0;
    printf("dot = %f (expected %f)\n", sum, expected);
    free(A);
    free(B);
    double diff = sum - expected;
    if (diff < 0) diff = -diff;
    return (diff > 0.1);
}
