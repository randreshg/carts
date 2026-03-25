/* 283: two sequential taskloops -- second reads what first wrote */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 256;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            /* first taskloop: fill A */
            #pragma omp taskloop grainsize(32)
            for (int i = 0; i < N; i++)
                A[i] = i + 1;

            /* taskwait ensures A is ready */
            #pragma omp taskwait

            /* second taskloop: read A, write B */
            #pragma omp taskloop grainsize(32)
            for (int i = 0; i < N; i++)
                B[i] = A[i] * 3;
        }
    }

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += B[i];

    /* B[i] = 3*(i+1), sum = 3*N*(N+1)/2 */
    long expected = 3L * N * (N + 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    free(B);
    return (sum != expected);
}
