/* 255: parallel for with non-unit step=7 -- non-power-of-2 stride */
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
    for (i = 0; i < N; i += 7)
        A[i] = i + 1;

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* indices: 0,7,14,..., count = ceil(N/7) */
    /* values: 1,8,15,..., = 7*k+1 for k=0..count-1 */
    int count = (N + 6) / 7;
    /* sum = count + 7*(0+1+...+(count-1)) = count + 7*count*(count-1)/2 */
    long expected = (long)count + 7L * count * (count - 1) / 2;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
