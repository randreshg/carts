/* 259: parallel for with function call in body */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int compute(int x) {
    return x * x + 2 * x + 1; /* (x+1)^2 */
}

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (int i = 0; i < N; i++)
        A[i] = compute(i);

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    /* sum of (i+1)^2, i=0..N-1 = N*(N+1)*(2N+1)/6 */
    long expected = (long)N * (N + 1) * (2 * N + 1) / 6;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
