/* 98_very_large_body.c - parallel for with 30+ arithmetic operations per iteration */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    double *A = (double *)malloc(N * sizeof(double));
    int i;

    #pragma omp parallel for
    for (i = 0; i < N; i++) {
        double x = (double)i;
        double t1 = x * x;
        double t2 = t1 + x;
        double t3 = t2 * 2.0;
        double t4 = t3 - 1.0;
        double t5 = t4 + 3.0;
        double t6 = t5 * t5;
        double t7 = t6 - t3;
        double t8 = t7 + t1;
        double t9 = t8 * 0.5;
        double t10 = t9 + t2;
        double t11 = t10 - t4;
        double t12 = t11 * 1.5;
        double t13 = t12 + t5;
        double t14 = t13 - t6 * 0.01;
        double t15 = t14 + t7 * 0.1;
        double t16 = t15 * t15;
        double t17 = t16 - t8;
        double t18 = t17 + t9 * 2.0;
        double t19 = t18 - t10;
        double t20 = t19 * 0.25;
        double t21 = t20 + t11;
        double t22 = t21 - t12 * 0.5;
        double t23 = t22 + t13;
        double t24 = t23 * 1.1;
        double t25 = t24 - t14;
        double t26 = t25 + t15 * 0.3;
        double t27 = t26 - t16 * 0.001;
        double t28 = t27 + t17 * 0.02;
        double t29 = t28 + t18 * 0.01;
        double t30 = t29 - t19 * 0.05;
        A[i] = t30 + t20;
    }

    double sum = 0.0;
    for (i = 0; i < N; i++)
        sum += A[i];

    /* Just verify it ran without crashing and produced finite results */
    int ok = (sum == sum); /* NaN check: NaN != NaN */
    printf("sum = %f, ok = %d\n", sum, ok);
    return !ok;
}
