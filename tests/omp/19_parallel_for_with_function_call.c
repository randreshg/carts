/* 19: call a helper function inside parallel for */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

static int compute(int x) {
    return x * x + 2 * x + 1; /* (x+1)^2 */
}

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        A[i] = compute(i);
    }

    /* sum = sum((i+1)^2, i=0..1023) = sum(k^2, k=1..1024) = 1024*1025*2049/6 = 358438400 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 358438400)\n", sum);
    free(A);
    return (sum != 358438400);
}
