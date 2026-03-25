/* 266: parallel for with both firstprivate and lastprivate on same var */
extern int printf(const char *, ...);
extern int atoi(const char *);

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 1024;
    int val = 50;

    #pragma omp parallel for firstprivate(val) lastprivate(val)
    for (int i = 0; i < N; i++)
        val = 50 + i; /* each thread starts with 50, overwrites with 50+i */

    /* lastprivate: val = value from last iteration = 50 + N - 1 */
    int expected = 50 + N - 1;
    printf("val = %d (expected %d)\n", val, expected);
    return (val != expected);
}
