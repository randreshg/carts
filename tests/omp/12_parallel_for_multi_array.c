/* 12: A[i] = B[i] + C[i] with 3 arrays */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));
    int *C = (int *)malloc(N * sizeof(int));

    for (int i = 0; i < N; i++) {
        B[i] = i;
        C[i] = i * 2;
    }

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        A[i] = B[i] + C[i];
    }

    /* sum = sum(i*3, i=0..1023) = 3*523776 = 1571328 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 1571328)\n", sum);
    free(A);
    free(B);
    free(C);
    return (sum != 1571328);
}
