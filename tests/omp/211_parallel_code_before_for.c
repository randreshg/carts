/* 211: parallel with code before for -- all threads execute pre-for code */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int counts[16];
    int i;

    for (i = 0; i < 16; i++)
        counts[i] = 0;
    for (i = 0; i < N; i++)
        A[i] = 0;

    int nthreads = 0;

    #pragma omp parallel num_threads(4)
    {
        int tid = omp_get_thread_num();
        #pragma omp single
        nthreads = omp_get_num_threads();

        /* code before for: all threads execute this */
        counts[tid] = 1;

        #pragma omp for
        for (int j = 0; j < N; j++)
            A[j] = j + 1;
    }

    /* verify all threads ran the pre-for code */
    int thread_count = 0;
    for (i = 0; i < nthreads; i++)
        thread_count += counts[i];

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    long expected = (long)N * (N + 1) / 2;
    int ok = (sum == expected) && (thread_count == nthreads);
    printf("sum = %ld (expected %ld), threads = %d/%d\n",
           sum, expected, thread_count, nthreads);
    return ok ? 0 : 1;
}
