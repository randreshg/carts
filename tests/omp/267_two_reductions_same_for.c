/* 267: two different reduction operators in the same parallel for */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = i + 1;

    long sum = 0;
    int max_val = 0;

    #pragma omp parallel for reduction(+:sum) reduction(max:max_val)
    for (i = 0; i < N; i++) {
        sum += A[i];
        if (A[i] > max_val)
            max_val = A[i];
    }

    long exp_sum = (long)N * (N + 1) / 2;
    int exp_max = N;
    printf("sum = %ld (expected %ld), max = %d (expected %d)\n",
           sum, exp_sum, max_val, exp_max);
    free(A);
    return (sum != exp_sum) || (max_val != exp_max);
}
