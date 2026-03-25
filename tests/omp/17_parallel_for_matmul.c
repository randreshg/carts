/* 17: matrix multiply C[i][j] += A[i][k]*B[k][j] (flattened) */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 64; /* smaller for O(N^3) */
    int *A = (int *)malloc(N * N * sizeof(int));
    int *B = (int *)malloc(N * N * sizeof(int));
    int *C = (int *)malloc(N * N * sizeof(int));

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i * N + j] = (i == j) ? 1 : 0; /* identity */
            B[i * N + j] = i + j;
            C[i * N + j] = 0;
        }
    }

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                C[i * N + j] += A[i * N + k] * B[k * N + j];
            }
        }
    }

    /* C = I * B = B, sum of (i+j) over 64x64 = 2*64*2016 = 258048 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            sum += C[i * N + j];

    printf("sum = %ld (expected 258048)\n", sum);
    free(A);
    free(B);
    free(C);
    return (sum != 258048);
}
