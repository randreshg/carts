/* 257: parallel for with long long loop variable */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    long long N = (argc > 1) ? (long long)atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (long long i = 0; i < N; i++)
        A[i] = (int)(i + 1);

    long long sum = 0;
    for (long long i = 0; i < N; i++)
        sum += A[i];

    long long expected = N * (N + 1) / 2;
    printf("sum = %lld (expected %lld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
