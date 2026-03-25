/* 25: parallel with master for init, then worksharing for */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int offset = 42;

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        A[i] = i + offset;
    }

    /* sum = sum(i+42, i=0..1023) = 523776 + 43008 = 566784 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 566784)\n", sum);
    free(A);
    return (sum != 566784);
}
