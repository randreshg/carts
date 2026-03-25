/* 92_single_iteration.c - parallel for with exactly one iteration */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int A = 0;

    #pragma omp parallel for
    for (int i = 0; i < 1; i++)
        A = 99;

    printf("A = %d (expected 99)\n", A);
    return (A != 99);
}
