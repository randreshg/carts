/* 61_readonly_shared.c - parallel for reads B[i], no write contention */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *B = (int *)malloc(N * sizeof(int));
    int *C = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        B[i] = i * 3;

    #pragma omp parallel for
    for (i = 0; i < N; i++)
        C[i] = B[i] + 1;

    int pass = 1;
    for (i = 0; i < N; i++) {
        if (C[i] != i * 3 + 1) {
            pass = 0;
            break;
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
    free(C);
    free(B);


    return pass ? 0 : 1;
}
