/* 62_writeonly_disjoint.c - A[i] = i*2 (disjoint writes) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    #pragma omp parallel for
    for (i = 0; i < N; i++)
        A[i] = i * 2;

    int pass = 1;
    for (i = 0; i < N; i++) {
        if (A[i] != i * 2) {
            pass = 0;
            break;
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    free(A);

    return pass ? 0 : 1;
}
