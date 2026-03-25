/* 75_nested_if.c - parallel for with deeply nested conditionals */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = 0;

    #pragma omp parallel for
    for (i = 0; i < N; i++) {
        if (i % 2 == 0) {
            if (i % 3 == 0) {
                if (i % 5 == 0) {
                    A[i] = i;
                }
            }
        }
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* Writes when i divisible by 30: 0,30,60,...,1020 */
    /* count = 34+1=35 values (0..1020 step 30) => actually 0,30,...,1020 = 35 values */
    /* sum = 30*(0+1+2+...+34) = 30*34*35/2 = 17850 */
    long expected = 17850;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
