/* 58_reduction_multiple_vars.c - reduction(+:a,b,c) three variables */
extern int printf(const char *, ...);
extern int atoi(const char *);


int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int i;
    int a = 0, b = 0, c = 0;

    #pragma omp parallel for reduction(+:a,b,c)
    for (i = 0; i < N; i++) {
        a += 1;
        b += i;
        c += i * i;
    }

    int ea = N;
    int eb = N * (N - 1) / 2;
    long ec_l = 0;
    for (i = 0; i < N; i++)
        ec_l += (long)i * i;
    int ec = (int)ec_l;

    printf("a=%d (exp %d) b=%d (exp %d) c=%d (exp %d)\n", a, ea, b, eb, c, ec);
    return (a == ea && b == eb && c == ec) ? 0 : 1;
}
