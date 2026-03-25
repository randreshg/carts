/* 72_early_continue.c - parallel for with continue to skip iterations */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = 0;

    #pragma omp parallel for
    for (i = 0; i < N; i++) {
        if (i % 3 == 0)
            continue;
        A[i] = i;
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* Sum of i where i%3 != 0, i in [0,1023] */
    /* Total sum = 1023*1024/2 = 523776 */
    /* Sum of multiples of 3: 0+3+6+...+1023 = 3*(0+1+...+341) = 3*341*342/2 = 174933 */
    /* Expected = 523776 - 174933 = 348843 */
    long expected = 348843;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
