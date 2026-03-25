/* 232: reduction with min and max on float */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    float *A = (float *)malloc(N * sizeof(float));
    int i;

    /* A[i] = sin-like pattern: i*0.1 - N*0.05 */
    for (i = 0; i < N; i++)
        A[i] = (float)i * 0.1f - (float)N * 0.05f;

    float fmin = A[0];
    float fmax = A[0];

    #pragma omp parallel for reduction(min:fmin) reduction(max:fmax)
    for (i = 0; i < N; i++) {
        if (A[i] < fmin) fmin = A[i];
        if (A[i] > fmax) fmax = A[i];
    }

    /* min = A[0] = -N*0.05, max = A[N-1] = (N-1)*0.1 - N*0.05 */
    float exp_min = -(float)N * 0.05f;
    float exp_max = (float)(N - 1) * 0.1f - (float)N * 0.05f;

    float eps = 0.001f;
    int ok = (fmin - exp_min < eps && fmin - exp_min > -eps) &&
             (fmax - exp_max < eps && fmax - exp_max > -eps);

    printf("min = %f (expected %f), max = %f (expected %f)\n",
           fmin, exp_min, fmax, exp_max);
    free(A);
    return ok ? 0 : 1;
}
