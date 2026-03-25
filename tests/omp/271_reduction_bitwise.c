/* 271: reduction with bitwise OR and AND */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 256;
    int or_result = 0;
    int and_result = ~0; /* all bits set */

    #pragma omp parallel for reduction(|:or_result) reduction(&:and_result)
    for (int i = 0; i < N; i++) {
        or_result |= i;
        and_result &= (i | 0xFF00); /* keep upper bits, vary lower */
    }

    /* OR: 0|1|2|...|255 = 0xFF (all 8 low bits set for N>=256) */
    int exp_or = 0;
    int exp_and = ~0;
    for (int i = 0; i < N; i++) {
        exp_or |= i;
        exp_and &= (i | 0xFF00);
    }

    printf("or = 0x%x (expected 0x%x), and = 0x%x (expected 0x%x)\n",
           or_result, exp_or, and_result, exp_and);
    return (or_result != exp_or) || (and_result != exp_and);
}
