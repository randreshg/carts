/* 216: parallel with for + master + barrier + for */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int offset = 0;

    #pragma omp parallel num_threads(4)
    {
        /* first for */
        #pragma omp for
        for (int i = 0; i < N; i++)
            A[i] = i;

        /* master sets offset -- no implicit barrier after master */
        #pragma omp master
        {
            offset = 100;
        }

        /* explicit barrier so all threads see offset=100 */
        #pragma omp barrier

        /* second for: uses offset */
        #pragma omp for
        for (int i = 0; i < N; i++)
            A[i] += offset;
    }

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    /* A[i] = i + 100, sum = N*(N-1)/2 + 100*N */
    long expected = (long)N * (N - 1) / 2 + 100L * N;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
