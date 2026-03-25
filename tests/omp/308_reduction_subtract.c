/* 308: subtraction reduction */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int result = 0;

    #pragma omp parallel for reduction(-:result)
    for (int i = 0; i < N; i++)
        result -= i;

    int expected = 0;
    for (int i = 0; i < N; i++)
        expected -= i;

    printf("result = %d (expected %d)\n", result, expected);
    return (result != expected);
}
