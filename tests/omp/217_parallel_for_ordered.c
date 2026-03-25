/* 217: parallel for with ordered clause */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 64;
    int *A = (int *)malloc(N * sizeof(int));
    int pos = 0;

    #pragma omp parallel for ordered schedule(static,1) num_threads(4)
    for (int i = 0; i < N; i++) {
        int val = i * i;
        #pragma omp ordered
        {
            A[pos] = val;
            pos++;
        }
    }

    /* A should contain 0, 1, 4, 9, 16, ... in order */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    /* sum of i^2, i=0..N-1 = N*(N-1)*(2N-1)/6 */
    long expected = (long)N * (N - 1) * (2 * N - 1) / 6;
    printf("sum = %ld (expected %ld)\n", sum, expected);

    /* also verify order */
    int ordered = 1;
    for (int i = 0; i < N; i++) {
        if (A[i] != i * i) {
            ordered = 0;
            break;
        }
    }

    printf("ordered = %d\n", ordered);
    free(A);
    return (sum != expected) || !ordered;
}
