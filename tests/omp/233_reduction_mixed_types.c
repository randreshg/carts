/* 233: reduction with multiple variables of different types */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int isum = 0;
    long lsum = 0;
    double dsum = 0.0;

    #pragma omp parallel for reduction(+:isum,lsum,dsum)
    for (int i = 0; i < N; i++) {
        isum += (i + 1);
        lsum += (long)(i + 1) * (i + 1);
        dsum += 1.0 / (i + 1);
    }

    int exp_isum = N * (N + 1) / 2;
    long exp_lsum = (long)N * (N + 1) * (2 * N + 1) / 6;

    /* harmonic series has no simple closed form, compute serially */
    double exp_dsum = 0.0;
    for (int i = 0; i < N; i++)
        exp_dsum += 1.0 / (i + 1);

    int ok = (isum == exp_isum) && (lsum == exp_lsum);
    double ddiff = dsum - exp_dsum;
    if (ddiff < 0) ddiff = -ddiff;
    ok = ok && (ddiff < 1e-6);

    printf("isum=%d lsum=%ld dsum=%f (expected %d/%ld/%f)\n",
           isum, lsum, dsum, exp_isum, exp_lsum, exp_dsum);
    return ok ? 0 : 1;
}
