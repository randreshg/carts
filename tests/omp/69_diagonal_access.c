/* 69_diagonal_access.c - A[i*N+i] = val (diagonal only) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 64;
    int *A = (int *)malloc(N * N * sizeof(int));
    int i;

    for (i = 0; i < N * N; i++)
        A[i] = 0;

    #pragma omp parallel for
    for (i = 0; i < N; i++)
        A[i * N + i] = i + 1;

    /* Verify diagonal */
    int pass = 1;
    for (i = 0; i < N; i++) {
        if (A[i * N + i] != i + 1) {
            pass = 0;
            break;
        }
    }

    /* Verify off-diagonal is zero */
    int j;
    for (i = 0; i < N && pass; i++)
        for (j = 0; j < N && pass; j++)
            if (i != j && A[i * N + j] != 0)
                pass = 0;

    printf("%s\n", pass ? "PASS" : "FAIL");
    free(A);

    return pass ? 0 : 1;
}
