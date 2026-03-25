/* 246: critical section -- serialized updates */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 256;
    long sum = 0;
    int count = 0;

    #pragma omp parallel for num_threads(4)
    for (int i = 0; i < N; i++) {
        long local = (long)(i + 1) * (i + 1);
        #pragma omp critical
        {
            sum += local;
            count++;
        }
    }

    /* sum of (i+1)^2, i=0..N-1 = N*(N+1)*(2N+1)/6 */
    long expected = (long)N * (N + 1) * (2 * N + 1) / 6;
    printf("sum = %ld (expected %ld), count = %d\n", sum, expected, count);
    return (sum != expected) || (count != N);
}
