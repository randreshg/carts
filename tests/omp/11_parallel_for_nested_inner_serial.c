/* 11: outer parallel for, inner serial for */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int M = 8;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        int acc = 0;
        for (int j = 0; j < M; j++) {
            acc += j;
        }
        A[i] = acc; /* M*(M-1)/2 = 28 */
    }

    /* sum = 28 * 1024 = 28672 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 28672)\n", sum);
    free(A);
    return (sum != 28672);
}
