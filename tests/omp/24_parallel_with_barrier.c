/* 24: parallel with explicit barrier between two for loops */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));

    #pragma omp parallel
    {
        #pragma omp for
        for (int i = 0; i < N; i++) {
            A[i] = i;
        }

        #pragma omp barrier

        #pragma omp for
        for (int i = 0; i < N; i++) {
            B[i] = A[N - 1 - i]; /* reverse copy -- needs A complete */
        }
    }

    /* sum = sum(1023-i, i=0..1023) = 523776 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += B[i];

    printf("sum = %ld (expected 523776)\n", sum);
    free(A);
    free(B);
    return (sum != 523776);
}
