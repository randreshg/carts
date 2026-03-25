/* 303: taskgroup with control flow inside (tests ExecuteRegionOp wrapper) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 16;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = 0;

    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            #pragma omp taskgroup
            {
                for (int j = 0; j < N; j++) {
                    #pragma omp task firstprivate(j)
                    {
                        /* control flow inside task to exercise taskgroup's
                           ExecuteRegionOp with multi-block body */
                        if (j % 2 == 0)
                            A[j] = j * 2;
                        else
                            A[j] = j * 2 + 1;
                    }
                }
            }
        }
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    long expected = 0;
    for (i = 0; i < N; i++)
        expected += (i % 2 == 0) ? i * 2 : i * 2 + 1;

    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
