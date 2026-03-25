/* 94_negative_step.c - parallel for with descending index */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    #pragma omp parallel for
    for (i = N - 1; i >= 0; i--)
        A[i] = N - 1 - i;

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* A[i] = N-1-i, sum = 0+1+...+1023 = 523776 */
    long expected = 523776;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
