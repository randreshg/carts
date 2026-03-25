/* 07: schedule(dynamic) */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < N; i++) {
        A[i] = i * i;
    }

    /* sum = sum(i*i, i=0..1023) = 1024*1023*2047/6 = 357389824 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 357389824)\n", sum);
    free(A);
    return (sum != 357389824);
}
