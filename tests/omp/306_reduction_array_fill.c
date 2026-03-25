/* 306: array element reduction (tests fillMemRefWithScalar for element init) */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int sums[4] = {0, 0, 0, 0};

    #pragma omp parallel for reduction(+:sums)
    for (int i = 0; i < N; i++) {
        sums[i % 4] += i;
    }

    int expected[4] = {0, 0, 0, 0};
    for (int i = 0; i < N; i++)
        expected[i % 4] += i;

    printf("sums = [%d, %d, %d, %d]\n", sums[0], sums[1], sums[2], sums[3]);
    printf("expected = [%d, %d, %d, %d]\n",
           expected[0], expected[1], expected[2], expected[3]);

    int ok = 1;
    for (int i = 0; i < 4; i++)
        if (sums[i] != expected[i]) ok = 0;
    return !ok;
}
