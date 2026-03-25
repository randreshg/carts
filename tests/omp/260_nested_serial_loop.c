/* 260: parallel for with nested serial loop in body */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 128;
    int M = 16;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        int acc = 0;
        for (int j = 0; j < M; j++)
            acc += j;
        A[i] = acc; /* M*(M-1)/2 = 120 */
    }

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    long inner_sum = (long)M * (M - 1) / 2;
    long expected = inner_sum * N;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
