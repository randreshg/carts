/* 74_switch_inside_for.c - parallel for with switch statement */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    #pragma omp parallel for
    for (i = 0; i < N; i++) {
        switch (i % 4) {
        case 0: A[i] = 10; break;
        case 1: A[i] = 20; break;
        case 2: A[i] = 30; break;
        default: A[i] = 40; break;
        }
    }

    long sum = 0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* 256 each: 256*(10+20+30+40) = 256*100 = 25600 */
    long expected = 25600;
    printf("sum = %ld (expected %ld)\n", sum, expected);
    free(A);
    return (sum != expected);
}
