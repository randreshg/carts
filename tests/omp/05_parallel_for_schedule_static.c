/* 05: schedule(static) explicit */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++) {
        A[i] = i * 2;
    }

    /* sum = sum(i*2, i=0..1023) = 2*523776 = 1047552 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 1047552)\n", sum);
    free(A);
    return (sum != 1047552);
}
