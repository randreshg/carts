/* 59_reduction_with_private.c - reduction(+:sum) private(tmp) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = i + 1;

    int sum = 0;

    #pragma omp parallel for reduction(+:sum) private(i)
    for (i = 0; i < N; i++) {
        int tmp = A[i] * 2;
        sum += tmp;
    }

    int expected = N * (N + 1);
    printf("sum = %d (expected %d)\n", sum, expected);
    free(A);

    return (sum == expected) ? 0 : 1;
}
