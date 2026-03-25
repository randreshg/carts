/* 38_taskwait.c - spawn 4 tasks, taskwait, check results */
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
            int chunk = N / 4;

            #pragma omp task firstprivate(chunk)
            {
                int j;
                for (j = 0; j < chunk; j++)
                    A[j] = 1;
            }

            #pragma omp task firstprivate(chunk)
            {
                int j;
                for (j = chunk; j < 2 * chunk; j++)
                    A[j] = 2;
            }

            #pragma omp task firstprivate(chunk)
            {
                int j;
                for (j = 2 * chunk; j < 3 * chunk; j++)
                    A[j] = 3;
            }

            #pragma omp task firstprivate(chunk)
            {
                int j;
                for (j = 3 * chunk; j < N; j++)
                    A[j] = 4;
            }

            #pragma omp taskwait
        }
    }

    int sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    int expected = (N / 4) * (1 + 2 + 3 + 4);
    printf("sum = %d (expected %d)\n", sum, expected);
    free(A);

    return (sum == expected) ? 0 : 1;
}
