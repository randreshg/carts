/* 245: atomic operations -- concurrent updates to shared variable */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int counter = 0;
    long sum = 0;

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        #pragma omp atomic
        counter++;

        #pragma omp atomic
        sum += (long)(i + 1);
    }

    long expected_sum = (long)N * (N + 1) / 2;
    printf("counter = %d (expected %d), sum = %ld (expected %ld)\n",
           counter, N, sum, expected_sum);
    return (counter != N) || (sum != expected_sum);
}
