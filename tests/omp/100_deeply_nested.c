/* 100_deeply_nested.c - parallel for with 4 levels of serial nesting inside */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 16;
    int D = N * N * N * N; /* 65536 */
    int *A = (int *)malloc(D * sizeof(int));
    int i;

    for (i = 0; i < D; i++)
        A[i] = 0;

    #pragma omp parallel for
    for (i = 0; i < N; i++) {
        int j, k, l;
        for (j = 0; j < N; j++) {
            for (k = 0; k < N; k++) {
                for (l = 0; l < N; l++) {
                    int idx = ((i * N + j) * N + k) * N + l;
                    A[idx] = i + j + k + l;
                }
            }
        }
    }

    long sum = 0;
    for (i = 0; i < D; i++)
        sum += A[i];

    /* Each dimension contributes sum 0..15 = 120, 4 dims, N^3 repeats each */
    /* sum = 4 * 120 * N^3 = 4 * 120 * 4096 = 1966080 */
    long expected = 1966080;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
