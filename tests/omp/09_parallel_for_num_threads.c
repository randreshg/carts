/* 09: num_threads(4) */

extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));

    #pragma omp parallel for num_threads(4)
    for (int i = 0; i < N; i++) {
        A[i] = i + 5;
    }

    /* sum = sum(i+5, i=0..1023) = 523776 + 5120 = 528896 */
    long sum = 0;
    for (int i = 0; i < N; i++)
        sum += A[i];

    printf("sum = %ld (expected 528896)\n", sum);
    free(A);
    return (sum != 528896);
}
