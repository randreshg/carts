/* 18: two consecutive parallel for regions */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        A[i] = i * 2;
    }

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        B[i] = A[i] + 1;
    }

    /* sum = sum(i*2+1, i=0..1023) = 2*523776 + 1024 = 1048576 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += B[i];

    printf("sum = %ld (expected 1048576)\n", sum);
    free(A);
    free(B);
    return (sum != 1048576);
}
