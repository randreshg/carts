/* 213: parallel with code before AND after for */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int pre[16], post[16];
    int i;

    for (i = 0; i < 16; i++) {
        pre[i] = 0;
        post[i] = 0;
    }

    int nthreads = 0;

    #pragma omp parallel num_threads(4)
    {
        int tid = omp_get_thread_num();
        #pragma omp single
        nthreads = omp_get_num_threads();

        /* pre-for code */
        pre[tid] = tid + 1;

        #pragma omp for
        for (int j = 0; j < N; j++)
            A[j] = j + 1;

        /* post-for code: can read A because barrier after for */
        post[tid] = A[tid];
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    long pre_sum = 0, post_sum = 0;
    for (i = 0; i < nthreads; i++) {
        pre_sum += pre[i];
        post_sum += post[i];
    }

    long expected = (long)N * (N + 1) / 2;
    long pre_expected = (long)nthreads * (nthreads + 1) / 2;
    /* post[tid] = A[tid] = tid+1, same as pre */
    long post_expected = pre_expected;

    int ok = (sum == expected) && (pre_sum == pre_expected) &&
             (post_sum == post_expected);
    printf("sum=%ld pre=%ld post=%ld (expected %ld/%ld/%ld)\n",
           sum, pre_sum, post_sum, expected, pre_expected, post_expected);
    return ok ? 0 : 1;
}
