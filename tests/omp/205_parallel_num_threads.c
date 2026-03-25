/* 205: parallel with varying num_threads -- two regions with different counts */
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

    int nt1 = 0, nt2 = 0;

    /* first region: 2 threads */
    #pragma omp parallel num_threads(2)
    {
        int tid = omp_get_thread_num();
        #pragma omp single
        nt1 = omp_get_num_threads();
        A[tid] = 10;
    }

    /* second region: 4 threads -- accumulates on top */
    #pragma omp parallel num_threads(4)
    {
        int tid = omp_get_thread_num();
        #pragma omp single
        nt2 = omp_get_num_threads();
        A[tid] += 5;
    }

    long sum = 0;
    int max = nt1 > nt2 ? nt1 : nt2;
    for (i = 0; i < max; i++)
        sum += A[i];

    /* A[0]=15, A[1]=15, A[2]=5, A[3]=5 -> sum=40 */
    long expected = 10L * nt1 + 5L * nt2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    return (sum != expected);
}
