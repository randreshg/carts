/* 301: #pragma omp for ordered (inside parallel region) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 32;
    int *A = (int *)malloc(N * sizeof(int));
    int pos = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp for ordered schedule(static,1)
        for (int i = 0; i < N; i++) {
            int val = i * 3;
            #pragma omp ordered
            {
                A[pos] = val;
                pos++;
            }
        }
    }

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    /* sum of 3*i, i=0..N-1 = 3 * N*(N-1)/2 */
    long expected = 3L * N * (N - 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);

    int ordered = 1;
    for (int i = 0; i < N; i++) {
        if (A[i] != i * 3) {
            ordered = 0;
            break;
        }
    }
    printf("ordered = %d\n", ordered);
    free(A);
    return (sum != expected) || !ordered;
}
