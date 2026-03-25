/* 304: unsigned min/max reduction */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;

    unsigned int umin = (unsigned int)(-1); /* UINT_MAX */
    unsigned int umax = 0;

    #pragma omp parallel for reduction(min:umin) reduction(max:umax)
    for (int i = 0; i < N; i++) {
        unsigned int val = (unsigned int)(i * 7 + 3);
        if (val < umin) umin = val;
        if (val > umax) umax = val;
    }

    unsigned int exp_min = 3;           /* i=0: 0*7+3=3 */
    unsigned int exp_max = (unsigned int)((N - 1) * 7 + 3);

    printf("umin = %u (expected %u), umax = %u (expected %u)\n",
           umin, exp_min, umax, exp_max);
    return (umin != exp_min) || (umax != exp_max);
}
