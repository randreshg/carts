/* 214: parallel with multiple for loops and code between them */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);
extern int omp_get_thread_num(void);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));
    int mid_count = 0;

    #pragma omp parallel num_threads(4)
    {
        /* first for: initialize A */
        #pragma omp for
        for (int i = 0; i < N; i++)
            A[i] = i + 1;

        /* code between fors: all threads reach here after barrier */
        #pragma omp atomic
        mid_count++;

        /* second for: B[i] = A[i] * 2 (reads A, needs barrier from first for) */
        #pragma omp for
        for (int i = 0; i < N; i++)
            B[i] = A[i] * 2;

        /* code between fors again */
        #pragma omp atomic
        mid_count++;

        /* third for: A[i] += B[i] */
        #pragma omp for
        for (int i = 0; i < N; i++)
            A[i] += B[i];
    }

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    /* A[i] = (i+1) + 2*(i+1) = 3*(i+1), sum = 3*N*(N+1)/2 */
    long expected = 3L * N * (N + 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    free(B);
    return (sum != expected);
}
