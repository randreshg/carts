/* 16: 5-point 2D stencil on NxN array (flattened) */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 64; /* smaller for 2D */
    int *A = (int *)malloc(N * N * sizeof(int));
    int *B = (int *)malloc(N * N * sizeof(int));

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            B[i * N + j] = 5; /* constant so stencil = 5 */

    #pragma omp parallel for
    for (int i = 1; i < N - 1; i++) {
        for (int j = 1; j < N - 1; j++) {
            A[i * N + j] = (B[(i - 1) * N + j] +
                            B[(i + 1) * N + j] +
                            B[i * N + (j - 1)] +
                            B[i * N + (j + 1)] +
                            B[i * N + j]) / 5;
        }
    }

    /* interior 62x62 elements all = 5, sum = 62*62*5 = 19220 */
    long sum = 0;
    for (int i = 1; i < N - 1; i++)
        for (int j = 1; j < N - 1; j++)
            sum += A[i * N + j];

    printf("sum = %ld (expected 19220)\n", sum);
    free(A);
    free(B);
    return (sum != 19220);
}
