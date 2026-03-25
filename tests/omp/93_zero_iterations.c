/* 93_zero_iterations.c - parallel for with zero iterations */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 0;
    int touched = 0;

    #pragma omp parallel for
    for (int i = 0; i < N; i++)
        touched = 1;

    printf("touched = %d (expected 0)\n", touched);
    return (touched != 0);
}
