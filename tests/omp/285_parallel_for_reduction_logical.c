/* 285: reduction with logical OR and AND */
extern void *malloc(unsigned long);
extern void free(void *);
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int *A = (int *)malloc(N * sizeof(int));
    int i;

    for (i = 0; i < N; i++)
        A[i] = i * 2; /* all even */

    int has_odd = 0;
    int all_positive = 1;

    #pragma omp parallel for reduction(||:has_odd) reduction(&&:all_positive)
    for (i = 0; i < N; i++) {
        has_odd = has_odd || (A[i] % 2 != 0);
        all_positive = all_positive && (A[i] >= 0);
    }

    /* all even -> has_odd = 0, all non-negative -> all_positive = 1 */
    printf("has_odd = %d (expected 0), all_positive = %d (expected 1)\n",
           has_odd, all_positive);
    free(A);
    return (has_odd != 0) || (all_positive != 1);
}
