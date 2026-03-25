/* 305: XOR reduction */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;

    int xor_result = 0;

    #pragma omp parallel for reduction(^:xor_result)
    for (int i = 0; i < N; i++) {
        xor_result ^= i;
    }

    int expected = 0;
    for (int i = 0; i < N; i++)
        expected ^= i;

    printf("xor = 0x%x (expected 0x%x)\n", xor_result, expected);
    return (xor_result != expected);
}
