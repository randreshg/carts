/* 309: schedule(runtime) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    #pragma omp parallel for schedule(runtime)
    for (i = 0; i < N; i++)
        A[i] = i * 2;

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* sum of 2*i, i=0..N-1 = N*(N-1) */
    long expected = (long)N * (N - 1);
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
