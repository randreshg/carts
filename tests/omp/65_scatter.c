/* 65_scatter.c - A[idx[i]] = B[i] with precomputed idx (potential conflicts) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *B = (int *)malloc(N * sizeof(int));
    int *idx = (int *)malloc(N * sizeof(int));
    int i;

    /* Build a permutation to avoid conflicts */
    for (i = 0; i < N; i++) {
        B[i] = i * 3;
        idx[i] = (i * 7) % N;  /* Permutation when gcd(7,N)=1 -- not true for 1024 */
    }

    /* Use a simple permutation: reverse */
    for (i = 0; i < N; i++)
        idx[i] = N - 1 - i;

    for (i = 0; i < N; i++)
        A[i] = 0;

    #pragma omp parallel for
    for (i = 0; i < N; i++)
        A[idx[i]] = B[i];

    int pass = 1;
    for (i = 0; i < N; i++) {
        if (A[idx[i]] != B[i]) {
            pass = 0;
            break;
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    free(A);
    free(B);
    free(idx);

    return pass ? 0 : 1;
}
