/* 106_axpy.c - Y[i] = a*X[i] + Y[i] */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    double *X = (double *)malloc(N * sizeof(double));
    double *Y = (double *)malloc(N * sizeof(double));
    double a = 2.5;
    int i;

    for (i = 0; i < N; i++) {
        X[i] = (double)i;
        Y[i] = (double)(i * 3);
    }

    #pragma omp parallel for
    for (i = 0; i < N; i++)
        Y[i] = a * X[i] + Y[i];

    double sum = 0.0;
    for (i = 0; i < N; i++)
        sum += Y[i];

    /* Y[i] = 2.5*i + 3*i = 5.5*i, sum = 5.5 * 1023*1024/2 = 5.5 * 523776 = 2880768 */
    double expected = 2880768.0;
    printf("sum = %f (expected %f)\n", sum, expected);
    free(X);
    free(Y);
    double diff = sum - expected;
    if (diff < 0) diff = -diff;
    return (diff > 0.1);
}
