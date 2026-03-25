/* 261: parallel for with if clause -- conditional parallelism */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));

    /* if(1): runs in parallel */
    #pragma omp parallel for if(N > 16)
    for (int i = 0; i < N; i++)
        A[i] = i + 1;

    /* if(0): runs serially (N>16 is true for default, test both paths) */
    int small = 4;
    #pragma omp parallel for if(small > 16)
    for (int i = 0; i < N; i++)
        B[i] = i * 2;

    long sumA = 0, sumB = 0;
    for (int i = 0; i < N; i++) {
        sumA += A[i];
        sumB += B[i];
    }

    long expA = (long)N * (N + 1) / 2;
    long expB = (long)N * (N - 1);
    printf("sumA = %ld (expected %ld), sumB = %ld (expected %ld)\n",
           sumA, expA, sumB, expB);
    free(A);
    free(B);
    return (sumA != expA) || (sumB != expB);
}
