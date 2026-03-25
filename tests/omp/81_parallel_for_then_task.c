/* 81_parallel_for_then_task.c - parallel for on A, then single+task writes B */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        B[i] = 0;

    #pragma omp parallel for
    for (i = 0; i < N; i++)
        A[i] = i * 2;

    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp task
            {
                int j;
                for (j = 0; j < N; j++)
                    B[j] = A[j] + 1;
            }
            #pragma omp taskwait
        }
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += B[i];

    /* B[i] = 2*i + 1, sum = sum(2i+1) = 2*1023*1024/2 + 1024 = 1047552 + 1024 = 1048576 */
    long expected = 1048576;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    free(B);
    return (sum != expected);
}
