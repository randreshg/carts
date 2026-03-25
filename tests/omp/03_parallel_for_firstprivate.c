/* 03: firstprivate(scale) read-only captured */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int scale = 7;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for firstprivate(scale)
    for (int i = 0; i < N; i++) {
        A[i] = i * scale;
    }

    /* sum = sum(i*7, i=0..1023) = 7*523776 = 3666432 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 3666432)\n", sum);
    free(A);
    return (sum != 3666432);
}
