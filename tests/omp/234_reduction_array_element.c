/* 234: reduction on array element -- different array slots reduced separately */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int bins[4];
    int i;

    bins[0] = 0;
    bins[1] = 0;
    bins[2] = 0;
    bins[3] = 0;

    /* reduce into 4 bins based on i%4 */
    #pragma omp parallel for reduction(+:bins)
    for (i = 0; i < N; i++)
        bins[i % 4] += i;

    /* bin k gets sum of all i where i%4==k */
    /* for N=1024: bin k = sum of (k, k+4, k+8, ..., k+1020) */
    /* = 256 terms: 256*k + 4*(0+1+...+255) = 256k + 4*255*256/2 = 256k + 130560 */
    long expected = 0;
    for (i = 0; i < 4; i++)
        expected += bins[i];

    long total_expected = (long)N * (N - 1) / 2;
    printf("bins=[%d,%d,%d,%d] total=%ld (expected %ld)\n",
           bins[0], bins[1], bins[2], bins[3], expected, total_expected);
    return (expected != total_expected);
}
