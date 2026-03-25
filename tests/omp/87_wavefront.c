/* 87_wavefront.c - 2D wavefront via task depends */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 32;
    int *A = (int *)malloc(N * N * sizeof(int));
    int i, j;

    for (i = 0; i < N * N; i++)
        A[i] = 0;

    #pragma omp parallel
    {
        #pragma omp single
        {
            for (i = 0; i < N; i++) {
                for (j = 0; j < N; j++) {
                    #pragma omp task depend(out: A[i*N+j]) \
                        depend(in: A[(i-1)*N+j]) depend(in: A[i*N+(j-1)]) \
                        firstprivate(i, j)
                    {
                        int top = (i > 0) ? A[(i - 1) * N + j] : 0;
                        int left = (j > 0) ? A[i * N + (j - 1)] : 0;
                        A[i * N + j] = top + left + 1;
                    }
                }
            }
            #pragma omp taskwait
        }
    }

    /* A[i][j] = C(i+j, i) which is binomial coefficient, but simpler: each cell = top+left+1 */
    /* A[0][0]=1, A[N-1][N-1] can be computed. Just verify corners. */
    int ok = (A[0] == 1);
    /* A[0][j] = j+1, A[i][0] = i+1 */
    ok = ok && (A[N - 1] == N);
    ok = ok && (A[(N - 1) * N] == N);
    printf("A[0][0]=%d A[0][%d]=%d A[%d][0]=%d ok=%d\n",
           A[0], N - 1, A[N - 1], N - 1, A[(N - 1) * N], ok);
    free(A);
    return !ok;
}
