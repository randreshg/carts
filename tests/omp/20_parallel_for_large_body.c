/* 20: 20+ arithmetic ops per iteration */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        int v = i;
        v = v + 1;   /* 1 */
        v = v * 3;   /* 2 */
        v = v - 2;   /* 3 */
        v = v + 7;   /* 4 */
        v = v * 2;   /* 5 */
        v = v - 1;   /* 6 */
        v = v + 4;   /* 7 */
        v = v * 5;   /* 8 */
        v = v - 3;   /* 9 */
        v = v + 6;   /* 10 */
        v = v + 8;   /* 11 */
        v = v - 5;   /* 12 */
        v = v * 2;   /* 13 */
        v = v + 9;   /* 14 */
        v = v - 4;   /* 15 */
        v = v + 11;  /* 16 */
        v = v * 3;   /* 17 */
        v = v - 7;   /* 18 */
        v = v + 2;   /* 19 */
        v = v + 13;  /* 20 */
        v = v - 10;  /* 21 */
        A[i] = v;
    }

    /* f(i) = 180*i + 652, sum = 180*523776 + 652*1024 = 94947328 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 94947328)\n", sum);
    free(A);
    return (sum != 94947328);
}
