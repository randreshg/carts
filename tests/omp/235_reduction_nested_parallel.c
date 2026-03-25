/* 235: nested parallel for with reduction */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 64;
    long total = 0;

    /* outer parallel for with reduction, inner serial loop */
    #pragma omp parallel for reduction(+:total)
    for (int i = 0; i < N; i++) {
        long row_sum = 0;
        for (int j = 0; j < N; j++) {
            row_sum += (long)(i * N + j);
        }
        total += row_sum;
    }

    /* sum of 0..N*N-1 = N*N*(N*N-1)/2 */
    long M = (long)N * N;
    long expected = M * (M - 1) / 2;
    printf("total = %ld (expected %ld)\n", total, expected);
    return (total != expected);
}
