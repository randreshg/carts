/* 228: recursive task tree -- sum of array via divide-and-conquer */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int tree_sum(int *A, int lo, int hi) {
    if (hi - lo <= 4) {
        int s = 0;
        for (int i = lo; i < hi; i++)
            s += A[i];
        return s;
    }
    int mid = (lo + hi) / 2;
    int left, right;

    #pragma omp task shared(left) firstprivate(lo, mid)
    left = tree_sum(A, lo, mid);

    #pragma omp task shared(right) firstprivate(mid, hi)
    right = tree_sum(A, mid, hi);

    #pragma omp taskwait
    return left + right;
}

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 256;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = i + 1;

    int result;
    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        result = tree_sum(A, 0, N);
    }

    long expected = (long)N * (N + 1) / 2;
    printf("sum = %d (expected %ld)\n", result, expected);
    free(A);
    return (result != expected);
}
