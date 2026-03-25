/* 86_fork_join.c - single spawns 8 tasks, taskwait, check all results */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int NTASKS = 8;
    int CHUNK = 128;
    int N = NTASKS * CHUNK;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    #pragma omp parallel
    {
        #pragma omp single
        {
            int t;
            for (t = 0; t < NTASKS; t++) {
                int start = t * CHUNK;
                #pragma omp task firstprivate(start)
                {
                    int j;
                    for (j = start; j < start + CHUNK; j++)
                        A[j] = j * 3;
                }
            }
            #pragma omp taskwait
        }
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* sum = 3*(0+1+...+1023) = 3*1023*1024/2 = 1571328 */
    long expected = 1571328;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
