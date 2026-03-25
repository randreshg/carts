/* 273: reduction with subtraction (reduction(-:var)) */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    long result = 0;

    #pragma omp parallel for reduction(-:result)
    for (int i = 0; i < N; i++)
        result -= (long)(i + 1);

    /* result = -(1+2+...+N) = -N*(N+1)/2 */
    long expected = -(long)N * (N + 1) / 2;
    printf("result = %ld (expected %ld)\n", result, expected);
    return (result != expected);
}
