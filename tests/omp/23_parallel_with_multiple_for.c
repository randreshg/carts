/* 23: parallel region with two worksharing for loops */

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
            A[i] = i * 2;
        }

        #pragma omp for
        for (int i = 0; i < N; i++) {
            B[i] = A[i] + 3;
        }
    }

    /* sum = sum(i*2+3, i=0..1023) = 2*523776 + 3072 = 1050624 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += B[i];

    printf("sum = %ld (expected 1050624)\n", sum);
    free(A);
    free(B);
    return (sum != 1050624);
}
