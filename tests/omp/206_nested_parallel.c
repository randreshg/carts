/* 206: nested parallel regions -- outer spawns inner parallel */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);
int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 256;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = 0;

    /* outer: 2 threads, each spawns inner parallel for over its half */
    #pragma omp parallel num_threads(2)
    {
        int tid = omp_get_thread_num();
        int half = N / 2;
        int start = tid * half;

        #pragma omp parallel for num_threads(2)
        for (int j = 0; j < half; j++)
            A[start + j] = start + j + 1;
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    long expected = (long)N * (N + 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
