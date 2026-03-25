/* 254: parallel for with negative step (decrement by 2) */
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

    /* fill even indices in reverse order */
    #pragma omp parallel for
    for (i = N - 2; i >= 0; i -= 2)
        A[i] = i + 1;

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* even indices: 0,2,4,...,N-2 -> values 1,3,5,...,N-1 */
    /* count = N/2, sum = N/2 * (1 + (N-1)) / 2 = N/2 * N/2 = N^2/4 */
    long expected = (long)N * N / 4;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
