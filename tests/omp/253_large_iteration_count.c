/* 253: very large iteration count */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1000000;
    long sum = 0;

    #pragma omp parallel for reduction(+:sum)
    for (int i = 0; i < N; i++)
        sum += i;

    long expected = (long)N * (N - 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    return (sum != expected);
}
