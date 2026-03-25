/* 284: parallel for with 2D array using flat allocation and row-major access */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 64;
    int *mat = (int *)malloc(N * N * sizeof(int));

    /* parallel over rows, serial over columns */
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            mat[i * N + j] = i * N + j;
        }
    }

    long sum = 0;
    int total = N * N;
    for (int idx = 0; idx < total; idx++)
        sum += mat[idx];

    /* sum = 0+1+2+...+(N*N-1) = N*N*(N*N-1)/2 */
    long M = (long)N * N;
    long expected = M * (M - 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(mat);
    return (sum != expected);
}
