/* 101_gemm.c - general matrix multiply C[i][j] += A[i][k]*B[k][j] */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 64;
    double *A = (double *)malloc(N * N * sizeof(double));
    double *B = (double *)malloc(N * N * sizeof(double));
    double *C = (double *)malloc(N * N * sizeof(double));
    int i, j, k;

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            A[i * N + j] = (double)(i + j);
            B[i * N + j] = (double)(i - j);
            C[i * N + j] = 0.0;
        }

    #pragma omp parallel for private(j, k)
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            for (k = 0; k < N; k++)
                C[i * N + j] += A[i * N + k] * B[k * N + j];

    double checksum = 0.0;
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            checksum += C[i * N + j];

    printf("checksum = %f\n", checksum);
    free(A);
    free(B);
    free(C);
    return 0;
}
