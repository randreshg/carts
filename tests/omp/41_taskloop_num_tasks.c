/* 41_taskloop_num_tasks.c - taskloop num_tasks(8) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = 0;

    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp taskloop num_tasks(8)
            for (i = 0; i < N; i++)
                A[i] = i + 1;
        }
    }

    int sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    int expected = N * (N + 1) / 2;
    printf("sum = %d (expected %d)\n", sum, expected);
    free(A);

    return (sum == expected) ? 0 : 1;
}
