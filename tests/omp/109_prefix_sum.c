/* 109_prefix_sum.c - simplified parallel prefix sum (up-sweep + down-sweep) */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int *prefix = (int *)malloc(N * sizeof(int));
    int i, stride;

    for (i = 0; i < N; i++)
        A[i] = 1;

    /* Copy input to prefix array */
    for (i = 0; i < N; i++)
        prefix[i] = A[i];

    /* Up-sweep (reduce) phase */
    for (stride = 1; stride < N; stride *= 2) {
        #pragma omp parallel for
        for (i = 2 * stride - 1; i < N; i += 2 * stride)
            prefix[i] += prefix[i - stride];
    }

    /* Down-sweep phase */
    for (stride = N / 4; stride >= 1; stride /= 2) {
        #pragma omp parallel for
        for (i = 3 * stride - 1; i < N; i += 2 * stride)
            prefix[i] += prefix[i - stride];
    }

    /* With all 1s input, prefix[i] should be i+1 */
    int ok = 1;
    for (i = 0; i < N; i++) {
        if (prefix[i] != i + 1) {
            ok = 0;
            break;
        }
    }

    printf("prefix[0]=%d prefix[%d]=%d ok=%d\n",
           prefix[0], N - 1, prefix[N - 1], ok);
    free(A);
    free(prefix);
    return !ok;
}
