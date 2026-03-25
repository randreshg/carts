/* 215: parallel with for + single + for -- single block between two fors */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int scale = 0;

    #pragma omp parallel num_threads(4)
    {
        /* first for: fill A */
        #pragma omp for
        for (int i = 0; i < N; i++)
            A[i] = i + 1;

        /* single: compute scale factor (only one thread) */
        #pragma omp single
        {
            scale = 3;
        }
        /* implicit barrier after single */

        /* second for: scale A using the value computed in single */
        #pragma omp for
        for (int i = 0; i < N; i++)
            A[i] *= scale;
    }

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    /* A[i] = 3*(i+1), sum = 3*N*(N+1)/2 */
    long expected = 3L * N * (N + 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
