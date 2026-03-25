/* 270: parallel for where body has complex control flow -- multiple branches */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        if (i % 3 == 0)
            A[i] = i * 2;
        else if (i % 3 == 1)
            A[i] = i + 100;
        else
            A[i] = -i;
    }

    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    /* compute expected serially */
    long expected = 0;
    for (int i = 0; i < N; i++) {
        if (i % 3 == 0)
            expected += i * 2;
        else if (i % 3 == 1)
            expected += i + 100;
        else
            expected += -i;
    }

    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
