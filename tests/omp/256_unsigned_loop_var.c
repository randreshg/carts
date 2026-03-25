/* 256: parallel for with unsigned loop variable */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    unsigned int N = (argc > 1) ? (unsigned int)atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (unsigned int i = 0; i < N; i++)
        A[i] = (int)(i + 1);

    long sum = 0;
    for (unsigned int i = 0; i < N; i++)
        sum += A[i];

    long expected = (long)N * (N + 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
