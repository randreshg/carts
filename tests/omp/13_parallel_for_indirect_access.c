/* 13: A[i] = B[idx[i]] gather pattern */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A   = (int *)malloc(N * sizeof(int));
    int *B   = (int *)malloc(N * sizeof(int));
    int *idx = (int *)malloc(N * sizeof(int));

    for (int i = 0; i < N; i++) {
        B[i] = i * 10;
        idx[i] = N - 1 - i; /* reverse mapping */
    }

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        A[i] = B[idx[i]];
    }

    /* sum = sum((1023-i)*10, i=0..1023) = 10*523776 = 5237760 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 5237760)\n", sum);
    free(A);
    free(B);
    free(idx);
    return (sum != 5237760);
}
