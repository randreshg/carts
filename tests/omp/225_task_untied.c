/* 225: task with untied clause -- task may migrate between threads */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 16;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            for (int j = 0; j < N; j++) {
                #pragma omp task untied firstprivate(j)
                {
                    A[j] = j * j + 1;
                }
            }
        }
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* sum of (i^2+1), i=0..N-1 = N*(N-1)*(2N-1)/6 + N */
    long expected = (long)N * (N - 1) * (2 * N - 1) / 6 + N;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
