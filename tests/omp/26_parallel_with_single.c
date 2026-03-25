/* 26: parallel with single for init, then worksharing for */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int multiplier = 0;

    #pragma omp parallel
    {
        #pragma omp single
        {
            multiplier = 5;
        }
        /* single has an implicit barrier at exit */

        #pragma omp for
        for (int i = 0; i < N; i++) {
            A[i] = i * multiplier;
        }
    }

    /* sum = sum(i*5, i=0..1023) = 5*523776 = 2618880 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 2618880)\n", sum);
    free(A);
    return (sum != 2618880);
}
