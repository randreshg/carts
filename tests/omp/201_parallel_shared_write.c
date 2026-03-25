/* 201: parallel region -- all threads write to shared array via thread id */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
    int A[64];
    int i;

    for (i = 0; i < 64; i++)
        A[i] = 0;

    int nthreads = 0;

    #pragma omp parallel num_threads(8)
    {
        int tid = omp_get_thread_num();
        #pragma omp single
        nthreads = omp_get_num_threads();

        /* each thread writes to its own slot -- no race */
        A[tid] = (tid + 1) * (tid + 1);
    }

    long sum = 0;
    for (i = 0; i < nthreads; i++)
        sum += A[i];

    /* sum of squares: 1^2 + 2^2 + ... + n^2 = n(n+1)(2n+1)/6 */
    long expected = (long)nthreads * (nthreads + 1) * (2 * nthreads + 1) / 6;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    return (sum != expected);
}
