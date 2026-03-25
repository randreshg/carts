/* 21: parallel region -- each thread sets A[omp_get_thread_num()] */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);
extern int omp_get_num_threads(void);

int main(int argc, char **argv) {
    int A[16]; /* max threads we expect */

    for (int i = 0; i < 16; i++)
        A[i] = -1;

    int nthreads = 0;

    #pragma omp parallel num_threads(4)
    {
        int tid = omp_get_thread_num();
        A[tid] = tid * 10;
        #pragma omp single
        {
            nthreads = omp_get_num_threads();
        }
    }

    /* sum = 10 * nthreads*(nthreads-1)/2; with 4 threads = 60 */
    long sum = 0;
    for (int i = 0; i < nthreads; i++)
        sum += A[i];

    long expected = 10L * nthreads * (nthreads - 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    return (sum != expected);
}
