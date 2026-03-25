/* 08: schedule(guided) */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for schedule(guided)
    for (int i = 0; i < N; i++) {
        A[i] = N - i;
    }

    /* sum = sum(1024-i, i=0..1023) = 1024*1024 - 523776 = 524800 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 524800)\n", sum);
    free(A);
    return (sum != 524800);
}
